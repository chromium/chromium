// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_COLLECTOR_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_COLLECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace reporting {

// Collector used to observe and collect app usage data from the
// `AppPlatformMetrics` component so it can be persisted in the pref store and
// reported subsequently.
class AppUsageCollector : public ::apps::AppPlatformMetrics::Observer {
 public:
  AppUsageCollector(Profile* profile,
                    const ReportingSettings* reporting_settings,
                    ::apps::AppPlatformMetrics* app_platform_metrics);
  AppUsageCollector(const AppUsageCollector& other) = delete;
  AppUsageCollector& operator=(const AppUsageCollector& other) = delete;
  ~AppUsageCollector() override;

 private:
  // ::apps::AppPlatformMetrics::Observer:
  void OnAppUsage(const std::string& app_id,
                  ::apps::AppType app_type,
                  const base::UnguessableToken& instance_id,
                  base::TimeDelta running_time) override;

  // Aggregates the app usage entry with the specified usage/running time and
  // persists it in the pref store. Creates a new placeholder entry if one does
  // not exist for the specified instance.
  void CreateOrUpdateAppUsageEntry(const std::string& app_id,
                                   ::apps::AppType app_type,
                                   const base::UnguessableToken& instance_id,
                                   const base::TimeDelta& running_time);

  const raw_ptr<Profile> profile_;
  const raw_ptr<const ReportingSettings> reporting_settings_;
  const raw_ptr<::apps::AppPlatformMetrics> app_platform_metrics_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_COLLECTOR_H_
