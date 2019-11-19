// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_base.h"

#include <stdint.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "crypto/sha2.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

uint64_t ComputeDataReductionProxyUUID(
    data_reduction_proxy::DataReductionProxyData* data) {
  if (!data || data->session_key().empty() || !data->page_id().has_value() ||
      data->page_id().value() == 0) {
    return 0;
  }

  uint64_t page_id = data->page_id().value();
  char buf[8];
  base::WriteBigEndian<uint64_t>(buf, page_id);

  std::string to_hash(data->session_key());
  to_hash.append(std::begin(buf), std::end(buf));

  char hash[32];
  crypto::SHA256HashString(base::StringPiece(to_hash), hash, 32);

  uint64_t uuid;
  base::ReadBigEndian<uint64_t>(hash, &uuid);
  return uuid;
}

}  // namespace

DataReductionProxyMetricsObserverBase::DataReductionProxyMetricsObserverBase()
    : browser_context_(nullptr),
      opted_out_(false),
      num_data_reduction_proxy_resources_(0),
      num_network_resources_(0),
      insecure_original_network_bytes_(0),
      secure_original_network_bytes_(0),
      network_bytes_proxied_(0),
      insecure_network_bytes_(0),
      secure_network_bytes_(0),
      insecure_cached_bytes_(0),
      secure_cached_bytes_(0) {}

DataReductionProxyMetricsObserverBase::
    ~DataReductionProxyMetricsObserverBase() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This BrowserContext is valid for the lifetime of
  // DataReductionProxyMetricsObserverBase. BrowserContext is always valid and
  // non-nullptr in NavigationControllerImpl, which is a member of WebContents.
  // A raw pointer to BrowserContext taken at this point will be valid until
  // after WebContent's destructor. The latest that PageLoadTracker's destructor
  // will be called is in MetricsWebContentsObserver's destrcutor, which is
  // called in WebContents destructor.
  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();

  return OnCommitCalled(navigation_handle, source_id);
}

// Check if the NavigationData indicates anything about the DataReductionProxy.
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::OnCommitCalled(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<DataReductionProxyData> data;
  auto* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context_);
  if (settings) {
    data = settings->CreateDataFromNavigationHandle(
        navigation_handle, navigation_handle->GetResponseHeaders());
  }
  if (!data || !(data->used_data_reduction_proxy() ||
                 data->was_cached_data_reduction_proxy_response())) {
    return STOP_OBSERVING;
  }
  data_ = std::move(data);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  previews::PreviewsUserData* previews_data = nullptr;

  if (ui_tab_helper)
    previews_data = ui_tab_helper->GetPreviewsUserData(navigation_handle);

  if (previews_data) {
    data_->set_black_listed(previews_data->black_listed_for_lite_page());
  }

  // DataReductionProxy page loads should only occur on HTTP navigations.
  DCHECK(!navigation_handle->GetURL().SchemeIsCryptographic());
  DCHECK_EQ(data_->request_url(), navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_in_foreground)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserverBase::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we record UKM with data collected up to this point.
  if (GetDelegate().DidCommit()) {
    RecordUKM();
  }
  return STOP_OBSERVING;
}

void DataReductionProxyMetricsObserverBase::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordUKM();
}

// static
int64_t DataReductionProxyMetricsObserverBase::ExponentiallyBucketBytes(
    int64_t bytes) {
  const int64_t start_buckets = 10000;
  if (bytes < start_buckets) {
    return 0;
  }
  return ukm::GetExponentialBucketMin(bytes, 1.16);
}

void DataReductionProxyMetricsObserverBase::RecordUKM() const {
  if (!data())
    return;

  if (!data()->used_data_reduction_proxy())
    return;

  int64_t original_network_bytes =
      insecure_original_network_bytes() + secure_original_network_bytes();
  uint64_t uuid = ComputeDataReductionProxyUUID(data());

  ukm::builders::DataReductionProxy builder(GetDelegate().GetSourceId());

  builder.SetEstimatedOriginalNetworkBytes(
      ExponentiallyBucketBytes(original_network_bytes));
  builder.SetDataSaverPageUUID(uuid);

  builder.Record(ukm::UkmRecorder::Get());
}

void DataReductionProxyMetricsObserverBase::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto const& resource : resources) {
    if (resource->cache_type == page_load_metrics::mojom::CacheType::kMemory)
      continue;
    if (resource->cache_type !=
        page_load_metrics::mojom::CacheType::kNotCached) {
      if (resource->is_complete) {
        if (resource->is_secure_scheme) {
          secure_cached_bytes_ += resource->encoded_body_length;
        } else {
          insecure_cached_bytes_ += resource->encoded_body_length;
        }
      }
      continue;
    }
    int64_t original_network_bytes =
        resource->delta_bytes *
        resource->data_reduction_proxy_compression_ratio_estimate;
    if (resource->is_secure_scheme) {
      secure_original_network_bytes_ += original_network_bytes;
      secure_network_bytes_ += resource->delta_bytes;
    } else {
      insecure_original_network_bytes_ += original_network_bytes;
      insecure_network_bytes_ += resource->delta_bytes;
    }
    if (resource->is_complete)
      num_network_resources_++;
    // If the request is proxied on a page with data saver proxy for the main
    // frame request, then it is very likely a data saver proxy for this
    // request.
    if (resource->proxy_used) {
      if (resource->is_complete)
        num_data_reduction_proxy_resources_++;
      // Proxied bytes are always non-secure.
      network_bytes_proxied_ += resource->delta_bytes;
    }
  }
}

void DataReductionProxyMetricsObserverBase::OnEventOccurred(
    const void* const event_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event_key == PreviewsUITabHelper::OptOutEventKey())
    opted_out_ = true;
}

}  // namespace data_reduction_proxy
