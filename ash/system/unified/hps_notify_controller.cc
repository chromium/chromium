// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/hps_notify_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/hps/hps_notify_notification_blocker.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/components/hps/hps_configuration.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/message_center.h"

namespace ash {

HpsNotifyController::HpsNotifyController()
    : notification_blocker_(std::make_unique<HpsNotifyNotificationBlocker>(
          message_center::MessageCenter::Get(),
          this)),
      pos_window_(hps::GetSnoopingProtectionPositiveWindow()) {
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
  //
  // Might not exist in unit tests.
  if (chromeos::HpsDBusClient::Get()) {
    chromeos::HpsDBusClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&HpsNotifyController::StartHpsObservation,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Orientation controller is instantiated before us in the shell.
  HpsOrientationController* orientation_controller =
      Shell::Get()->hps_orientation_controller();
  state_.orientation_suitable = orientation_controller->IsOrientationSuitable();
  orientation_observation_.Observe(orientation_controller);
}

HpsNotifyController::~HpsNotifyController() {
  // This is a no-op if the service isn't available or isn't enabled.
  // TODO(crbug.com/1241704): only disable if the service is enabled.
  //
  // Might not exist in unit tests.
  if (chromeos::HpsDBusClient::Get())
    chromeos::HpsDBusClient::Get()->DisableHpsNotify();

  for (auto& observer : observers_)
    observer.OnHpsNotifyControllerDestroyed();
}

// static
void HpsNotifyController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kSnoopingProtectionEnabled,
      /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled,
      /*default_value=*/true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void HpsNotifyController::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  const bool session_active =
      session_state == session_manager::SessionState::ACTIVE;

  State new_state = state_;
  new_state.session_active = session_active;

  ReconfigureHps(&new_state);
  UpdateSnooperStatus(new_state);
}

void HpsNotifyController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  DCHECK(pref_service);
  const bool pref_enabled =
      pref_service->GetBoolean(prefs::kSnoopingProtectionEnabled);

  State new_state = state_;
  new_state.pref_enabled = pref_enabled;

  ReconfigureHps(&new_state);
  UpdateSnooperStatus(new_state);

  // Re-subscribe to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionEnabled,
      base::BindRepeating(&HpsNotifyController::UpdatePrefState,
                          weak_ptr_factory_.GetWeakPtr()));
}

void HpsNotifyController::OnOrientationChanged(bool suitable_for_hps) {
  State new_state = state_;
  new_state.orientation_suitable = suitable_for_hps;

  ReconfigureHps(&new_state);
  UpdateSnooperStatus(new_state);
}

void HpsNotifyController::OnHpsSenseChanged(hps::HpsResult) {}

void HpsNotifyController::OnHpsNotifyChanged(hps::HpsResult hps_state) {
  const bool present = hps_state == hps::HpsResult::POSITIVE;

  State new_state = state_;
  new_state.hps_state = present;

  // Prevent snooping status from becoming negative within a window of time.
  if (present) {
    new_state.within_pos_window = true;

    // Cancels previous task if it is already scheduled.
    pos_window_timer_.Start(FROM_HERE, pos_window_, this,
                            &HpsNotifyController::OnMinWindowExpired);
  }

  UpdateSnooperStatus(new_state);
}

void HpsNotifyController::OnRestart() {
  DCHECK(!state_.hps_state);

  State new_state = state_;
  new_state.hps_available = true;

  ReconfigureHps(&new_state);
  UpdateSnooperStatus(new_state);
}

void HpsNotifyController::OnShutdown() {
  State new_state = state_;
  new_state.hps_available = false;

  ReconfigureHps(&new_state);
  UpdateSnooperStatus(new_state);

  // We will be notified of the service starting back up again via our ongoing
  // observation of the DBus client.
}

void HpsNotifyController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HpsNotifyController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool HpsNotifyController::SnooperPresent() const {
  return state_.within_pos_window ||
         (state_.session_active && state_.hps_state && state_.pref_enabled &&
          state_.orientation_suitable);
}

void HpsNotifyController::UpdateSnooperStatus(const State& new_state) {
  // Clean up new state to be consistent.
  const bool detection_active =
      new_state.session_active && new_state.pref_enabled &&
      new_state.hps_available && new_state.hps_configured &&
      new_state.orientation_suitable;

  State clean_state = new_state;
  clean_state.hps_state = new_state.hps_state && detection_active;
  clean_state.within_pos_window =
      new_state.within_pos_window && detection_active;

  const bool was_present = SnooperPresent();
  state_ = clean_state;
  const bool is_present = SnooperPresent();

  if (was_present == is_present)
    return;

  for (auto& observer : observers_)
    observer.OnSnoopingStatusChanged(is_present);
}

void HpsNotifyController::ReconfigureHps(State* new_state) {
  // Can't configure or de-configure the service if it's unavailable.
  if (!new_state->hps_available) {
    new_state->hps_configured = false;
    return;
  }

  // We have correctly cached that the service is available; now handle
  // configuring its signal.
  const bool want_configured = new_state->pref_enabled &&
                               new_state->session_active &&
                               new_state->orientation_suitable;
  if (state_.hps_configured == want_configured) {
    new_state->hps_configured = want_configured;
    return;
  }

  if (want_configured) {
    // Configure the snooping started/stopped signals that the service will
    // emit.
    const absl::optional<hps::FeatureConfig> config =
        hps::GetEnableHpsNotifyConfig();
    if (!config.has_value()) {
      LOG(ERROR) << "Couldn't parse notify configuration";
      return;
    }

    chromeos::HpsDBusClient::Get()->EnableHpsNotify(*config);

    // Populate our initial HPS state for consistency with the service.
    chromeos::HpsDBusClient::Get()->GetResultHpsNotify(base::BindOnce(
        &HpsNotifyController::UpdateHpsState, weak_ptr_factory_.GetWeakPtr()));
    new_state->hps_configured = true;

    return;
  }

  // No longer need signals to be emitted.
  chromeos::HpsDBusClient::Get()->DisableHpsNotify();
  new_state->hps_configured = false;
}

void HpsNotifyController::StartHpsObservation(bool service_is_available) {
  state_.hps_available = service_is_available;
  state_.hps_configured = false;

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
  ReconfigureHps(&state_);
  UpdateSnooperStatus(state_);
}

void HpsNotifyController::UpdateHpsState(
    absl::optional<hps::HpsResult> response) {
  LOG_IF(WARNING, !response.has_value())
      << "Polling the presence daemon failed";

  const bool present =
      response.value_or(hps::HpsResult::NEGATIVE) == hps::HpsResult::POSITIVE;

  State new_state = state_;
  new_state.hps_state = present;

  // Prevent snooping status from becoming negative within a window of time.
  if (present) {
    new_state.within_pos_window = true;

    // Cancels previous task if it is already scheduled.
    pos_window_timer_.Start(FROM_HERE, pos_window_, this,
                            &HpsNotifyController::OnMinWindowExpired);
  }

  UpdateSnooperStatus(new_state);
}

void HpsNotifyController::UpdatePrefState() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());
  const bool pref_enabled = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kSnoopingProtectionEnabled);

  State new_state = state_;
  new_state.pref_enabled = pref_enabled;

  ReconfigureHps(&new_state);
  UpdateSnooperStatus(new_state);
  base::UmaHistogramBoolean("ChromeOS.HPS.SnoopingProtection.Enabled",
                            pref_enabled);
}

void HpsNotifyController::OnMinWindowExpired() {
  State new_state = state_;
  new_state.within_pos_window = false;
  UpdateSnooperStatus(new_state);
}

}  // namespace ash
