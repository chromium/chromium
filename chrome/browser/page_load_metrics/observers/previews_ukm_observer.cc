// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace previews {

PreviewsUKMObserver::PreviewsUKMObserver() = default;

PreviewsUKMObserver::~PreviewsUKMObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnCommit(content::NavigationHandle* navigation_handle,
                              ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  save_data_enabled_ = IsDataSaverEnabled(navigation_handle);
  RecordPreviewsTypes();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnStart(content::NavigationHandle* navigation_handle,
                             const GURL& currently_committed_url,
                             bool started_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_in_foreground)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPreviewsTypes();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsUKMObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPreviewsTypes();
  return STOP_OBSERVING;
}

void PreviewsUKMObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPreviewsTypes();
}

void PreviewsUKMObserver::RecordPreviewsTypes() {
  // Only record previews types when they are active.
  if (!save_data_enabled_) {
    return;
  }

  ukm::builders::Previews builder(GetDelegate().GetPageUkmSourceId());

  if (save_data_enabled_)
    builder.Setsave_data_enabled(1);

  builder.Record(ukm::UkmRecorder::Get());
}

void PreviewsUKMObserver::OnEventOccurred(
    page_load_metrics::PageLoadMetricsEvent event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool PreviewsUKMObserver::IsDataSaverEnabled(
    content::NavigationHandle* navigation_handle) const {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());

  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              profile);
  if (!data_reduction_proxy_settings) {
    DCHECK(profile->IsOffTheRecord());
    return false;
  }

  return data_reduction_proxy_settings->IsDataReductionProxyEnabled();
}

}  // namespace previews
