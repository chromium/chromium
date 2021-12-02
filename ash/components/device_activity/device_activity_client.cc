// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "ash/components/device_activity/fresnel_pref_names.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "crypto/hmac.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash {
namespace device_activity {
namespace {

// Amount of time to wait before retriggering repeating timer.
// Currently we define it as 5 hours to align our protocol with Omahas
// device active reporting.
constexpr base::TimeDelta kTimeToRepeat = base::Hours(5);

const size_t kHmacDigestLength = 32;

// TODO(https://crbug.com/1262177): currently the PSM use cases are not synced
// with google3. Update to retrieve from synced RlweUseCase in file:
// third_party/private_membership/src/private_membership_rlwe.proto.
constexpr psm_rlwe::RlweUseCase kDailyPsmUseCase =
    psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY;

// Generate the window identifier for the kCrosDaily use case.
// For example, the daily use case should generate a window identifier
// formatted: yyyyMMdd.
// TODO(https://crbug.com/1262187): This window identifier will need to support
// more use cases in the future. Currently it only supports the kCrosDaily use
// case.
std::string GenerateWindowIdentifier(base::Time ts) {
  return base::UTF16ToUTF8(base::TimeFormatWithPattern(ts, "yyyyMMdd"));
}

// Generate the PSM identifier, used to identify a fixed
// window of time for device active counting. Privacy compliance is guaranteed
// by retrieving the |derived_stable_secret| from chromeos, and
// performing an additional HMAC-SHA256 hash on generated plaintext string.
absl::optional<std::string> GeneratePsmIdentifier(
    const std::string& derived_stable_secret,
    const std::string& psm_use_case,
    const std::string& window_id) {
  if (derived_stable_secret.empty() || psm_use_case.empty() ||
      window_id.empty())
    return absl::nullopt;

  std::string unhashed_psm_id =
      base::JoinString({psm_use_case, window_id}, "|");

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  unsigned char digest[kHmacDigestLength];
  bool result = hmac.Init(derived_stable_secret) &&
                hmac.Sign(unhashed_psm_id, digest, kHmacDigestLength);
  if (result) {
    return std::string(reinterpret_cast<const char*>(digest),
                       kHmacDigestLength);
  }

  // Failed HMAC-SHA256 hash on PSM id.
  return absl::nullopt;
}

// Determines if |prev_ping_ts| occurred in a different daily active window then
// |new_ping_ts| for a given device. Performing this check helps reduce QPS to
// the |CheckingMembership| network requests.
// TODO(https://crbug.com/1262187): This function will need to get modified to
// support kCrosMonthly and kCrosAllTime use cases.
bool IsDailyDeviceActivePingRequired(base::Time prev_ping_ts,
                                     base::Time new_ping_ts) {
  std::string prev_ping_ts_period = GenerateWindowIdentifier(prev_ping_ts);
  std::string new_ping_ts_period = GenerateWindowIdentifier(new_ping_ts);

  return prev_ping_ts < new_ping_ts &&
         prev_ping_ts_period != new_ping_ts_period;
}

}  // namespace

DeviceActivityClient::DeviceActivityClient(NetworkStateHandler* handler,
                                           PrefService* local_state)
    : report_timer_(ConstructReportTimer()),
      network_state_handler_(handler),
      local_state_(local_state) {
  DCHECK(network_state_handler_);
  DCHECK(local_state_);

  report_timer_->Start(FROM_HERE, kTimeToRepeat, this,
                       &DeviceActivityClient::TransitionOutOfIdle);

  network_state_handler_->AddObserver(this, FROM_HERE);
  DefaultNetworkChanged(network_state_handler_->DefaultNetwork());
}

DeviceActivityClient::~DeviceActivityClient() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
}

std::unique_ptr<base::RepeatingTimer>
DeviceActivityClient::ConstructReportTimer() {
  return std::make_unique<base::RepeatingTimer>();
}

base::RepeatingTimer* DeviceActivityClient::GetReportTimer() {
  return report_timer_.get();
}

// Method gets called when the state of the default (primary)
// network OR properties of the default network changes.
void DeviceActivityClient::DefaultNetworkChanged(const NetworkState* network) {
  bool was_connected = network_connected_;
  network_connected_ = network && network->IsOnline();

  if (network_connected_ == was_connected)
    return;
  if (network_connected_)
    OnNetworkOnline();
}

DeviceActivityClient::State DeviceActivityClient::GetState() const {
  return state_;
}

void DeviceActivityClient::OnNetworkOnline() {
  TransitionOutOfIdle();
}

// TODO(https://crbug.com/1262189): Add callback to report actives only after
// synchronizing the system clock.
void DeviceActivityClient::TransitionOutOfIdle() {
  if (!network_connected_ || state_ != State::kIdle)
    return;

  // The network is connected and the client |state_| is kIdle.
  last_transition_out_of_idle_time__ = base::Time::Now();

  // Begin phase one of checking membership if the device has not pinged yet
  // within the given use case window.
  // TODO(https://crbug.com/1262187): Remove hardcoded use case when adding
  // support for additional use cases (i.e MONTHLY, ALL_TIME, etc.).
  if (IsDailyDeviceActivePingRequired(
          local_state_->GetTime(
              prefs::kDeviceActiveLastKnownDailyPingTimestamp),
          last_transition_out_of_idle_time__)) {
    current_day_window_id_ =
        GenerateWindowIdentifier(last_transition_out_of_idle_time__);
    current_day_psm_id_ =
        GeneratePsmIdentifier(derived_stable_device_secret_,
                              psm_rlwe::RlweUseCase_Name(kDailyPsmUseCase),
                              current_day_window_id_.value());

    // Check if the PSM id is generated.
    if (!current_day_psm_id_.has_value())
      return;

    TransitionToCheckMembershipOprf();
  }
}

void DeviceActivityClient::TransitionToHealthCheck() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kHealthCheck;

  // TODO(https://crbug.com/1262201): Add Health Check network request with
  // callback to OnHealthCheckDone with response.
}

void DeviceActivityClient::OnHealthCheckDone() {
  TransitionToIdle();
}

void DeviceActivityClient::TransitionToCheckMembershipOprf() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kCheckingMembershipOprf;

  // TODO(https://crbug.com/1262201): Add OPRF network request with callback to
  // OnCheckMembershipOprfDone with response.
}

void DeviceActivityClient::OnCheckMembershipOprfDone() {
  TransitionToCheckMembershipQuery();
}

void DeviceActivityClient::TransitionToCheckMembershipQuery() {
  DCHECK_EQ(state_, State::kCheckingMembershipOprf);
  state_ = State::kCheckingMembershipQuery;

  // TODO(https://crbug.com/1262201): Add Query network request with callback to
  // OnCheckMembershipQueryDone with response.
}

void DeviceActivityClient::OnCheckMembershipQueryDone(bool needs_check_in) {
  if (needs_check_in) {
    TransitionToCheckIn();
  } else {
    TransitionToIdle();
  }
}

void DeviceActivityClient::TransitionToCheckIn() {
  DCHECK_EQ(state_, State::kCheckingMembershipQuery);
  state_ = State::kCheckingIn;

  // TODO(https://crbug.com/1262201): Add import network request with callback
  // to OnCheckInDone with response.
}

void DeviceActivityClient::OnCheckInDone() {
  TransitionToIdle();
}

void DeviceActivityClient::TransitionToIdle() {
  state_ = State::kIdle;

  current_day_window_id_ = absl::nullopt;
  current_day_psm_id_ = absl::nullopt;
}

}  // namespace device_activity
}  // namespace ash
