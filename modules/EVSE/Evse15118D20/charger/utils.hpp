// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <generated/types/iso15118.hpp>
#include <generated/types/iso15118_vas.hpp>

#include <iso15118/d20/ev_information.hpp>
#include <iso15118/message/ac_charge_loop.hpp>
#include <iso15118/message/ac_charge_parameter_discovery.hpp>
#include <iso15118/message/dc_charge_loop.hpp>
#include <iso15118/message/service_detail.hpp>
#include <iso15118/message/type.hpp>

static constexpr auto NUMBER_OF_SETUP_STEPS = 5;

namespace module::charger {

namespace dt = iso15118::message_20::datatypes;

enum class SetupStep : std::uint8_t {
    SETUP,
    ENERGY_SERVICE,
    AUTH_SETUP,
    MAX_LIMITS,
    MIN_LIMITS,
};

template <typename T> constexpr auto to_underlying_value(T t) {
    return static_cast<std::underlying_type_t<T>>(t);
}

static_assert(NUMBER_OF_SETUP_STEPS == to_underlying_value(SetupStep::MIN_LIMITS) + 1,
              "NUMBER_OF_SETUP_STEPS should be in sync with the SetupStep enum definition");

constexpr types::iso15118::V2gMessageId convert_v2g_message_type(iso15118::message_20::Type type) {

    using Type = iso15118::message_20::Type;
    using Id = types::iso15118::V2gMessageId;

    switch (type) {
    case Type::None:
        return Id::UnknownMessage;
    case Type::SupportedAppProtocolReq:
        return Id::SupportedAppProtocolReq;
    case Type::SupportedAppProtocolRes:
        return Id::SupportedAppProtocolRes;
    case Type::SessionSetupReq:
        return Id::SessionSetupReq;
    case Type::SessionSetupRes:
        return Id::SessionSetupRes;
    case Type::AuthorizationSetupReq:
        return Id::AuthorizationSetupReq;
    case Type::AuthorizationSetupRes:
        return Id::AuthorizationSetupRes;
    case Type::AuthorizationReq:
        return Id::AuthorizationReq;
    case Type::AuthorizationRes:
        return Id::AuthorizationRes;
    case Type::ServiceDiscoveryReq:
        return Id::ServiceDiscoveryReq;
    case Type::ServiceDiscoveryRes:
        return Id::ServiceDiscoveryRes;
    case Type::ServiceDetailReq:
        return Id::ServiceDetailReq;
    case Type::ServiceDetailRes:
        return Id::ServiceDetailRes;
    case Type::ServiceSelectionReq:
        return Id::ServiceSelectionReq;
    case Type::ServiceSelectionRes:
        return Id::ServiceSelectionRes;
    case Type::DC_ChargeParameterDiscoveryReq:
        return Id::DcChargeParameterDiscoveryReq;
    case Type::DC_ChargeParameterDiscoveryRes:
        return Id::DcChargeParameterDiscoveryRes;
    case Type::ScheduleExchangeReq:
        return Id::ScheduleExchangeReq;
    case Type::ScheduleExchangeRes:
        return Id::ScheduleExchangeRes;
    case Type::DC_CableCheckReq:
        return Id::DcCableCheckReq;
    case Type::DC_CableCheckRes:
        return Id::DcCableCheckRes;
    case Type::DC_PreChargeReq:
        return Id::DcPreChargeReq;
    case Type::DC_PreChargeRes:
        return Id::DcPreChargeRes;
    case Type::PowerDeliveryReq:
        return Id::PowerDeliveryReq;
    case Type::PowerDeliveryRes:
        return Id::PowerDeliveryRes;
    case Type::DC_ChargeLoopReq:
        return Id::DcChargeLoopReq;
    case Type::DC_ChargeLoopRes:
        return Id::DcChargeLoopRes;
    case Type::DC_WeldingDetectionReq:
        return Id::DcWeldingDetectionReq;
    case Type::DC_WeldingDetectionRes:
        return Id::DcWeldingDetectionRes;
    case Type::SessionStopReq:
        return Id::SessionStopReq;
    case Type::SessionStopRes:
        return Id::SessionStopRes;
    case Type::AC_ChargeParameterDiscoveryReq:
        return Id::AcChargeParameterDiscoveryReq;
    case Type::AC_ChargeParameterDiscoveryRes:
        return Id::AcChargeParameterDiscoveryRes;
    case Type::AC_ChargeLoopReq:
        return Id::AcChargeLoopReq;
    case Type::AC_ChargeLoopRes:
        return Id::AcChargeLoopRes;
    }

    return Id::UnknownMessage;
}

// The message type enums of all three protocols name these messages alike.
template <typename Type> constexpr bool is_session_stop_message(Type type) {
    return type == Type::SessionStopReq or type == Type::SessionStopRes;
}

inline bool is_session_stop_message(const iso15118::V2gMessageType& type) {
    return std::visit([](auto&& concrete) { return is_session_stop_message(concrete); }, type);
}

template <typename Type> constexpr bool is_power_delivery_message(Type type) {
    return type == Type::PowerDeliveryReq or type == Type::PowerDeliveryRes;
}

inline bool is_power_delivery_message(const iso15118::V2gMessageType& type) {
    return std::visit([](auto&& concrete) { return is_power_delivery_message(concrete); }, type);
}

// The messages of a session as hlc_failed_reason_at_teardown reads them. A request the SECC rejects as out of
// sequence is not a phase the session reached, so it is left out. The sequence error ends the session, so
// nothing after it is recorded either. A rejected SessionStopReq stays: it is classified as a SessionStop.
class HlcMessageHistory {
public:
    void record(const iso15118::V2gMessageType& message) {
        if (m_ended_by_sequence_error) {
            return;
        }
        m_before_last_message = m_current;
        m_current.last_message = message;
        if (not is_session_stop_message(message)) {
            m_current.last_message_before_session_stop = message;
        }
    }

    // The message recorded last is a request the SECC answers with a sequence error.
    void reject_last_request() {
        if (m_current.last_message.has_value() and is_session_stop_message(*m_current.last_message)) {
            return;
        }
        m_current = m_before_last_message;
        m_ended_by_sequence_error = true;
    }

    void reset() {
        *this = HlcMessageHistory{};
    }

    const std::optional<iso15118::V2gMessageType>& last_message() const {
        return m_current.last_message;
    }

    const std::optional<iso15118::V2gMessageType>& last_message_before_session_stop() const {
        return m_current.last_message_before_session_stop;
    }

private:
    struct Messages {
        std::optional<iso15118::V2gMessageType> last_message;
        std::optional<iso15118::V2gMessageType> last_message_before_session_stop;
    };
    Messages m_current;
    Messages m_before_last_message;
    bool m_ended_by_sequence_error{false};
};

// Reason to publish as hlc_session_failed when a data link ends, from the last message handled and the last
// one outside the SessionStop exchange. A connection that closed without SessionStopReq is classified by the
// message in flight. A SessionStopReq is reported when it follows protocol negotiation, authorization, cable
// check or pre-charge, e.g. when the V2G_EVCC_Ongoing_Timer expired while AuthorizationReq was still answered
// with Ongoing. Who caused the stop is not considered: an EV that follows the EVSE's shutdown status during
// cable check or pre-charge is reported alike. Elsewhere it is not reported. During service and charge
// parameter negotiation the preceding message alone cannot tell an EV that gave up from a regular end, since
// the charge parameter responses carry the EVSE's own stop notifications. After any PowerDelivery, the charge
// loop or welding detection it is the regular end.
inline std::optional<types::evse_manager::HlcSessionFailedReasonEnum>
hlc_failed_reason_at_teardown(const std::optional<iso15118::V2gMessageType>& last_message,
                              const std::optional<iso15118::V2gMessageType>& last_message_before_session_stop) {
    using Reason = types::evse_manager::HlcSessionFailedReasonEnum;
    if (not last_message.has_value()) {
        return std::nullopt;
    }
    if (not is_session_stop_message(*last_message)) {
        return map_v2g_message_to_hlc_failed_reason(*last_message);
    }
    if (not last_message_before_session_stop.has_value() or
        is_power_delivery_message(*last_message_before_session_stop)) {
        return std::nullopt;
    }
    const auto reason = map_v2g_message_to_hlc_failed_reason(*last_message_before_session_stop);
    if (reason == Reason::ProtocolNegotiationFailed or reason == Reason::AuthorizationFailed or
        reason == Reason::EnergyTransferSetupFailed) {
        return reason;
    }
    return std::nullopt;
}

std::optional<float> convert_from_optional(const std::optional<dt::RationalNumber>& in);
std::optional<dt::RationalNumber> convert_from_optional(const std::optional<float>& in);
std::optional<float> convert_from_optional(const std::optional<uint32_t>& in);

types::iso15118::AppProtocol convert_app_protocol(const iso15118::message_20::SupportedAppProtocol& app_protocol);
types::iso15118::EvInformation convert_ev_info(const iso15118::d20::EVInformation& ev_info);

types::iso15118::DcChargeDynamicModeValues convert_dynamic_values(const dt::Dynamic_DC_CLReqControlMode& in);
types::iso15118::DcChargeDynamicModeValues convert_dynamic_values(const dt::BPT_Dynamic_DC_CLReqControlMode& in);

template <typename EVSE, typename EV>
void fill_v2x_charging_parameters(types::iso15118::V2XChargingParameters& out_params, const EVSE& evse_limits,
                                  const EV& ev_limits);
template <typename In>
void fill_v2x_charging_parameters(types::iso15118::V2XChargingParameters& out_params, const In& data);

std::vector<dt::ParameterSet>
convert_parameter_set_list(const std::vector<types::iso15118_vas::ParameterSet>& parameter_set_list);

types::iso15118::AcEvPowerLimits fill_ac_ev_power_limits(const dt::AC_CPDReqEnergyTransferMode& mode);
types::iso15118::AcEvPowerLimits fill_ac_ev_power_limits(const dt::BPT_AC_CPDReqEnergyTransferMode& mode);
types::iso15118::AcEvPowerLimits fill_ac_ev_power_limits(const dt::Dynamic_AC_CLReqControlMode& mode);
types::iso15118::AcEvPowerLimits fill_ac_ev_power_limits(const dt::BPT_Dynamic_AC_CLReqControlMode& mode);
types::iso15118::AcEvPowerLimits fill_ac_ev_power_limits(const dt::Scheduled_AC_CLReqControlMode& mode);
types::iso15118::AcEvPowerLimits fill_ac_ev_power_limits(const dt::BPT_Scheduled_AC_CLReqControlMode& mode);

types::iso15118::AcEvPresentPowerValues fill_ac_ev_present_power_values(const dt::Dynamic_AC_CLReqControlMode& mode);
types::iso15118::AcEvPresentPowerValues fill_ac_ev_present_power_values(const dt::Scheduled_AC_CLReqControlMode& mode);

} // namespace module::charger
