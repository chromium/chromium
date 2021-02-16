// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/ad_metrics/floc_page_load_metrics_observer.h"

#include "chrome/browser/federated_learning/floc_eligibility_observer.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

FlocPageLoadMetricsObserver::FlocPageLoadMetricsObserver() = default;

FlocPageLoadMetricsObserver::~FlocPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
FlocPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  federated_learning::FlocIdProvider* floc_id_provider =
      federated_learning::FlocIdProviderFactory::GetForProfile(
          Profile::FromBrowserContext(
              GetDelegate().GetWebContents()->GetBrowserContext()));

  if (floc_id_provider)
    floc_id_provider->MaybeRecordFlocToUkm(source_id);

  return federated_learning::FlocEligibilityObserver::
      GetOrCreateForCurrentDocument(navigation_handle->GetRenderFrameHost())
          ->OnCommit(navigation_handle);
}

void FlocPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  bool any_ads_resource = base::ranges::any_of(
      resources,
      [](const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {
        return resource->reported_as_ad_resource &&
               resource->received_data_length > 0;
      });

  if (any_ads_resource) {
    content::WebContents* web_contents = GetDelegate().GetWebContents();
    federated_learning::FlocEligibilityObserver::GetOrCreateForCurrentDocument(
        web_contents->GetMainFrame())
        ->OnAdResource();
  }
}
