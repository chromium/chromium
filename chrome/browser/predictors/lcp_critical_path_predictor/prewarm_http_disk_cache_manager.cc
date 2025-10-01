// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/prewarm_http_disk_cache_manager.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"

namespace predictors {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("prewarm_http_disk_cache",
                                        R"(
    semantics {
      sender: "HTTPDiskCachePrewarming"
      description:
        "This is not actually a network request. It only warms up "
        "the HTTP disk cache and does not go to network."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      user_data {
        type: NONE
      }
      data: "None. The request does not go to network."
      destination: LOCAL
      internal {
        contacts {
          email: "chrome-loading@google.com"
        }
      }
      last_reviewed: "2023-12-13"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This is not controlled by a setting."
      policy_exception_justification: "This is not a network request."
  })");

bool IsSameSite(const GURL& url1, const GURL& url2) {
  return url1.SchemeIs(url2.GetScheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}
}  // namespace

PrewarmHttpDiskCacheManager::PrewarmHttpDiskCacheManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      prewarm_history_(
          blink::features::kHttpDiskCachePrewarmingHistorySize.Get()),
      reprewarm_period_(
          blink::features::kHttpDiskCachePrewarmingReprewarmPeriod.Get()),
      use_read_and_discard_body_option_(
          blink::features::kHttpDiskCachePrewarmingUseReadAndDiscardBodyOption
              .Get()),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW0(
      "loading", "PrewarmHttpDiskCacheManager::PrewarmHttpDiskCacheManager",
      TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);
}

PrewarmHttpDiskCacheManager::~PrewarmHttpDiskCacheManager() {
  TRACE_EVENT_WITH_FLOW0(
      "loading", "PrewarmHttpDiskCacheManager::~PrewarmHttpDiskCacheManager",
      TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
}

void PrewarmHttpDiskCacheManager::MaybePrewarmResources(
    const std::optional<url::Origin>& initiator_origin,
    const GURL& top_frame_main_resource_url,
    const std::vector<GURL>& top_frame_subresource_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW0(
      "loading",
      "PrewarmHttpDiskCacheManager::MaybePrewarmMainResourceAndSubresources",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // Avoid making network service to be more busy during the browser startup.
  const bool skip_during_browser_startup =
      blink::features::kHttpDiskCachePrewarmingSkipDuringBrowserStartup.Get();
  if (skip_during_browser_startup &&
      !AfterStartupTaskUtils::IsBrowserStartupComplete()) {
    return;
  }

  static const bool prewarm_main_resource =
      base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kHttpDiskCachePrewarming,
          "http_disk_cache_prewarming_main_resource", true);
  url::Origin top_frame_origin =
      url::Origin::Create(top_frame_main_resource_url);
  if (prewarm_main_resource) {
    MaybeAddPrewarmJob({initiator_origin, top_frame_origin,
                        top_frame_main_resource_url,
                        net::IsolationInfo::RequestType::kMainFrame});
  }
  for (const GURL& url : top_frame_subresource_urls) {
    MaybeAddPrewarmJob({top_frame_origin, top_frame_origin, url,
                        net::IsolationInfo::RequestType::kOther});
  }

  if (!queued_jobs_.empty()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PrewarmHttpDiskCacheManager::MaybeProcessNextQueuedJob,
                       weak_factory_.GetWeakPtr()));
  }

  base::UmaHistogramCounts100("Blink.LCPP.PrewarmHttpDiskCacheURL.Count",
                              top_frame_subresource_urls.size());
  if (!top_frame_subresource_urls.empty()) {
    size_t subresource_urls_same_site = 0;
    size_t subresource_urls_cross_site = 0;
    for (const GURL& url : top_frame_subresource_urls) {
      if (IsSameSite(url, top_frame_main_resource_url)) {
        ++subresource_urls_same_site;
      } else {
        ++subresource_urls_cross_site;
      }
    }
    base::UmaHistogramCounts10000(
        "Blink.LCPP.PrewarmHttpDiskCacheURL.Count.SameSite",
        base::checked_cast<int>(subresource_urls_same_site));
    base::UmaHistogramCounts10000(
        "Blink.LCPP.PrewarmHttpDiskCacheURL.Count.CrossSite",
        base::checked_cast<int>(subresource_urls_cross_site));
    base::UmaHistogramPercentage(
        "Blink.LCPP.PrewarmHttpDiskCacheURL.Count.SameSiteRatio",
        base::checked_cast<int>(100 * subresource_urls_same_site /
                                top_frame_subresource_urls.size()));
  }
}

PrewarmHttpDiskCacheManager::PrewarmJob::PrewarmJob() = default;

PrewarmHttpDiskCacheManager::PrewarmJob::PrewarmJob(
    std::optional<url::Origin> initiator_origin,
    url::Origin top_frame_origin,
    GURL url,
    net::IsolationInfo::RequestType request_type)
    : initiator_origin(std::move(initiator_origin)),
      top_frame_origin(std::move(top_frame_origin)),
      url(std::move(url)),
      request_type(request_type) {}

PrewarmHttpDiskCacheManager::PrewarmJob::PrewarmJob(PrewarmJob&&) = default;

PrewarmHttpDiskCacheManager::PrewarmJob&
PrewarmHttpDiskCacheManager::PrewarmJob::operator=(PrewarmJob&&) = default;

PrewarmHttpDiskCacheManager::PrewarmJob::PrewarmJob(const PrewarmJob&) =
    default;

PrewarmHttpDiskCacheManager::PrewarmJob&
PrewarmHttpDiskCacheManager::PrewarmJob::operator=(const PrewarmJob&) = default;

PrewarmHttpDiskCacheManager::PrewarmJob::~PrewarmJob() = default;

bool PrewarmHttpDiskCacheManager::PrewarmJob::operator==(
    const PrewarmJob&) const = default;

auto PrewarmHttpDiskCacheManager::PrewarmJob::operator<=>(
    const PrewarmJob&) const = default;

void PrewarmHttpDiskCacheManager::MaybeAddPrewarmJob(
    const PrewarmJob& prewarm_job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(prewarm_job.url.is_valid() && prewarm_job.url.SchemeIsHTTPOrHTTPS());
  TRACE_EVENT_WITH_FLOW1("loading",
                         "PrewarmHttpDiskCacheManager::MaybeAddPrewarmJob",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "url", prewarm_job.url);
  base::TimeTicks now = base::TimeTicks::Now();
  const auto& it = prewarm_history_.Peek(prewarm_job);
  if (it != prewarm_history_.end() && now - it->second < reprewarm_period_) {
    // Already processed recently.
    TRACE_EVENT_INSTANT1("loading",
                         "PrewarmHttpDiskCacheManager::MaybeAddPrewarmJob"
                         ".AlreadyPrewarmedRecently",
                         TRACE_EVENT_SCOPE_THREAD,
                         "duration_from_previous_prewarming_in_seconds",
                         (now - it->second).InSeconds());
    return;
  }
  queued_jobs_.push(prewarm_job);
  prewarm_history_.Put(prewarm_job, now);
}

void PrewarmHttpDiskCacheManager::MaybeProcessNextQueuedJob() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (queued_jobs_.empty()) {
    return;
  }
  if (url_loader_) {
    // There is an in-flight url loader. After the in-flight url loader
    // complets, OnComplete() is called, and
    // MaybeProcessNextQueuedJob() is called again.
    return;
  }
  PrewarmJob prewarm_job;
  std::swap(prewarm_job, queued_jobs_.front());
  queued_jobs_.pop();
  TRACE_EVENT_WITH_FLOW1(
      "loading", "PrewarmHttpDiskCacheManager::MaybeProcessNextQueuedJob",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url",
      prewarm_job.url);

  // Load only from cache, and don't use cookies.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = prewarm_job.url;
  request->load_flags =
      net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
  if (base::FeatureList::IsEnabled(
          blink::features::kAvoidTrustedParamsCopies)) {
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info = net::IsolationInfo::Create(
        prewarm_job.request_type, prewarm_job.top_frame_origin,
        prewarm_job.top_frame_origin,
        net::SiteForCookies::FromOrigin(prewarm_job.top_frame_origin));
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();
  } else {
    net::IsolationInfo isolation_info = net::IsolationInfo::Create(
        prewarm_job.request_type, prewarm_job.top_frame_origin,
        prewarm_job.top_frame_origin,
        net::SiteForCookies::FromOrigin(prewarm_job.top_frame_origin));
    network::ResourceRequest::TrustedParams trusted_params;
    trusted_params.isolation_info = isolation_info;
    request->trusted_params = trusted_params;
    request->site_for_cookies =
        trusted_params.isolation_info.site_for_cookies();
  }
  request->request_initiator = std::move(prewarm_job.initiator_origin);
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->skip_service_worker = true;
  request->do_not_prompt_for_login = true;
  request->priority = net::IDLE;
  CHECK(!request->SendsCookies());
  CHECK(!request->SavesCookies());
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  if (use_read_and_discard_body_option_) {
    url_loader_->DownloadHeadersOnly(
        url_loader_factory_.get(),
        base::BindOnce(&PrewarmHttpDiskCacheManager::OnHeadersOnly,
                       weak_factory_.GetWeakPtr()));
  } else {
    url_loader_->DownloadAsStream(url_loader_factory_.get(), this);
  }
}

void PrewarmHttpDiskCacheManager::OnDataReceived(std::string_view string_piece,
                                                 base::OnceClosure resume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW1("loading",
                         "PrewarmHttpDiskCacheManager::OnDataReceived",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "received_data_size", string_piece.size());
  CHECK(!use_read_and_discard_body_option_);
  std::move(resume).Run();
}

void PrewarmHttpDiskCacheManager::OnComplete(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW1("loading", "PrewarmHttpDiskCacheManager::OnComplete",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "success", success);
  CHECK(!use_read_and_discard_body_option_);
  base::UmaHistogramBoolean(
      "Blink.LCPP.PrewarmHttpDiskCache.DownloadBody.CacheExists", success);
  DoComplete();
}

void PrewarmHttpDiskCacheManager::OnRetry(base::OnceClosure start_retry) {
  NOTREACHED();
}

void PrewarmHttpDiskCacheManager::OnHeadersOnly(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW0("loading",
                         "PrewarmHttpDiskCacheManager::OnHeadersOnly",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK(use_read_and_discard_body_option_);
  base::UmaHistogramBoolean(
      "Blink.LCPP.PrewarmHttpDiskCache.HeadersOnly.CacheExists", bool(headers));
  DoComplete();
}

void PrewarmHttpDiskCacheManager::DoComplete() {
  url_loader_ = nullptr;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PrewarmHttpDiskCacheManager::MaybeProcessNextQueuedJob,
                     weak_factory_.GetWeakPtr()));
  if (prewarm_finished_callback_for_testing_) {
    std::move(prewarm_finished_callback_for_testing_).Run();
  }
}

}  // namespace predictors
