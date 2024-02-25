// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_EVENTS_OBSERVER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "content/public/browser/web_contents.h"

namespace reporting {

// Event observer that observes relevant website activity events supported by
// the `WebsiteMetrics` component for reporting purposes.
class WebsiteEventsObserver : public MetricEventObserver,
                              public ::apps::WebsiteMetrics::Observer {
 public:
  WebsiteEventsObserver(std::unique_ptr<WebsiteMetricsRetrieverInterface>
                            website_metrics_retriever,
                        const ReportingSettings* reporting_settings);
  WebsiteEventsObserver(const WebsiteEventsObserver& other) = delete;
  WebsiteEventsObserver& operator=(const WebsiteEventsObserver& other) = delete;
  ~WebsiteEventsObserver() override;

  // MetricEventObserver:
  void SetOnEventObservedCallback(MetricRepeatingCallback callback) override;

  // MetricEventObserver:
  void SetReportingEnabled(bool is_enabled) override;

  // ::apps::WebsiteMetrics::Observer:
  void OnUrlOpened(const GURL& url_opened,
                   ::content::WebContents* web_contents) override;

  // ::apps::WebsiteMetrics::Observer:
  void OnUrlClosed(const GURL& url_closed,
                   ::content::WebContents* web_contents) override;

 private:
  // Initializes events observer and starts observing website events tracked
  // by the `WebsiteMetrics` component (if initialized).
  void InitEventObserver(::apps::WebsiteMetrics* website_metrics);

  // ::apps::WebsiteMetrics::Observer:
  void OnWebsiteMetricsDestroyed() override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Retriever that retrieves the `WebsiteMetrics` component so the
  // `WebsiteEventsObserver` can start observing website events.
  const std::unique_ptr<WebsiteMetricsRetrieverInterface>
      website_metrics_retriever_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Pointer to the reporting settings that controls website activity event
  // reporting. Guaranteed to outlive the observer because it is managed by the
  // metric reporting manager.
  const raw_ptr<const ReportingSettings> reporting_settings_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Observer for tracking website events. Will be reset if the `WebsiteMetrics`
  // component gets destructed before the event observer.
  base::ScopedObservation<::apps::WebsiteMetrics,
                          ::apps::WebsiteMetrics::Observer>
      observer_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  // Callback triggered when website metrics are collected and website activity
  // event reporting is enabled.
  MetricRepeatingCallback on_metric_observed_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<WebsiteEventsObserver> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_EVENTS_OBSERVER_H_
