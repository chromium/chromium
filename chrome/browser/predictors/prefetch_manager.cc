// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/prefetch_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/predictors_switches.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/prefetch/prefetch_headers.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_loader_throttles.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/empty_url_loader_client.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace predictors {

namespace {

const net::NetworkTrafficAnnotationTag kPrefetchTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("predictive_prefetch",
                                        R"(
    semantics {
      sender: "Loading Predictor"
      description:
        "This request is issued near the start of a navigation to "
        "speculatively fetch resources that resulting page is predicted to "
        "request."
      trigger:
        "Navigating Chrome (by clicking on a link, bookmark, history item, "
        "using session restore, etc)."
      data:
        "Arbitrary site-controlled data can be included in the URL."
        "Requests may include cookies and site-specific credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "There are a number of ways to prevent this request:"
        "A) Disable predictive operations under Settings > Advanced > "
        "   Privacy > Preload pages for faster browsing and searching,"
        "B) Disable Lite Mode under Settings > Advanced > Lite mode, or "
        "C) Disable 'Make searches and browsing better' under Settings > "
        "   Sync and Google services > Make searches and browsing better"
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "This feature can be safely disabled, but enabling it may result in "
      "faster page loads. Using either URLBlocklist or URLAllowlist policies "
      "(or a combination of both) limits the scope of these requests."
)");

}  // namespace

// Stores the status of all prefetches associated with a given |url|.
struct PrefetchInfo {
  PrefetchInfo(const GURL& url, PrefetchManager& manager)
      : url(url),
        stats(std::make_unique<PrefetchStats>(url)),
        manager(&manager) {
    DCHECK(url.is_valid());
    DCHECK(url.SchemeIsHTTPOrHTTPS());
  }

  ~PrefetchInfo() = default;

  PrefetchInfo(const PrefetchInfo&) = delete;
  PrefetchInfo& operator=(const PrefetchInfo&) = delete;

  void OnJobCreated() { job_count++; }

  void OnJobDestroyed() {
    job_count--;
    if (is_done()) {
      // Destroys |this|.
      manager->AllPrefetchJobsForUrlFinished(*this);
    }
  }

  bool is_done() const { return job_count == 0; }

  GURL url;
  size_t job_count = 0;
  bool was_canceled = false;
  std::unique_ptr<PrefetchStats> stats;
  // Owns |this|.
  const raw_ptr<PrefetchManager> manager;

  base::WeakPtrFactory<PrefetchInfo> weak_factory{this};
};

// Stores all data need for running a prefetch to a |url|.
struct PrefetchJob {
  PrefetchJob(PrefetchRequest prefetch_request, PrefetchInfo& info)
      : url(prefetch_request.url),
        network_anonymization_key(
            std::move(prefetch_request.network_anonymization_key)),
        destination(prefetch_request.destination),
        creation_time(base::TimeTicks::Now()),
        info(info.weak_factory.GetWeakPtr()) {
    DCHECK(url.is_valid());
    DCHECK(url.SchemeIsHTTPOrHTTPS());
    DCHECK(network_anonymization_key.IsFullyPopulated());
    info.OnJobCreated();
  }

  ~PrefetchJob() {
    if (info)
      info->OnJobDestroyed();
  }

  PrefetchJob(const PrefetchJob&) = delete;
  PrefetchJob& operator=(const PrefetchJob&) = delete;

  GURL url;
  net::NetworkAnonymizationKey network_anonymization_key;
  network::mojom::RequestDestination destination;
  base::TimeTicks creation_time;

  // PrefetchJob lives until the URL load completes, so it can outlive the
  // PrefetchManager and therefore the PrefetchInfo.
  base::WeakPtr<PrefetchInfo> info;
};

PrefetchStats::PrefetchStats(const GURL& url)
    : url(url), start_time(base::TimeTicks::Now()) {}
PrefetchStats::~PrefetchStats() = default;

PrefetchManager::PrefetchManager(base::WeakPtr<Delegate> delegate,
                                 Profile* profile)
    : delegate_(std::move(delegate)), profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
}

PrefetchManager::~PrefetchManager() = default;

void PrefetchManager::Start(const GURL& url,
                            std::vector<PrefetchRequest> requests) {
  DCHECK(base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch));
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefetchInfo* info;
  if (prefetch_info_.find(url) == prefetch_info_.end()) {
    auto iterator_and_whether_inserted =
        prefetch_info_.emplace(url, std::make_unique<PrefetchInfo>(url, *this));
    info = iterator_and_whether_inserted.first->second.get();
  } else {
    info = prefetch_info_.find(url)->second.get();
  }

  for (auto& request : requests) {
    queued_jobs_.emplace_back(
        std::make_unique<PrefetchJob>(std::move(request), *info));
  }

  TryToLaunchPrefetchJobs();
}

void PrefetchManager::Stop(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = prefetch_info_.find(url);
  if (it == prefetch_info_.end())
    return;
  it->second->was_canceled = true;
}

blink::mojom::ResourceType GetResourceType(
    network::mojom::RequestDestination destination) {
  switch (destination) {
    case network::mojom::RequestDestination::kEmpty:
      return blink::mojom::ResourceType::kSubResource;
    case network::mojom::RequestDestination::kScript:
      return blink::mojom::ResourceType::kScript;
    case network::mojom::RequestDestination::kStyle:
      return blink::mojom::ResourceType::kStylesheet;
    case network::mojom::RequestDestination::kFont:
      return blink::mojom::ResourceType::kFontResource;
    default:
      NOTREACHED_IN_MIGRATION() << destination;
  }
  return blink::mojom::ResourceType::kSubResource;
}

void PrefetchManager::PrefetchUrl(
    std::unique_ptr<PrefetchJob> job,
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  DCHECK(job);
  DCHECK(job->info);

  PrefetchInfo& info = *job->info;
  url::Origin top_frame_origin = url::Origin::Create(info.url);

  network::ResourceRequest request;
  request.method = "GET";
  request.url = job->url;
  request.site_for_cookies = net::SiteForCookies::FromUrl(info.url);
  request.request_initiator = top_frame_origin;

  // The prefetch can happen before the referrer policy is known, so use a
  // conservative one (no-referrer) by default.
  request.referrer_policy = net::ReferrerPolicy::NO_REFERRER;

  request.headers.SetHeader("Purpose", "prefetch");
  request.headers.SetHeader(prefetch::headers::kSecPurposeHeaderName,
                            prefetch::headers::kSecPurposePrefetchHeaderValue);

  request.load_flags = net::LOAD_PREFETCH;
  request.destination = job->destination;
  request.resource_type =
      static_cast<int>(GetResourceType(request.destination));

  // TODO(falken): Support CORS?
  request.mode = network::mojom::RequestMode::kNoCors;

  // The hints are only for requests made from the top frame,
  // so frame_origin is the same as top_frame_origin.
  auto frame_origin = top_frame_origin;

  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, top_frame_origin, frame_origin,
      net::SiteForCookies::FromUrl(info.url));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  network::URLLoaderFactoryBuilder factory_builder;
  auto* web_request_api =
      extensions::BrowserContextKeyedAPIFactory<extensions::WebRequestAPI>::Get(
          profile_);
  if (web_request_api) {
    web_request_api->MaybeProxyURLLoaderFactory(
        profile_, /*frame=*/nullptr, /*render_process_id=*/0,
        content::ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
        /*navigation_id=*/std::nullopt, ukm::kInvalidSourceIdObj,
        factory_builder, /*header_client=*/nullptr,
        /*navigation_response_task_runner=*/nullptr,
        /*request_initiator=*/url::Origin());
  }
  factory = std::move(factory_builder).Finish(factory);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Set up throttles. Use null values for frame/navigation-related params, for
  // now, since this is just the browser prefetching resources and the requests
  // don't need to appear to come from a frame.
  // TODO(falken): Clarify the API of CreateURLLoaderThrottles() for prefetching
  // and subresources.
  auto wc_getter =
      base::BindRepeating([]() -> content::WebContents* { return nullptr; });
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      content::CreateContentBrowserURLLoaderThrottles(
          request, profile_, std::move(wc_getter),
          /*navigation_ui_data=*/nullptr, content::FrameTreeNodeId(),
          /*navigation_id=*/std::nullopt);

  auto client = std::make_unique<network::EmptyURLLoaderClient>();

  ++inflight_jobs_count_;

  // Since the CORS-RFC1918 check is skipped when the client security state is
  // unknown, just block any local request to be safe for now.
  int options = base::CommandLine::ForCurrentProcess()->HasSwitch(
                    switches::kLoadingPredictorAllowLocalRequestForTesting)
                    ? network::mojom::kURLLoadOptionNone
                    : network::mojom::kURLLoadOptionBlockLocalRequest;

  if (base::FeatureList::IsEnabled(
          features::kLoadingPredictorPrefetchUseReadAndDiscardBody)) {
    options |= network::mojom::kURLLoadOptionReadAndDiscardBody;
  }

  base::UmaHistogramBoolean("Navigation.Prefetch.IsHttps",
                            request.url.SchemeIsCryptographic());

  std::unique_ptr<blink::ThrottlingURLLoader> loader =
      blink::ThrottlingURLLoader::CreateLoaderAndStart(
          std::move(factory), std::move(throttles),
          content::GlobalRequestID::MakeBrowserInitiated().request_id, options,
          &request, client.get(), kPrefetchTrafficAnnotation,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          /*cors_exempt_header_list=*/std::nullopt);

  delegate_->PrefetchInitiated(info.url, job->url);

  // The idea of prefetching is for the network service to put the response in
  // the http cache. So from the prefetching layer, nothing needs to be done
  // with the response, so just drain it.
  auto* raw_client = client.get();
  raw_client->Drain(base::BindOnce(&PrefetchManager::OnPrefetchFinished,
                                   weak_factory_.GetWeakPtr(), std::move(job),
                                   std::move(loader), std::move(client)));
}

// Some params are unused but bound to this function to keep them alive until
// the load finishes.
void PrefetchManager::OnPrefetchFinished(
    std::unique_ptr<PrefetchJob> job,
    std::unique_ptr<blink::ThrottlingURLLoader> loader,
    std::unique_ptr<network::mojom::URLLoaderClient> client,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefetchInfo& info = *job->info;
  if (observer_for_testing_)
    observer_for_testing_->OnPrefetchFinished(info.url, job->url, status);

  // TODO(ricea): Remove these histograms in October 2024 and make a note of the
  // results in https://crbug.com/335524391.
  if (status.error_code == net::OK && status.decoded_body_length > 0) {
    if (status.decoded_body_length > status.encoded_body_length) {
      // Assume it was compressed.
      base::UmaHistogramCounts10000(
          "Navigation.Prefetch.CompressedBodySize",
          static_cast<int>(status.encoded_body_length / 1024));
    } else {
      // The cast to int will overflow if we prefetch a resource over a terabyte
      // in size, but I'm hoping that will never happen.
      base::UmaHistogramCounts10000(
          "Navigation.Prefetch.UncompressedBodySize",
          static_cast<int>(status.encoded_body_length / 1024));
    }
  }

  // Cannot access the fields of `status` after this point.
  loader.reset();
  client.reset();
  job.reset();

  --inflight_jobs_count_;
  TryToLaunchPrefetchJobs();
}

void PrefetchManager::TryToLaunchPrefetchJobs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We assume that the number of jobs in the queue will be relatively small at
  // any given time. We can revisit this as needed.
  UMA_HISTOGRAM_COUNTS_100("Navigation.Prefetch.PrefetchJobQueueLength",
                           queued_jobs_.size());

  if (queued_jobs_.empty() || inflight_jobs_count_ >= kMaxInflightPrefetches) {
    return;
  }

  // TODO(falken): Is it ok to assume the default partition? Try to plumb the
  // partition here, e.g., from WebContentsObserver. And make a similar change
  // in PreconnectManager.
  content::StoragePartition* storage_partition =
      profile_->GetDefaultStoragePartition();
  scoped_refptr<network::SharedURLLoaderFactory> factory =
      storage_partition->GetURLLoaderFactoryForBrowserProcess();

  while (!queued_jobs_.empty() &&
         inflight_jobs_count_ < kMaxInflightPrefetches) {
    std::unique_ptr<PrefetchJob> job = std::move(queued_jobs_.front());
    queued_jobs_.pop_front();
    base::WeakPtr<PrefetchInfo> info = job->info;
    // |this| owns all infos.
    DCHECK(info);

    // Note: PrefetchJobs are put into |queued_jobs_| immediately on creation,
    // so their creation time is also the time at which they started queueing.
    UMA_HISTOGRAM_TIMES("Navigation.Prefetch.PrefetchJobQueueingTime",
                        base::TimeTicks::Now() - job->creation_time);

    if (job->url.is_valid() && factory && !info->was_canceled)
      PrefetchUrl(std::move(job), factory);
  }
}

void PrefetchManager::AllPrefetchJobsForUrlFinished(PrefetchInfo& info) {
  DCHECK(info.is_done());
  auto it = prefetch_info_.find(info.url);
  CHECK(it != prefetch_info_.end(), base::NotFatalUntil::M130);
  DCHECK(&info == it->second.get());

  if (delegate_)
    delegate_->PrefetchFinished(std::move(info.stats));
  if (observer_for_testing_)
    observer_for_testing_->OnAllPrefetchesFinished(info.url);
  prefetch_info_.erase(it);
}

}  // namespace predictors
