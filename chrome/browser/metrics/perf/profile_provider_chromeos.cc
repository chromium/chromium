// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/perf/metric_provider.h"
#include "chrome/browser/metrics/perf/perf_events_collector.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/common/content_switches.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

const char kJankinessTriggerStatusHistogram[] =
    "ChromeOS.CWP.JankinessTriggerStatus";

// The default value of minimum interval between jankiness collections is 30
// minutes.
const int kDefaultJankinessCollectionMinIntervalSec = 30 * 60;

enum class JankinessTriggerStatus {
  // Attempt to collect a profile triggered by browser jankiness.
  kCollectionAttempted,
  // The collection is throttled.
  kThrottled,
  kMaxValue = kThrottled
};

// Returns true if a normal user is logged in. Returns false otherwise (e.g. if
// logged in as a guest or as a kiosk app).
bool IsNormalUserLoggedIn() {
  return ash::LoginState::Get()->IsUserAuthenticated();
}

}  // namespace

ProfileProvider::ProfileProvider()
    : jankiness_collection_min_interval_(
          base::Seconds(kDefaultJankinessCollectionMinIntervalSec)) {
  // Initialize the WindowedIncognitoMonitor on the UI thread.
  WindowedIncognitoMonitor::Init();
  // Register a perf events collector.
  collectors_.push_back(std::make_unique<MetricProvider>(
      std::make_unique<PerfCollector>(), g_browser_process->profile_manager()));
}

ProfileProvider::~ProfileProvider() {
  ash::LoginState::Get()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  base::PowerMonitor::GetInstance()->RemovePowerThermalObserver(this);
  if (jank_monitor_) {
    jank_monitor_->RemoveObserver(this);
    jank_monitor_->Destroy();
  }
}

void ProfileProvider::Init() {
  for (auto& collector : collectors_) {
    collector->Init();
  }

  // Register as an observer of login state changes.
  ash::LoginState::Get()->AddObserver(this);

  // Register as an observer of power manager events.
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  // Register as an observer of session restore.
  on_session_restored_callback_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
          &ProfileProvider::OnSessionRestoreDone, weak_factory_.GetWeakPtr()));

  // Register as an observer of thermal state changes.
  base::PowerThermalObserver::DeviceThermalState thermal_state =
      base::PowerMonitor::GetInstance()
          ->AddPowerStateObserverAndReturnPowerThermalState(this);
  OnThermalStateChange(thermal_state);

  // Check the login state. At the time of writing, this class is instantiated
  // before login. A subsequent login would activate the profiling. However,
  // that behavior may change in the future so that the user is already logged
  // when this class is instantiated. By calling LoggedInStateChanged() here,
  // ProfileProvider will recognize that the system is already logged in.
  LoggedInStateChanged();

  // Set up the JankMonitor for watching browser jankiness.
  jank_monitor_ = content::JankMonitor::Create();
  jank_monitor_->SetUp();
  jank_monitor_->AddObserver(this);
}

bool ProfileProvider::GetSampledProfiles(
    std::vector<SampledProfile>* sampled_profiles) {
  bool result = false;
  for (auto& collector : collectors_) {
    bool written = collector->GetSampledProfiles(sampled_profiles);
    result = result || written;
  }
  return result;
}

void ProfileProvider::OnRecordingEnabled() {
  for (auto& collector : collectors_) {
    collector->EnableRecording();
  }
}

void ProfileProvider::OnRecordingDisabled() {
  for (auto& collector : collectors_) {
    collector->DisableRecording();
  }
}

void ProfileProvider::LoggedInStateChanged() {
  if (IsNormalUserLoggedIn()) {
    for (auto& collector : collectors_) {
      collector->OnUserLoggedIn();
    }
  } else {
    for (auto& collector : collectors_) {
      collector->Deactivate();
    }
  }
}

void ProfileProvider::SuspendDone(base::TimeDelta sleep_duration) {
  // A zero value for the suspend duration indicates that the suspend was
  // canceled. Do not collect anything if that's the case.
  if (sleep_duration.is_zero())
    return;

  // Do not collect a profile unless logged in. The system behavior when closing
  // the lid or idling when not logged in is currently to shut down instead of
  // suspending. But it's good to enforce the rule here in case that changes.
  if (!IsNormalUserLoggedIn())
    return;

  // Inform each collector that a successful suspend has completed.
  for (auto& collector : collectors_) {
    collector->SuspendDone(sleep_duration);
  }
}

void ProfileProvider::OnSessionRestoreDone(Profile* profile,
                                           int num_tabs_restored) {
  // Do not collect a profile unless logged in as a normal user.
  if (!IsNormalUserLoggedIn())
    return;

  // Inform each collector of a session restore event.
  for (auto& collector : collectors_) {
    collector->OnSessionRestoreDone(num_tabs_restored);
  }
}

void ProfileProvider::OnJankStarted() {
  if (!IsNormalUserLoggedIn())
    return;

  // For JANKY_TASK collection, require successive collections to happen between
  // this duration at minimum. Subsequent janky task detected within this
  // interval will be throttled.
  if (!last_jank_start_time_.is_null() &&
      base::TimeTicks::Now() - last_jank_start_time_ <
          jankiness_collection_min_interval_) {
    UMA_HISTOGRAM_ENUMERATION(kJankinessTriggerStatusHistogram,
                              JankinessTriggerStatus::kThrottled);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kJankinessTriggerStatusHistogram,
                            JankinessTriggerStatus::kCollectionAttempted);
  last_jank_start_time_ = base::TimeTicks::Now();

  // Inform each collector that a jank is observed.
  for (auto& collector : collectors_) {
    collector->OnJankStarted();
  }
}

void ProfileProvider::OnJankStopped() {
  if (!IsNormalUserLoggedIn())
    return;

  // Inform each collector that a jank has stopped.
  for (auto& collector : collectors_) {
    collector->OnJankStopped();
  }
}

void ProfileProvider::OnThermalStateChange(
    base::PowerThermalObserver::DeviceThermalState new_state) {
  // Pass the new thermal state to each collector.
  for (auto& collector : collectors_) {
    collector->SetThermalState(new_state);
  }
}

void ProfileProvider::OnSpeedLimitChange(int new_limit) {
  // Pass the new speed limit to each collector.
  for (auto& collector : collectors_) {
    collector->SetSpeedLimit(new_limit);
  }
}

}  // namespace metrics
