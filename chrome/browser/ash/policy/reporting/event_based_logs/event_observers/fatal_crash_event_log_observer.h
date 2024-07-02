// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVERS_FATAL_CRASH_EVENT_LOG_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVERS_FATAL_CRASH_EVENT_LOG_OBSERVER_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observer_base.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"

namespace policy {

// Observes `reporting::FatalCrashEventsObserver` for fatal crashes. When fatal
// crash event is observed, uploads the related log files to server.
// `reporting::FatalCrashEventsObserver` will notify this if only the reporting
// policy is enabled.
class FatalCrashEventLogObserver
    : public EventObserverBase,
      public reporting::FatalCrashEventsObserver::FatalCrashEventLogObserver {
 public:
  FatalCrashEventLogObserver();
  ~FatalCrashEventLogObserver() override;

  // EventObserverBase
  ash::reporting::TriggerEventType GetEventType() const override;
  std::set<support_tool::DataCollectorType> GetDataCollectorTypes()
      const override;

  // reporting::FatalCrashEventsObserver::FatalCrashEventLogObserver:
  void OnFatalCrashEvent(const std::string& upload_id) override;

 private:
  void OnUploadTriggered(EventBasedUploadStatus status);

  base::ScopedObservation<
      reporting::FatalCrashEventsObserver,
      reporting::FatalCrashEventsObserver::FatalCrashEventLogObserver>
      observation_{this};
  base::WeakPtrFactory<FatalCrashEventLogObserver> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EVENT_BASED_LOGS_EVENT_OBSERVERS_FATAL_CRASH_EVENT_LOG_OBSERVER_H_
