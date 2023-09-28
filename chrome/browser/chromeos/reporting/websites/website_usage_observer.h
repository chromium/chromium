// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "url/gurl.h"

namespace reporting {

// Observer used to observe and collect website usage data from the
// `WebsiteMetrics` component so it can be persisted in the pref store and
// reported subsequently.
class WebsiteUsageObserver : public ::apps::WebsiteMetrics::Observer {
 public:
  WebsiteUsageObserver(base::WeakPtr<Profile> profile,
                       const ReportingSettings* reporting_settings,
                       std::unique_ptr<WebsiteMetricsRetrieverInterface>
                           website_metrics_retriever);
  WebsiteUsageObserver(const WebsiteUsageObserver& other) = delete;
  WebsiteUsageObserver& operator=(const WebsiteUsageObserver& other) = delete;
  ~WebsiteUsageObserver() override;

  // ::apps::WebsiteMetrics::Observer:
  void OnUrlUsage(const GURL& url, base::TimeDelta running_time) override;

 private:
  // Initializes the usage observer and starts observing website usage
  // collection tracked by the `WebsiteMetrics` component (if initialized).
  void InitUsageObserver(::apps::WebsiteMetrics* website_metrics);

  // ::apps::WebsiteMetrics::Observer:
  void OnWebsiteMetricsDestroyed() override;

  // Aggregates the website usage entry with the specified usage/running time
  // and persists it in the pref store. Creates a new placeholder entry if one
  // does not exist for the specified URL.
  void CreateOrUpdateWebsiteUsageEntry(const GURL& url,
                                       const base::TimeDelta& running_time);

  // Returns true if the website usage telemetry type is enabled for reporting
  // purposes. False otherwise.
  bool IsWebsiteUsageTelemetryEnabled() const;

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer to the user profile. Used to save usage data to the user pref
  // store.
  const base::WeakPtr<Profile> profile_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Pointer to the reporting settings component that outlives the
  // `WebsiteUsageObserver`. Used to control usage data collection.
  const raw_ptr<const ReportingSettings> reporting_settings_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Retriever that retrieves the `WebsiteMetrics` component so the usage
  // observer can start tracking website usage collection.
  const std::unique_ptr<WebsiteMetricsRetrieverInterface>
      website_metrics_retriever_;

  // Observer for tracking website usage collection. Will be reset if the
  // `WebsiteMetrics` component gets destructed before the usage observer.
  base::ScopedObservation<::apps::WebsiteMetrics,
                          ::apps::WebsiteMetrics::Observer>
      observer_{this};

  base::WeakPtrFactory<WebsiteUsageObserver> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_OBSERVER_H_
