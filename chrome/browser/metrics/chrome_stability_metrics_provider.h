// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_STABILITY_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_CHROME_STABILITY_METRICS_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/stability_metrics_helper.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if defined(OS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif  // defined(OS_ANDROID)

class PrefService;

// ChromeStabilityMetricsProvider gathers and logs Chrome-specific stability-
// related metrics.
class ChromeStabilityMetricsProvider
    : public metrics::MetricsProvider,
      public content::BrowserChildProcessObserver,
#if defined(OS_ANDROID)
      public crash_reporter::CrashMetricsReporter::Observer,
#endif
      public content::NotificationObserver {
 public:
  explicit ChromeStabilityMetricsProvider(PrefService* local_state);
  ~ChromeStabilityMetricsProvider() override;

  // metrics::MetricsDataProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideStabilityMetrics(
      metrics::SystemProfileProto* system_profile_proto) override;
  void ClearSavedStabilityMetrics() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeStabilityMetricsProviderTest,
                           BrowserChildProcessObserverGpu);
  FRIEND_TEST_ALL_PREFIXES(ChromeStabilityMetricsProviderTest,
                           BrowserChildProcessObserverUtility);
  FRIEND_TEST_ALL_PREFIXES(ChromeStabilityMetricsProviderTest,
                           NotificationObserver);

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessCrashed(
      const content::ChildProcessData& data,
      const content::ChildProcessTerminationInfo& info) override;
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;

#if defined(OS_ANDROID)
  // crash_reporter::CrashMetricsReporter::Observer:
  void OnCrashDumpProcessed(
      int rph_id,
      const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
          reported_counts) override;

  ScopedObserver<crash_reporter::CrashMetricsReporter,
                 crash_reporter::CrashMetricsReporter::Observer>
      scoped_observer_;
#endif  // defined(OS_ANDROID)

  metrics::StabilityMetricsHelper helper_;

  // Registrar for receiving stability-related notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStabilityMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_CHROME_STABILITY_METRICS_PROVIDER_H_
