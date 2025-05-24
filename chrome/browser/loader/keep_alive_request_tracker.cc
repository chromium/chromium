// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/keep_alive_request_tracker.h"

#include <stdint.h>

#include <string>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// The maximum number of redirects to be recorded without bucketing.
// This is unlikely to reach as the maximum is 20 per spec
// https://fetch.spec.whatwg.org/#http-redirect-fetch
constexpr size_t kMaxNonBucketedNumRedirects = 20;

ChromeKeepAliveRequestTracker::RequestType ComputeRequestType(
    const network::ResourceRequest& request) {
  switch (request.attribution_reporting_eligibility) {
    case network::mojom::AttributionReportingEligibility::kUnset:
    case network::mojom::AttributionReportingEligibility::kEmpty:
      break;
    case network::mojom::AttributionReportingEligibility::kEventSource:
    case network::mojom::AttributionReportingEligibility::kNavigationSource:
    case network::mojom::AttributionReportingEligibility::kTrigger:
    case network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger:
      return ChromeKeepAliveRequestTracker::RequestType::kAttribution;
  }

  if (request.is_fetch_later_api) {
    return ChromeKeepAliveRequestTracker::RequestType::kFetchLater;
  }

  return ChromeKeepAliveRequestTracker::RequestType::kFetch;
}

}  // namespace

// static
std::unique_ptr<ChromeKeepAliveRequestTracker>
ChromeKeepAliveRequestTracker::MaybeCreateKeepAliveRequestTracker(
    const network::ResourceRequest& request,
    std::optional<ukm::SourceId> ukm_source_id,
    IsContextDetachedCallback is_context_detached_callback) {
  if (!request.keepalive || !request.keepalive_token.has_value()) {
    return nullptr;
  }

  if (!ukm_source_id.has_value()) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(
          page_load_metrics::features::kBeaconLeakageLogging)) {
    return nullptr;
  }

  auto category_id = page_load_metrics::GetCategoryIdFromUrl(request.url);
  if (!category_id.has_value()) {
    return nullptr;
  }

  ChromeKeepAliveRequestTracker::RequestType request_type =
      ComputeRequestType(request);

  return base::WrapUnique(new ChromeKeepAliveRequestTracker(
      request_type, *category_id, *ukm_source_id,
      std::move(is_context_detached_callback), *request.keepalive_token));
}

ChromeKeepAliveRequestTracker::ChromeKeepAliveRequestTracker(
    RequestType request_type,
    uint32_t request_category,
    ukm::SourceId ukm_source_id,
    IsContextDetachedCallback is_context_detached_callback,
    base::UnguessableToken keepalive_token)
    : KeepAliveRequestTracker(request_type),
      created_time_(base::TimeTicks::Now()),
      ukm_builder_(ukm_source_id),
      is_context_detached_callback_(std::move(is_context_detached_callback)) {
  ukm_builder_.SetId_Low(keepalive_token.GetLowForSerialization());
  ukm_builder_.SetId_High(keepalive_token.GetHighForSerialization());
  ukm_builder_.SetRequestType(static_cast<int64_t>(request_type));
  ukm_builder_.SetCategory(request_category);
}

ChromeKeepAliveRequestTracker::~ChromeKeepAliveRequestTracker() {
  // TODO(crbug.com/382527001): Logging in dtor might not work on Android.
  LogUkmEvent();
}

void ChromeKeepAliveRequestTracker::AddStageMetrics(const RequestStage& stage) {
  base::TimeDelta relative_to_created_time =
      base::TimeTicks::Now() - created_time_;

  switch (stage.type) {
    case RequestStageType::kLoaderCreated:
      // kLoaderCreated is the initial stage set in ctor.
      NOTREACHED();

    case RequestStageType::kRequestStarted:
      ukm_builder_.SetTimeDelta_RequestStarted(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kFirstRedirectReceived:
      IncreaseNumRedirects();
      ukm_builder_.SetTimeDelta_FirstRedirectReceived(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kSecondRedirectReceived:
      IncreaseNumRedirects();
      ukm_builder_.SetTimeDelta_SecondRedirectReceived(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kThirdOrLaterRedirectReceived:
      IncreaseNumRedirects();
      ukm_builder_.SetTimeDelta_ThirdOrLaterRedirectReceived(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kResponseReceived:
      ukm_builder_.SetTimeDelta_ResponseReceived(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kRequestFailed:
      ukm_builder_.SetTimeDelta_RequestFailed(
          relative_to_created_time.InMilliseconds());
      ukm_builder_.SetRequestFailed_ErrorCode(stage.status->error_code);
      ukm_builder_.SetRequestFailed_ExtendedErrorCode(
          stage.status->extended_error_code);
      break;

    case RequestStageType::kLoaderDisconnectedFromRenderer:
      ukm_builder_.SetTimeDelta_LoaderDisconnectedFromRenderer(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kRequestCancelledByRenderer:
      ukm_builder_.SetTimeDelta_RequestCancelledByRenderer(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kRequestCancelledAfterTimeLimit:
      ukm_builder_.SetTimeDelta_RequestCancelledAfterTimeLimit(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kBrowserShutdown:
      ukm_builder_.SetTimeDelta_BrowserShutdown(
          relative_to_created_time.InMilliseconds());
      break;

    case RequestStageType::kLoaderCompleted:
      ukm_builder_.SetTimeDelta_LoaderCompleted(
          relative_to_created_time.InMilliseconds());
      ukm_builder_.SetLoaderCompleted_ErrorCode(stage.status->error_code);
      ukm_builder_.SetLoaderCompleted_ExtendedErrorCode(
          stage.status->extended_error_code);
      break;
  }
}

void ChromeKeepAliveRequestTracker::LogUkmEvent() {
  ukm_builder_.SetEndStage(static_cast<int64_t>(GetCurrentStage().type));
  if (auto previous_stage = GetPreviousStage(); previous_stage.has_value()) {
    ukm_builder_.SetPreviousStage(static_cast<int64_t>(previous_stage->type));
  }
  uint32_t num_redirects = GetNumRedirects();
  ukm_builder_.SetNumRedirects(
      num_redirects <= kMaxNonBucketedNumRedirects
          ? num_redirects
          : ukm::GetExponentialBucketMinForCounts1000(num_redirects));
  ukm_builder_.SetIsContextDetached(is_context_detached_callback_.Run());
  ukm_builder_.SetTimeDelta_EventLogged(
      (base::TimeTicks::Now() - created_time_).InMilliseconds());

  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_builder_.Record(ukm_recorder->Get());
}
