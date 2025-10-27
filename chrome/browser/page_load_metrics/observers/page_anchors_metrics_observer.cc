// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"

#include "base/debug/dump_without_crashing.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_metrics_document_data.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

void PageAnchorsMetricsObserver::RecordAnchorElementMetricsDataToUkm() {
  content::RenderFrameHost* rfh = render_frame_host();
  if (!rfh) {
    return;
  }
  NavigationPredictorMetricsDocumentData* data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          rfh);
  CHECK(data);
  data->RecordAnchorElementMetricsData(ukm_source_id_);
}

void PageAnchorsMetricsObserver::RecordDataToUkm(bool reset_source) {
  // `AnchorElementMetricsData` are already recorded to UKM as we receive them,
  // and we don't need to record them again here. The edge case scenario is
  // handled separately in `OnRestoreFromBackForwardCache`.
  content::RenderFrameHost* rfh = render_frame_host();
  if (!rfh) {
    return;
  }
  NavigationPredictorMetricsDocumentData* data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          rfh);
  CHECK(data);
  data->RecordDataToUkm(ukm_source_id_);
  if (reset_source) {
    data->ResetUkmSourceId();
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  is_in_prerendered_page_ = true;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in the primary page's end of life timing,
  // and doesn't need to continue observing FencedFrame pages.
  return STOP_OBSERVING;
}

void PageAnchorsMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming&) {
  // Do not report Ukm while prerendering.
  if (is_in_prerendered_page_) {
    return;
  }
  // Resetting the source so that no more data is recorded to UKM when
  // OnComplete is shortly followed by
  // ~NavigationPredictorMetricsDocumentData(). The source can be set again,
  // namely, when a RFH is restored from the BFCache.
  RecordDataToUkm(/*reset_source=*/true);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming&) {
  // Do not report Ukm while prerendering.
  if (is_in_prerendered_page_) {
    return CONTINUE_OBSERVING;
  }

  RecordDataToUkm(/*reset_source=*/false);
  return STOP_OBSERVING;
}

void PageAnchorsMetricsObserver::UpdateRenderFrameHostAndSourceId(
    content::NavigationHandle* navigation_handle) {
  render_frame_host_id_ =
      navigation_handle->GetRenderFrameHost()->GetGlobalId();
  ukm_source_id_ = ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                          ukm::SourceIdType::NAVIGATION_ID);

  NavigationPredictorMetricsDocumentData* data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          render_frame_host());
  CHECK(data);
  data->SetUkmSourceId(ukm_source_id_);
  data->SetNavigationStartTime(GetDelegate().GetNavigationStart());
}

void PageAnchorsMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  DCHECK(is_in_prerendered_page_);
  is_in_prerendered_page_ = false;
  UpdateRenderFrameHostAndSourceId(navigation_handle);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  UpdateRenderFrameHostAndSourceId(navigation_handle);
  return CONTINUE_OBSERVING;
}

void PageAnchorsMetricsObserver::OnRestoreFromBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    content::NavigationHandle* navigation_handle) {
  UpdateRenderFrameHostAndSourceId(navigation_handle);
  RecordAnchorElementMetricsDataToUkm();
}

void PageAnchorsMetricsObserver::OnRenderFrameDeleted(
    content::RenderFrameHost* rfh) {
  // OnRenderFrameDeleted is called when RenderFrameHost for a frame is deleted.
  // Including the sub-frames.
  if (render_frame_host() == rfh) {
    if (!is_in_prerendered_page_) {
      // Resetting the source so that data is not recorded to UKM again when the
      // RenderFrameHost is destroyed.
      RecordDataToUkm(/*reset_source=*/true);
    }
    render_frame_host_id_.reset();
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageAnchorsMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming&) {
  if (is_in_prerendered_page_) {
    return CONTINUE_OBSERVING;
  }

  RecordDataToUkm(/*reset_source=*/false);
  return CONTINUE_OBSERVING;
}
