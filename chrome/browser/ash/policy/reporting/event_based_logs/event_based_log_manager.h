// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_MANAGER_H_

#include <map>
#include <memory>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"

namespace policy {

class EventBasedLogManager {
 public:
  EventBasedLogManager();

  EventBasedLogManager(const EventBasedLogManager&) = delete;
  EventBasedLogManager& operator=(const EventBasedLogManager&) = delete;

  ~EventBasedLogManager();

  // Subscribes to LogUploadEnabled policy changes. When the policy is enabled,
  // creates `event_observers_`. When policy is disabled, deletes all
  // EventObservers from `event_observers_` so we don't listen for events.
  // Individual `EventObserverBase` implementations are responsible for checking
  // any related policies specific to events they're observing.
  void OnLogUploadEnabledPolicyUpdated();

  const std::map<ash::reporting::TriggerEventType,
                 std::unique_ptr<EventObserverBase>>&
  GetEventObserversForTesting() const;

 private:
  // Initializes and adds all available event observers to `event_observers_`.
  void MaybeAddAllEventObservers();

  SEQUENCE_CHECKER(sequence_checker_);
  // List of event observers.
  std::map<ash::reporting::TriggerEventType, std::unique_ptr<EventObserverBase>>
      event_observers_;
  base::CallbackListSubscription log_upload_enabled_policy_subscription_;
  base::WeakPtrFactory<EventBasedLogManager> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_BASED_LOG_MANAGER_H_
