// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_manager.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observers/os_update_event_observer.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace {

// `EnumerateEnumValues()` is not available on Chromium so we have hard-coded
// list of available enum values.
constexpr ash::reporting::TriggerEventType kAllTriggerEventTypes[] = {
    ash::reporting::TRIGGER_EVENT_TYPE_UNSPECIFIED,
    ash::reporting::OS_UPDATE_FAILED};

}  // namespace

namespace policy {

EventBasedLogManager::EventBasedLogManager() {
  ash::CrosSettings* settings = ash::CrosSettings::Get();
  log_upload_enabled_policy_subscription_ = settings->AddSettingsObserver(
      ash::kSystemLogUploadEnabled,
      base::BindRepeating(
          &EventBasedLogManager::OnLogUploadEnabledPolicyUpdated,
          weak_ptr_factory_.GetWeakPtr()));

  // Check the device policy for the first time after initialization. Schedule a
  // callback if device policy has not yet been verified.
  ash::CrosSettingsProvider::TrustedStatus status =
      settings->PrepareTrustedValues(
          base::BindOnce(&EventBasedLogManager::OnLogUploadEnabledPolicyUpdated,
                         weak_ptr_factory_.GetWeakPtr()));
  if (status == ash::CrosSettingsProvider::TRUSTED) {
    OnLogUploadEnabledPolicyUpdated();
  }
}

EventBasedLogManager::~EventBasedLogManager() = default;

void EventBasedLogManager::OnLogUploadEnabledPolicyUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ash::CrosSettings* settings = ash::CrosSettings::Get();
  bool log_upload_enabled = false;
  LOG_IF(WARNING, (!settings->GetBoolean(ash::kSystemLogUploadEnabled,
                                         &log_upload_enabled)))
      << "Disabling event based log upload because LogUploadEnabled is policy "
         "not found.";
  if (log_upload_enabled) {
    MaybeAddAllEventObservers();
  } else {
    // Remove all observers if the policy is disabled.
    event_observers_.clear();
  }
}

void EventBasedLogManager::MaybeAddAllEventObservers() {
  if (!event_observers_.empty()) {
    // If there are already observers on `event_observers_`, do nothing.
    LOG(WARNING) << "Event observers already exist.";
    return;
  }
  for (const auto event_type : kAllTriggerEventTypes) {
    switch (event_type) {
      case ash::reporting::TriggerEventType::OS_UPDATE_FAILED:
        event_observers_.emplace(event_type,
                                 std::make_unique<OsUpdateEventObserver>());
        break;
      case ash::reporting::TRIGGER_EVENT_TYPE_UNSPECIFIED:
        continue;
      default:
        NOTREACHED();
    }
  }
}

const std::map<ash::reporting::TriggerEventType,
               std::unique_ptr<EventObserverBase>>&
EventBasedLogManager::GetEventObserversForTesting() const {
  CHECK_IS_TEST();
  return event_observers_;
}

}  // namespace policy
