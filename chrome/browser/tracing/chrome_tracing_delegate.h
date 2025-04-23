// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
#define CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/tracing_delegate.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#else
#include "chrome/browser/ui/browser_list_observer.h"
#endif

namespace tracing {
class BackgroundTracingStateManager;
}

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "TracingFinalizationDisallowedReason" in
// src/tools/metrics/histograms/enums.xml.
enum class TracingFinalizationDisallowedReason {
  kIncognitoLaunched = 0,
  //  kProfileNotLoaded = 1, Obsolete
  //  kCrashMetricsNotLoaded = 2, Obsolete
  //  kLastSessionCrashed = 3, Obsolete
  //  kMetricsReportingDisabled = 4, Obsolete
  //  kTraceUploadedRecently = 5, Obsolete
  //  kLastTracingSessionDidNotEnd = 6, Obsolete as of Nov'2024.
  kMaxValue = kIncognitoLaunched
};

class ChromeTracingDelegate : public content::TracingDelegate,
#if BUILDFLAG(IS_ANDROID)
                              public TabModelListObserver
#else
                              public BrowserListObserver
#endif
{
 public:
  // Whether system-wide performance trace collection using the external system
  // tracing service is enabled.
  static bool IsSystemWideTracingEnabled();

  ChromeTracingDelegate();
  ~ChromeTracingDelegate() override;

  // content::TracingDelegate implementation:
  bool IsRecordingAllowed(bool requires_anonymized_data) const override;

  bool ShouldSaveUnuploadedTrace() const override;

#if BUILDFLAG(IS_WIN)
  void GetSystemTracingState(
      base::OnceCallback<void(bool service_supported, bool service_enabled)>
          on_tracing_state) override;
  void EnableSystemTracing(
      base::OnceCallback<void(bool success)> on_complete) override;
  void DisableSystemTracing(
      base::OnceCallback<void(bool success)> on_complete) override;
#endif  // BUILDFLAG(IS_WIN)

 private:
#if BUILDFLAG(IS_ANDROID)
  // TabModelListObserver implementation.
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;
#else
  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override;
#endif

  bool incognito_launched_ = false;

  std::unique_ptr<tracing::BackgroundTracingStateManager> state_manager_;
};

#endif  // CHROME_BROWSER_TRACING_CHROME_TRACING_DELEGATE_H_
