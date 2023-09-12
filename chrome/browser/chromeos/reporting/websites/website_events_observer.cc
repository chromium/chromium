// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_events_observer.h"

#include <memory>

#include "base/sequence_checker.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_interface.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace reporting {

WebsiteEventsObserver::WebsiteEventsObserver(
    std::unique_ptr<WebsiteMetricsRetrieverInterface> website_metrics_retriever,
    const ReportingSettings* reporting_settings)
    : website_metrics_retriever_(std::move(website_metrics_retriever)),
      reporting_settings_(reporting_settings) {
  CHECK(website_metrics_retriever_);
  website_metrics_retriever_->GetWebsiteMetrics(
      base::BindOnce(&WebsiteEventsObserver::InitEventObserver,
                     weak_ptr_factory_.GetWeakPtr()));
}

WebsiteEventsObserver::~WebsiteEventsObserver() = default;

void WebsiteEventsObserver::SetOnEventObservedCallback(
    MetricRepeatingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!on_metric_observed_) << "Observer callback already set";
  on_metric_observed_ = std::move(callback);
}

void WebsiteEventsObserver::SetReportingEnabled(bool is_enabled) {
  // Do nothing. We retrieve the reporting setting and validate the website URL
  // is allowlisted before we report an observed event.
}

void WebsiteEventsObserver::OnUrlOpened(const GURL& url_opened,
                                        ::content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWebsiteUrlAllowlisted(url_opened, reporting_settings_,
                               kReportWebsiteActivityAllowlist)) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::URL_OPENED);

  auto* const website_opened_data = metric_data.mutable_telemetry_data()
                                        ->mutable_website_telemetry()
                                        ->mutable_website_opened_data();
  website_opened_data->set_url(url_opened.spec());
  website_opened_data->set_render_process_host_id(
      web_contents->GetRenderViewHost()->GetProcess()->GetID());
  website_opened_data->set_render_frame_routing_id(
      web_contents->GetRenderViewHost()->GetRoutingID());

  on_metric_observed_.Run(std::move(metric_data));
}

void WebsiteEventsObserver::OnUrlClosed(const GURL& url_closed,
                                        ::content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsWebsiteUrlAllowlisted(url_closed, reporting_settings_,
                               kReportWebsiteActivityAllowlist)) {
    return;
  }

  MetricData metric_data;
  metric_data.mutable_event_data()->set_type(MetricEventType::URL_CLOSED);

  auto* const website_closed_data = metric_data.mutable_telemetry_data()
                                        ->mutable_website_telemetry()
                                        ->mutable_website_closed_data();
  website_closed_data->set_url(url_closed.spec());
  website_closed_data->set_render_process_host_id(
      web_contents->GetRenderViewHost()->GetProcess()->GetID());
  website_closed_data->set_render_frame_routing_id(
      web_contents->GetRenderViewHost()->GetRoutingID());

  on_metric_observed_.Run(std::move(metric_data));
}

void WebsiteEventsObserver::InitEventObserver(
    ::apps::WebsiteMetrics* website_metrics) {
  // Runs on the same sequence as the website metrics retriever because they
  // both use the UI thread.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!website_metrics) {
    // This can happen if the `WebsiteMetrics` component initialization
    // failed (for example, component was destructed). We just abort
    // initialization of the event observer when this happens.
    return;
  }
  observer_.Observe(website_metrics);
}

void WebsiteEventsObserver::OnWebsiteMetricsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_.Reset();
}

}  // namespace reporting
