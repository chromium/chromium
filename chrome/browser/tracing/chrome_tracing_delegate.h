// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
#define CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_

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

namespace base {
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
