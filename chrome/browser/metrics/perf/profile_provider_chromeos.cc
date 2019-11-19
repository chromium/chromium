// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/profile_provider_chromeos.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "chrome/browser/metrics/perf/heap_collector.h"
#include "chrome/browser/metrics/perf/metric_provider.h"
#include "chrome/browser/metrics/perf/perf_events_collector.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "content/public/common/content_switches.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

const base::Feature kBrowserJankinessProfiling{
    "BrowserJankinessProfiling", base::FEATURE_DISABLED_BY_DEFAULT};

const char kJankinessTriggerStatusHistogram[] =
    "ChromeOS.CWP.JankinessTriggerStatus";

// The default value of minimum interval between jankiness collections is 30
// minutes.
const int kDefaultJankinessCollectionMinIntervalSec = 30 * 60;

// Feature parameters that control the behavior of the jankiness trigger.
constexpr base::FeatureParam<int> kJankinessCollectionMinIntervalSec{
    &kBrowserJankinessProfiling, "JankinessCollectionMinIntervalSec",
    kDefaultJankinessCollectionMinIntervalSec};

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
  return chromeos::LoginState::Get()->IsUserAuthenticated();
}

}  // namespace

ProfileProvider::ProfileProvider()
    : jankiness_collection_min_interval_(base::TimeDelta::FromSeconds(
          kJankinessCollectionMinIntervalSec.Get())) {
  // Initialize the WindowedIncognitoMonitor on the UI thread.
  WindowedIncognitoMonitor::Init();
  // Register a perf events collector.
  collectors_.push_back(
      std::make_unique<MetricProvider>(std::make_unique<PerfCollector>()));
}

ProfileProvider::~ProfileProvider() {
  chromeos::LoginState::Get()->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  if (jank_monitor_) {
    jank_monitor_->RemoveObserver(this);
    jank_monitor_->Destroy();
  }
}

void ProfileProvider::Init() {
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  if (base::FeatureList::IsEnabled(heap_profiling::kOOPHeapProfilingFeature)) {
    HeapCollectionMode mode = HeapCollector::CollectionModeFromString(
        base::GetFieldTrialParamValueByFeature(
            heap_profiling::kOOPHeapProfilingFeature,
            heap_profiling::kOOPHeapProfilingFeatureMode));
    if (mode != HeapCollectionMode::kNone) {
      collectors_.push_back(std::make_unique<MetricProvider>(
          std::make_unique<HeapCollector>(mode)));
    }
  }
#endif

  for (auto& collector : collectors_) {
    collector->Init();
  }

  // Register as an observer of login state changes.
  chromeos::LoginState::Get()->AddObserver(this);

  // Register as an observer of power manager events.
  chromeos::PowerManagerClient::Get()->AddObserver(this);

  // Register as an observer of session restore.
  on_session_restored_callback_subscription_ =
      SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
          &ProfileProvider::OnSessionRestoreDone, weak_factory_.GetWeakPtr()));

  // Check the login state. At the time of writing, this class is instantiated
  // before login. A subsequent login would activate the profiling. However,
  // that behavior may change in the future so that the user is already logged
  // when this class is instantiated. By calling LoggedInStateChanged() here,
  // ProfileProvider will recognize that the system is already logged in.
  LoggedInStateChanged();

  if (base::FeatureList::IsEnabled(kBrowserJankinessProfiling)) {
    // Set up the JankMonitor for watching browser jankiness.
    jank_monitor_ =
        base::MakeRefCounted<content::responsiveness::JankMonitor>();
    jank_monitor_->SetUp();
    jank_monitor_->AddObserver(this);
  }
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

void ProfileProvider::SuspendDone(const base::TimeDelta& sleep_duration) {
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

void ProfileProvider::OnSessionRestoreDone(int num_tabs_restored) {
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

}  // namespace metrics
