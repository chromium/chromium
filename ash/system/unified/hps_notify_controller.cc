// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/hps/hps_configuration.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

HpsNotifyController::HpsNotifyController() {
  // When the controller is initialized, we are never in an active user session
  // and we never have any user preferences active. Hence, our default state
  // values are correct.

  // Session controller is instantiated before us in the shell.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  // Wait for the service to be available before subscribing to its events. If
  // we directly subscribe here, we will attempt to configure the DBus service
  // twice (once via this callback and once via |OnRestart|) if it's slow to
  // start. Configuring HPS notify without first disabling it is an error.
  chromeos::HpsDBusClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&HpsNotifyController::StartHpsObservation,
                     weak_ptr_factory_.GetWeakPtr()));
}

HpsNotifyController::~HpsNotifyController() {
  // This is a no-op if the service isn't available or isn't enabled.
  // TODO(crbug.com/1241704): only disable if the service is enabled.
  chromeos::HpsDBusClient::Get()->DisableHpsNotify();
}

// static
void HpsNotifyController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kSnoopingProtectionEnabled,
      /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled,
      /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void HpsNotifyController::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  const bool session_active =
      session_state == session_manager::SessionState::ACTIVE;
  ReconfigureHps(hps_available_, session_active, pref_enabled_);
  UpdateIconVisibility(session_active, hps_state_ && session_active,
                       pref_enabled_);
}

void HpsNotifyController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  DCHECK(pref_service);

  const bool pref_enabled =
      pref_service->GetBoolean(prefs::kSnoopingProtectionEnabled);
  ReconfigureHps(hps_available_, session_active_, pref_enabled);
  UpdateIconVisibility(session_active_, hps_state_ && pref_enabled,
                       pref_enabled);

  // Re-subscribe to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionEnabled,
      base::BindRepeating(&HpsNotifyController::UpdatePrefState,
                          weak_ptr_factory_.GetWeakPtr()));
}

void HpsNotifyController::OnHpsNotifyChanged(hps::HpsResult hps_state) {
  UpdateIconVisibility(session_active_,
                       /*hps_state=*/hps_state == hps::HpsResult::POSITIVE,
                       pref_enabled_);
}

void HpsNotifyController::OnRestart() {
  DCHECK(!hps_state_);

  ReconfigureHps(/*hps_available_=*/true, session_active_, pref_enabled_);
}

void HpsNotifyController::OnShutdown() {
  ReconfigureHps(/*hps_available=*/false, session_active_, pref_enabled_);
  UpdateIconVisibility(session_active_, /*hps_state=*/false, pref_enabled_);

  // We will be notified of the service starting back up again via our ongoing
  // observation of the DBus client.
}

void HpsNotifyController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HpsNotifyController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool HpsNotifyController::IsIconVisible() const {
  return session_active_ && hps_state_ && pref_enabled_;
}

void HpsNotifyController::UpdateIconVisibility(bool session_active,
                                               bool hps_state,
                                               bool pref_enabled) {
  // We should only receive a "present" signal if the service is available and
  // configured.
  DCHECK((hps_available_ && hps_configured_) || !hps_state);

  const bool old_visibility = IsIconVisible();

  session_active_ = session_active;
  hps_state_ = hps_state;
  pref_enabled_ = pref_enabled;

  const bool new_visibility = IsIconVisible();

  if (old_visibility == new_visibility)
    return;

  for (auto& observer : observers_)
    observer.ShouldUpdateVisibility(new_visibility);
}

void HpsNotifyController::ReconfigureHps(bool hps_available,
                                         bool session_active,
                                         bool pref_enabled) {
  // Can't configure or de-configure the service if it's unavailable.
  if (!hps_available) {
    hps_available_ = false;
    hps_configured_ = false;
    return;
  }
  hps_available_ = true;

  // We have correctly cached that the service is available; now handle
  // configuring its signal.
  const bool want_configured = pref_enabled && session_active;
  if (hps_configured_ == want_configured)
    return;

  if (want_configured) {
    // Configure the snooping started/stopped signals that the service will
    // emit.
    const absl::optional<hps::FeatureConfig> config =
        GetEnableHpsNotifyConfig();
    if (!config.has_value()) {
      LOG(ERROR) << "Couldn't parse notify configuration";
      return;
    }

    chromeos::HpsDBusClient::Get()->EnableHpsNotify(*config);

    // Populate our initial HPS state for consistency with the service.
    chromeos::HpsDBusClient::Get()->GetResultHpsNotify(base::BindOnce(
        &HpsNotifyController::UpdateHpsState, weak_ptr_factory_.GetWeakPtr()));
    hps_configured_ = true;

    return;
  }

  // No longer need signals to be emitted.
  chromeos::HpsDBusClient::Get()->DisableHpsNotify();
  hps_configured_ = false;
}

void HpsNotifyController::StartHpsObservation(bool service_is_available) {
  hps_available_ = service_is_available;
  hps_configured_ = false;

  if (!service_is_available) {
    LOG(ERROR) << "Could not make initial connection to HPS service";
    return;
  }

  // Special case: at this point, the service could have been left in an enabled
  // state by a previous session that crashed (and hence didn't clean up
  // properly). Disable it here, which is a no-op if it is already disabled.
  chromeos::HpsDBusClient::Get()->DisableHpsNotify();

  // Start listening for state updates and restarts/shutdowns.
  hps_dbus_observation_.Observe(chromeos::HpsDBusClient::Get());

  // Configure the service and poll its initial value if necessary.
  ReconfigureHps(/*hps_available_=*/true, session_active_, pref_enabled_);
}

void HpsNotifyController::UpdateHpsState(
    absl::optional<hps::HpsResult> response) {
  LOG_IF(WARNING, !response.has_value())
      << "Polling the presence daemon failed";

  UpdateIconVisibility(
      session_active_,
      response.value_or(hps::HpsResult::NEGATIVE) == hps::HpsResult::POSITIVE,
      pref_enabled_);
}

void HpsNotifyController::UpdatePrefState() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());

  const bool pref_enabled = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kSnoopingProtectionEnabled);
  ReconfigureHps(hps_available_, session_active_, pref_enabled);
  UpdateIconVisibility(session_active_, hps_state_ && pref_enabled,
                       pref_enabled);
}

}  // namespace ash
