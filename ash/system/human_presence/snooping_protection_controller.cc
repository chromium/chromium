// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/snooping_protection_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/human_presence/human_presence_metrics.h"
#include "ash/system/human_presence/snooping_protection_notification_blocker.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "chromeos/ash/components/human_presence/human_presence_configuration.h"
#include "components/account_id/account_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/message_center/message_center.h"

namespace ash {

namespace metrics = ash::snooping_protection_metrics;

SnoopingProtectionController::SnoopingProtectionController()
    : notification_blocker_(
          std::make_unique<SnoopingProtectionNotificationBlocker>(
              message_center::MessageCenter::Get(),
              this)),
      pos_window_(hps::GetSnoopingProtectionPositiveWindow()) {
  // When the controller is initialized, we are never in an active user session
  // and we never have any user preferences active. Hence, our default state
  // values are correct.

  // Finish initializing `notification_blocker_`.
  notification_blocker_->Init();

  // Session controller is instantiated before us in the shell.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  // Wait for the service to be available before subscribing to its events. If
  // we directly subscribe here, we will attempt to configure the DBus service
  // twice (once via this callback and once via |OnRestart|) if it's slow to
  // start. Configuring snooping protection without first disabling it is an
  // error.
  //
  // Might not exist in unit tests.
  if (HumanPresenceDBusClient::Get()) {
    HumanPresenceDBusClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&SnoopingProtectionController::StartServiceObservation,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Orientation controller is instantiated before us in the shell.
  HumanPresenceOrientationController* orientation_controller =
      Shell::Get()->human_presence_orientation_controller();
  state_.orientation_suitable = orientation_controller->IsOrientationSuitable();
  orientation_observation_.Observe(orientation_controller);
}

SnoopingProtectionController::~SnoopingProtectionController() {
  // This is a no-op if the service isn't available or isn't enabled.
  // TODO(crbug.com/40194784): only disable if the service is enabled.
  //
  // Might not exist in unit tests.
  if (HumanPresenceDBusClient::Get())
    HumanPresenceDBusClient::Get()->DisableHpsNotify();

  for (auto& observer : observers_)
    observer.OnSnoopingProtectionControllerDestroyed();

  // We want to log current presence/absence duration since we'll not get
  // another event anymore.
  LogPresenceWindow(state_.present);
}

// static
void SnoopingProtectionController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kSnoopingProtectionEnabled,
                                /*default_value=*/false);
  registry->RegisterBooleanPref(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled,
      /*default_value=*/true);
}

void SnoopingProtectionController::OnSessionStateChanged(
    session_manager::SessionState session_state) {
  const bool session_active =
      session_state == session_manager::SessionState::ACTIVE;

  State new_state = state_;
  new_state.session_active = session_active;

  ReconfigureService(&new_state);
  UpdateSnooperStatus(new_state);
}

void SnoopingProtectionController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  DCHECK(pref_service);
  const bool pref_enabled =
      pref_service->GetBoolean(prefs::kSnoopingProtectionEnabled);

  State new_state = state_;
  new_state.pref_enabled = pref_enabled;

  ReconfigureService(&new_state);
  UpdateSnooperStatus(new_state);

  // Re-subscribe to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionEnabled,
      base::BindRepeating(&SnoopingProtectionController::UpdatePrefState,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SnoopingProtectionController::OnOrientationChanged(
    bool suitable_for_human_presence) {
  State new_state = state_;
  new_state.orientation_suitable = suitable_for_human_presence;

  ReconfigureService(&new_state);
  UpdateSnooperStatus(new_state);
}

void SnoopingProtectionController::OnHpsSenseChanged(
    const hps::HpsResultProto&) {}

void SnoopingProtectionController::OnHpsNotifyChanged(
    const hps::HpsResultProto& result) {
  const bool present = result.value() == hps::HpsResult::POSITIVE;

  State new_state = state_;
  new_state.present = present;

  // Prevent snooping status from becoming negative within a window of time.
  if (present) {
    new_state.within_pos_window = true;

    // Cancels previous task if it is already scheduled.
    pos_window_timer_.Start(FROM_HERE, pos_window_, this,
                            &SnoopingProtectionController::OnMinWindowExpired);
  }

  UpdateSnooperStatus(new_state);
}

void SnoopingProtectionController::OnRestart() {
  DCHECK(!state_.present);

  State new_state = state_;
  new_state.service_available = true;

  ReconfigureService(&new_state);
  UpdateSnooperStatus(new_state);
}

void SnoopingProtectionController::OnShutdown() {
  // Log current presence window and reset the report time so that the next
  // present/absent duration will not be logged, because the duration will be
  // incorrect.
  // This has to be done before UpdateSnooperStatus below.
  LogPresenceWindow(state_.present);
  last_presence_report_time_ = base::TimeTicks();

  State new_state = state_;
  new_state.service_available = false;

  ReconfigureService(&new_state);
  UpdateSnooperStatus(new_state);

  // We will be notified of the service starting back up again via our ongoing
  // observation of the DBus client.
}

void SnoopingProtectionController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SnoopingProtectionController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SnoopingProtectionController::SnooperPresent() const {
  return state_.within_pos_window ||
         (state_.session_active && state_.present && state_.pref_enabled &&
          state_.orientation_suitable);
}

void SnoopingProtectionController::UpdateSnooperStatus(const State& new_state) {
  // Clean up new state to be consistent.
  const bool detection_active =
      new_state.session_active && new_state.pref_enabled &&
      new_state.service_available && new_state.service_configured &&
      new_state.orientation_suitable;

  State clean_state = new_state;
  clean_state.present = new_state.present && detection_active;
  clean_state.within_pos_window =
      new_state.within_pos_window && detection_active;

  // If the present state changes to false while within_pos_window, we would
  // have got a flakey disappearing of the eyecon without pos_window.
  if (clean_state.within_pos_window && !clean_state.present) {
    base::UmaHistogramBoolean("ChromeOS.HPS.SnoopingProtection.FlakeyDetection",
                              false);
  }
  const bool was_present = SnooperPresent();
  state_ = clean_state;
  const bool is_present = SnooperPresent();

  if (was_present == is_present)
    return;

  LogPresenceWindow(was_present);
  for (auto& observer : observers_)
    observer.OnSnoopingStatusChanged(is_present);
}

void SnoopingProtectionController::ReconfigureService(State* new_state) {
  // Can't configure or de-configure the service if it's unavailable.
  if (!new_state->service_available) {
    new_state->service_configured = false;
    return;
  }

  // We have correctly cached that the service is available; now handle
  // configuring its signal.
  const bool want_configured = new_state->pref_enabled &&
                               new_state->session_active &&
                               new_state->orientation_suitable;
  if (state_.service_configured == want_configured) {
    new_state->service_configured = want_configured;
    return;
  }

  if (want_configured) {
    // Configure the snooping started/stopped signals that the service will
    // emit.
    const std::optional<hps::FeatureConfig> config =
        hps::GetEnableSnoopingProtectionConfig();
    if (!config.has_value()) {
      LOG(ERROR) << "SnoopingProtectionController: couldn't parse HpsNotify "
                    "configuration.";
      return;
    }
    LOG(ERROR)
        << "SnoopingProtectionController: enabling HpsNotify from chrome.";

    HumanPresenceDBusClient::Get()->EnableHpsNotify(*config);

    // Populate our initial HPS state for consistency with the service.
    HumanPresenceDBusClient::Get()->GetResultHpsNotify(
        base::BindOnce(&SnoopingProtectionController::UpdateServiceState,
                       weak_ptr_factory_.GetWeakPtr()));
    new_state->service_configured = true;

    return;
  }

  // No longer need signals to be emitted.
  HumanPresenceDBusClient::Get()->DisableHpsNotify();
  new_state->service_configured = false;
}

void SnoopingProtectionController::StartServiceObservation(
    bool service_is_available) {
  state_.service_available = service_is_available;
  state_.service_configured = false;

  if (!service_is_available) {
    LOG(ERROR) << "Could not make initial connection to HPS service";
    return;
  }

  // Special case: at this point, the service could have been left in an enabled
  // state by a previous session that crashed (and hence didn't clean up
  // properly). Disable it here, which is a no-op if it is already disabled.
  HumanPresenceDBusClient::Get()->DisableHpsNotify();

  // Start listening for state updates and restarts/shutdowns.
  human_presence_dbus_observation_.Observe(HumanPresenceDBusClient::Get());

  // Configure the service and poll its initial value if necessary.
  ReconfigureService(&state_);
  UpdateSnooperStatus(state_);
}

// This callback almost always runs as the service is starting up.
// LogPresenceWindow is purposefully not called inside ths function, because
// during startup the service reports an UNKNOWN state, so there's a risk of
// logging a spurious window of absence.
void SnoopingProtectionController::UpdateServiceState(
    std::optional<hps::HpsResultProto> response) {
  LOG_IF(WARNING, !response.has_value())
      << "Polling the presence daemon failed";

  const bool present =
      response.has_value() && response->value() == hps::HpsResult::POSITIVE;

  State new_state = state_;
  new_state.present = present;

  // Prevent snooping status from becoming negative within a window of time.
  if (present) {
    new_state.within_pos_window = true;

    // Cancels previous task if it is already scheduled.
    pos_window_timer_.Start(FROM_HERE, pos_window_, this,
                            &SnoopingProtectionController::OnMinWindowExpired);
  }

  UpdateSnooperStatus(new_state);
}

void SnoopingProtectionController::UpdatePrefState() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());
  const bool pref_enabled = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kSnoopingProtectionEnabled);

  State new_state = state_;
  new_state.pref_enabled = pref_enabled;

  ReconfigureService(&new_state);
  UpdateSnooperStatus(new_state);
  base::UmaHistogramBoolean(metrics::kEnabledHistogramName, pref_enabled);
}

void SnoopingProtectionController::OnMinWindowExpired() {
  State new_state = state_;
  new_state.within_pos_window = false;
  UpdateSnooperStatus(new_state);
}

void SnoopingProtectionController::LogPresenceWindow(bool was_present) {
  const auto now = base::TimeTicks::Now();

  // Set last_presence_report_time_ and return if it is the first time reported.
  if (last_presence_report_time_.is_null()) {
    last_presence_report_time_ = now;
    return;
  }

  const auto time_since_last_report = now - last_presence_report_time_;
  last_presence_report_time_ = now;

  if (was_present) {
    base::UmaHistogramCustomTimes(metrics::kPositiveDurationHistogramName,
                                  time_since_last_report, metrics::kDurationMin,
                                  metrics::kPositiveDurationMax,
                                  metrics::kDurationNumBuckets);
  } else {
    base::UmaHistogramCustomTimes(metrics::kNegativeDurationHistogramName,
                                  time_since_last_report, metrics::kDurationMin,
                                  metrics::kNegativeDurationMax,
                                  metrics::kDurationNumBuckets);
  }
}

}  // namespace ash
