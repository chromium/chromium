// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_controller.h"

#include "ash/components/device_activity/device_activity_client.h"
#include "ash/components/device_activity/fresnel_pref_names.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "google_apis/google_api_keys.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace psm_rlwe = private_membership::rlwe;

namespace ash {
namespace device_activity {

namespace {
DeviceActivityController* g_ash_device_activity_controller = nullptr;

// Production edge server for reporting device actives.
// TODO(https://crbug.com/1267432): Enable passing base url as a runtime flag.
const char kFresnelBaseUrl[] =
    "https://autopush-crosfresnel-pa.sandbox.googleapis.com";

class PsmDelegateImpl : public PsmDelegate {
 public:
  PsmDelegateImpl() = default;
  PsmDelegateImpl(const PsmDelegateImpl&) = delete;
  PsmDelegateImpl& operator=(const PsmDelegateImpl&) = delete;
  ~PsmDelegateImpl() override = default;

  // PsmDelegate:
  rlwe::StatusOr<std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(
      psm_rlwe::RlweUseCase use_case,
      const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids) override {
    return psm_rlwe::PrivateMembershipRlweClient::Create(use_case,
                                                         plaintext_ids);
  }
};

}  // namespace

DeviceActivityController* DeviceActivityController::Get() {
  return g_ash_device_activity_controller;
}

// static
void DeviceActivityController::RegisterPrefs(PrefRegistrySimple* registry) {
  const base::Time unix_epoch = base::Time::UnixEpoch();
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownDailyPingTimestamp,
                             unix_epoch);
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownMonthlyPingTimestamp,
                             unix_epoch);
  registry->RegisterTimePref(prefs::kDeviceActiveLastKnownAllTimePingTimestamp,
                             unix_epoch);
}

DeviceActivityController::DeviceActivityController() {
  DCHECK(!g_ash_device_activity_controller);
  g_ash_device_activity_controller = this;
}

DeviceActivityController::~DeviceActivityController() {
  DCHECK_EQ(this, g_ash_device_activity_controller);
  Stop(Trigger::kNetwork);
  g_ash_device_activity_controller = nullptr;
}

void DeviceActivityController::Start(
    Trigger trigger,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Wrap with callback from |psm_device_active_secret_| retrieval using
  // |SessionManagerClient| DBus.
  chromeos::SessionManagerClient::Get()->GetPsmDeviceActiveSecret(
      base::BindOnce(&device_activity::DeviceActivityController::
                         OnPsmDeviceActiveSecretFetched,
                     weak_factory_.GetWeakPtr(), trigger, local_state,
                     url_loader_factory));
}

void DeviceActivityController::OnPsmDeviceActiveSecretFetched(
    Trigger trigger,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& psm_device_active_secret) {
  if (trigger == Trigger::kNetwork) {
    da_client_network_ = std::make_unique<DeviceActivityClient>(
        chromeos::NetworkHandler::Get()->network_state_handler(), local_state,
        url_loader_factory, std::make_unique<PsmDelegateImpl>(),
        std::make_unique<base::RepeatingTimer>(), kFresnelBaseUrl,
        google_apis::GetFresnelAPIKey(), psm_device_active_secret);
  }
}

void DeviceActivityController::Stop(Trigger trigger) {
  if (trigger == Trigger::kNetwork && da_client_network_) {
    da_client_network_.reset();
  }
}

}  // namespace device_activity
}  // namespace ash
