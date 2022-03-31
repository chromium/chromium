// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
#define CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/tracing_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#else
#include "chrome/browser/ui/browser_list_observer.h"
#endif

class PrefRegistrySimple;

namespace base {
class Time;
class Value;
}

class ChromeTracingDelegate : public content::TracingDelegate,
#if BUILDFLAG(IS_ANDROID)
                              public TabModelListObserver
#else
                              public BrowserListObserver
#endif
{
 public:
  ChromeTracingDelegate();
  ~ChromeTracingDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns if the tracing session is allowed to begin. Also updates the
  // background tracing state in prefs using BackgroundTracingStateManager. So,
  // this is required to be called exactly once per background tracing session
  // before tracing is started. If this returns true, a tasks is posted 30
  // seconds into the future that will mark a successful startup / run of a
  // trace and will allow tracing to run next time.
  bool IsAllowedToBeginBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data) override;

  // Returns true if tracing is allowed to end. Also updates the background
  // tracing state in prefs using BackgroundTracingStateManager when returning
  // true. This is required to be called before stopping background tracing.
  bool IsAllowedToEndBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data,
      bool is_crash_scenario) override;

  bool IsSystemWideTracingEnabled() override;

  absl::optional<base::Value> GenerateMetadataDict() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingThrottleTimeElapsed);
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingTimeThrottledAfterPreviousDay);
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingUnexpectedSessionEnd);
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingSessionRanLong);
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingTimeThrottledUpdatedScenario);
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingTimeThrottledDifferentScenario);
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingFinalizationStarted);
  FRIEND_TEST_ALL_PREFIXES(ChromeTracingDelegateBrowserTest,
                           BackgroundTracingFinalizationBefore30Seconds);

  // Do not remove or change the order of enum fields since it is stored in
  // preferences.
  enum class BackgroundTracingState : int {
    // Default state when tracing is not started in previous session, or when
    // state is not found or invalid.
    NOT_ACTIVATED = 0,
    STARTED = 1,
    RAN_30_SECONDS = 2,
    FINALIZATION_STARTED = 3,
    LAST = FINALIZATION_STARTED,
  };

  using ScenarioUploadTimestampMap = base::flat_map<std::string, base::Time>;

  // Manages local state prefs for background tracing, and tracks state from
  // previous background tracing session(s). This is a singleton, but there
  // could be many instances of ChromeTracingDelegate. All the calls are
  // expected to run on UI thread.
  class BackgroundTracingStateManager {
   public:
    static BackgroundTracingStateManager& GetInstance();

    // Initializes state from previous session and writes current state to
    // prefs, when called the first time. NOOP on any calls after that. It also
    // deletes any expired entries from prefs.
    void Initialize();

    // True if last session potentially crashed and it is unsafe to turn on
    // background tracing in current session.
    bool DidLastSessionEndUnexpectedly() const;
    // True if chrome uploaded a trace for the given |config| recently, and
    // uploads should be throttled for the |config|.
    bool DidRecentlyUploadForScenario(
        const content::BackgroundTracingConfig& config) const;

    // Updates the current tracing state and saves it to prefs.
    void SetState(BackgroundTracingState new_state);
    // Updates the state to include the upload time for |scenario_name|, and
    // saves it to prefs.
    void OnScenarioUploaded(const std::string& scenario_name);

    // Saves the given state to prefs, public for testing.
    static void SaveState(const ScenarioUploadTimestampMap& upload_times,
                          BackgroundTracingState state);

   private:
    friend base::NoDestructor<BackgroundTracingStateManager>;

    BackgroundTracingStateManager();
    ~BackgroundTracingStateManager();

    void SaveState();

    BackgroundTracingState state_ = BackgroundTracingState::NOT_ACTIVATED;

    bool initialized_ = false;

    // Following are valid only when |initialized_| = true.
    BackgroundTracingState last_session_end_state_ =
        BackgroundTracingState::NOT_ACTIVATED;
    base::flat_map<std::string, base::Time> scenario_last_upload_timestamp_;
  };

#if BUILDFLAG(IS_ANDROID)
  // TabModelListObserver implementation.
  void OnTabModelAdded() override;
  void OnTabModelRemoved() override;
#else
  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
#endif

  // The types of action that are guarded by IsActionAllowed.
  enum class BackgroundScenarioAction {
    kStartTracing,
    kUploadTrace,
  };

  // Returns true if the delegate should be allowed to perform `action` for the
  // scenario described in `config`.
  bool IsActionAllowed(BackgroundScenarioAction action,
                       const content::BackgroundTracingConfig& config,
                       bool requires_anonymized_data,
                       bool ignore_trace_limit) const;

  bool incognito_launched_ = false;
};

#endif  // CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
