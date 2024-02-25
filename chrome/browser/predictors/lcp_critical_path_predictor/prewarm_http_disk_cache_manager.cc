// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/prewarm_http_disk_cache_manager.h"

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/base_tracing.h"
#include "base/trace_event/common/trace_event_common.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
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

}  // namespace

PrewarmHttpDiskCacheManager::PrewarmHttpDiskCacheManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)),
      prewarm_history_(
          blink::features::kHttpDiskCachePrewarmingHistorySize.Get()),
      reprewarm_period_(
          blink::features::kHttpDiskCachePrewarmingReprewarmPeriod.Get()),
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
    const GURL& top_frame_main_resource_url,
    const std::vector<GURL>& top_frame_subresource_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW0(
      "loading",
      "PrewarmHttpDiskCacheManager::MaybePrewarmMainResourceAndSubresources",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  url::Origin top_frame_origin =
      url::Origin::Create(top_frame_main_resource_url);
  MaybeAddPrewarmJob(top_frame_origin, top_frame_main_resource_url);
  for (const GURL& url : top_frame_subresource_urls) {
    MaybeAddPrewarmJob(top_frame_origin, url);
  }

  if (!queued_jobs_.empty()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PrewarmHttpDiskCacheManager::MaybeProcessNextQueuedJob,
                       weak_factory_.GetWeakPtr()));
  }
}

void PrewarmHttpDiskCacheManager::MaybeAddPrewarmJob(
    const url::Origin& top_frame_origin,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(url.is_valid() && url.SchemeIsHTTPOrHTTPS());
  TRACE_EVENT_WITH_FLOW1(
      "loading", "PrewarmHttpDiskCacheManager::MaybeAddPrewarmJob",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", url);
  base::TimeTicks now = base::TimeTicks::Now();
  std::pair<url::Origin, GURL> origin_and_url =
      std::make_pair(top_frame_origin, url);
  const auto& it = prewarm_history_.Peek(origin_and_url);
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
  prewarm_history_.Put(std::move(origin_and_url), now);
  queued_jobs_.emplace(top_frame_origin, url);
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
  std::pair<url::Origin, GURL> origin_and_url;
  std::swap(origin_and_url, queued_jobs_.front());
  queued_jobs_.pop();
  const auto& [origin, url] = origin_and_url;
  TRACE_EVENT_WITH_FLOW1(
      "loading", "PrewarmHttpDiskCacheManager::MaybeProcessNextQueuedJob",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "url", url);
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));
  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info = isolation_info;

  // Load only from cache, and don't use cookies.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags =
      net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION;
  request->trusted_params = trusted_params;
  request->site_for_cookies = trusted_params.isolation_info.site_for_cookies();
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->skip_service_worker = true;
  request->do_not_prompt_for_login = true;
  request->priority = net::IDLE;
  CHECK(!request->SendsCookies());
  CHECK(!request->SavesCookies());
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation);
  url_loader_->DownloadAsStream(url_loader_factory_.get(), this);
}

void PrewarmHttpDiskCacheManager::OnDataReceived(std::string_view string_piece,
                                                 base::OnceClosure resume) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW1("loading",
                         "PrewarmHttpDiskCacheManager::OnDataReceived",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "received_data_size", string_piece.size());
  std::move(resume).Run();
}

void PrewarmHttpDiskCacheManager::OnComplete(bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  TRACE_EVENT_WITH_FLOW1("loading", "PrewarmHttpDiskCacheManager::OnComplete",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "success", success);
  url_loader_ = nullptr;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PrewarmHttpDiskCacheManager::MaybeProcessNextQueuedJob,
                     weak_factory_.GetWeakPtr()));
  if (prewarm_finished_callback_for_testing_) {
    std::move(prewarm_finished_callback_for_testing_).Run();
  }
}

void PrewarmHttpDiskCacheManager::OnRetry(base::OnceClosure start_retry) {
  NOTREACHED();
}

}  // namespace predictors
