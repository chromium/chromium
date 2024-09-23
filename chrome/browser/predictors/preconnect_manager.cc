// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/preconnect_manager.h"

#include <utility>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace predictors {

const bool kAllowCredentialsOnPreconnectByDefault = true;

PreconnectedRequestStats::PreconnectedRequestStats(const url::Origin& origin,
                                                   bool was_preconnected)
    : origin(origin), was_preconnected(was_preconnected) {}

PreconnectedRequestStats::PreconnectedRequestStats(
    const PreconnectedRequestStats& other) = default;
PreconnectedRequestStats::~PreconnectedRequestStats() = default;

PreconnectStats::PreconnectStats(const GURL& url)
    : url(url), start_time(base::TimeTicks::Now()) {}
PreconnectStats::~PreconnectStats() = default;

PreresolveInfo::PreresolveInfo(const GURL& url, size_t count)
    : url(url),
      queued_count(count),
      inflight_count(0),
      was_canceled(false),
      stats(std::make_unique<PreconnectStats>(url)) {}

PreresolveInfo::~PreresolveInfo() = default;

PreresolveJob::PreresolveJob(
    const GURL& url,
    int num_sockets,
    bool allow_credentials,
    net::NetworkAnonymizationKey network_anonymization_key,
    PreresolveInfo* info)
    : url(url),
      num_sockets(num_sockets),
      allow_credentials(allow_credentials),
      network_anonymization_key(std::move(network_anonymization_key)),
      info(info),
      creation_time(base::TimeTicks::Now()) {
  DCHECK_GE(num_sockets, 0);
  DCHECK(!this->network_anonymization_key.IsEmpty());
}

PreresolveJob::PreresolveJob(PreconnectRequest preconnect_request,
                             PreresolveInfo* info)
    : PreresolveJob(preconnect_request.origin.GetURL(),
                    preconnect_request.num_sockets,
                    preconnect_request.allow_credentials,
                    std::move(preconnect_request.network_anonymization_key),
                    info) {}

PreresolveJob::PreresolveJob(PreresolveJob&& other) = default;
PreresolveJob::~PreresolveJob() = default;

PreconnectManager::PreconnectManager(base::WeakPtr<Delegate> delegate,
                                     content::BrowserContext* browser_context)
    : delegate_(std::move(delegate)),
      browser_context_(browser_context),
      inflight_preresolves_count_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(browser_context_);
}

PreconnectManager::~PreconnectManager() = default;

bool PreconnectManager::IsEnabled() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile) {
    return false;
  }
  return prefetch::IsSomePreloadingEnabled(*profile->GetPrefs()) ==
         content::PreloadingEligibility::kEligible;
}

void PreconnectManager::Start(const GURL& url,
                              std::vector<PreconnectRequest> requests) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsEnabled())
    return;
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  PreresolveInfo* info;
  if (preresolve_info_.find(url) == preresolve_info_.end()) {
    auto iterator_and_whether_inserted = preresolve_info_.emplace(
        url, std::make_unique<PreresolveInfo>(url, requests.size()));
    info = iterator_and_whether_inserted.first->second.get();
  } else {
    info = preresolve_info_.find(url)->second.get();
    info->queued_count += requests.size();
  }

  for (auto& request : requests) {
    PreresolveJobId job_id = preresolve_jobs_.Add(
        std::make_unique<PreresolveJob>(std::move(request), info));
    queued_jobs_.push_back(job_id);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreresolveHost(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsEnabled())
    return;
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  PreresolveJobId job_id = preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
      url.DeprecatedGetOriginAsURL(), 0, kAllowCredentialsOnPreconnectByDefault,
      network_anonymization_key, nullptr));
  queued_jobs_.push_front(job_id);

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreresolveHosts(
    const std::vector<GURL>& urls,
    const net::NetworkAnonymizationKey& network_anonymization_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsEnabled())
    return;
  // Push jobs in front of the queue due to higher priority.
  for (const GURL& url : base::Reversed(urls)) {
    if (!url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    PreresolveJobId job_id = preresolve_jobs_.Add(
        std::make_unique<PreresolveJob>(url.DeprecatedGetOriginAsURL(), 0,
                                        kAllowCredentialsOnPreconnectByDefault,
                                        network_anonymization_key, nullptr));
    queued_jobs_.push_front(job_id);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreconnectUrl(
    const GURL& url,
    bool allow_credentials,
    net::NetworkAnonymizationKey network_anonymization_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsEnabled())
    return;
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  PreresolveJobId job_id = preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
      url.DeprecatedGetOriginAsURL(), 1, allow_credentials,
      std::move(network_anonymization_key), nullptr));
  queued_jobs_.push_front(job_id);

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::Stop(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = preresolve_info_.find(url);
  if (it == preresolve_info_.end()) {
    return;
  }

  it->second->was_canceled = true;
}

void PreconnectManager::PreconnectUrl(
    const GURL& url,
    int num_sockets,
    bool allow_credentials,
    const net::NetworkAnonymizationKey& network_anonymization_key) const {
  DCHECK(url.DeprecatedGetOriginAsURL() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  if (observer_)
    observer_->OnPreconnectUrl(url, num_sockets, allow_credentials);

  auto* network_context = GetNetworkContext();

  if (num_sockets > 1 &&
      base::FeatureList::IsEnabled(
          features::kLoadingPredictorLimitPreconnectSocketCount)) {
    // Adjust the number of socket here because LoadingPredictor is the only
    // call site that sets `num_sockets` to a non-one value.
    num_sockets = 1;
  }

  network_context->PreconnectSockets(
      num_sockets, url,
      allow_credentials ? network::mojom::CredentialsMode::kInclude
                        : network::mojom::CredentialsMode::kOmit,
      network_anonymization_key);
}

std::unique_ptr<ResolveHostClientImpl> PreconnectManager::PreresolveUrl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    ResolveHostCallback callback) const {
  DCHECK(url.DeprecatedGetOriginAsURL() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  auto* network_context = GetNetworkContext();

  return std::make_unique<ResolveHostClientImpl>(
      url, network_anonymization_key, std::move(callback), network_context);
}

std::unique_ptr<ProxyLookupClientImpl> PreconnectManager::LookupProxyForUrl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    ProxyLookupCallback callback) const {
  DCHECK(url.DeprecatedGetOriginAsURL() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  auto* network_context = GetNetworkContext();

  return std::make_unique<ProxyLookupClientImpl>(
      url, network_anonymization_key, std::move(callback), network_context);
}

void PreconnectManager::TryToLaunchPreresolveJobs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We assume that the number of jobs in the queue will be relatively small at
  // any given time. We can revisit this as needed.
  UMA_HISTOGRAM_COUNTS_100("Navigation.Preconnect.PreresolveJobQueueLength",
                           queued_jobs_.size());

  while (!queued_jobs_.empty() &&
         inflight_preresolves_count_ < kMaxInflightPreresolves) {
    auto job_id = queued_jobs_.front();
    queued_jobs_.pop_front();
    PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
    DCHECK(job);

    // Note: PreresolveJobs are put into |queued_jobs_| immediately on creation,
    // so their creation time is also the time at which they started queueing.
    UMA_HISTOGRAM_TIMES("Navigation.Preconnect.PreresolveJobQueueingTime",
                        base::TimeTicks::Now() - job->creation_time);

    PreresolveInfo* info = job->info;

    if (!(info && info->was_canceled)) {
      // This is used to avoid issuing DNS requests when a fixed proxy
      // configuration is in place, which improves efficiency, and is also
      // important if the unproxied DNS may contain incorrect entries.
      job->proxy_lookup_client = LookupProxyForUrl(
          job->url, job->network_anonymization_key,
          base::BindOnce(&PreconnectManager::OnProxyLookupFinished,
                         weak_factory_.GetWeakPtr(), job_id));
      if (info) {
        ++info->inflight_count;
        if (delegate_)
          delegate_->PreconnectInitiated(info->url, job->url);
      }
      ++inflight_preresolves_count_;
    } else {
      preresolve_jobs_.Remove(job_id);
    }

    if (info) {
      DCHECK_LE(1u, info->queued_count);
      --info->queued_count;
      if (info->is_done()) {
        AllPreresolvesForUrlFinished(info);
      }
    }
  }
}

void PreconnectManager::OnPreresolveFinished(PreresolveJobId job_id,
                                             bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
  DCHECK(job);

  if (observer_)
    observer_->OnPreresolveFinished(job->url, job->network_anonymization_key,
                                    success);

  job->resolve_host_client = nullptr;
  FinishPreresolveJob(job_id, success);
}

void PreconnectManager::OnProxyLookupFinished(PreresolveJobId job_id,
                                              bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
  DCHECK(job);

  if (observer_) {
    observer_->OnProxyLookupFinished(job->url, job->network_anonymization_key,
                                     success);
  }

  job->proxy_lookup_client = nullptr;
  if (success) {
    FinishPreresolveJob(job_id, success);
  } else {
    job->resolve_host_client =
        PreresolveUrl(job->url, job->network_anonymization_key,
                      base::BindOnce(&PreconnectManager::OnPreresolveFinished,
                                     weak_factory_.GetWeakPtr(), job_id));
  }
}

void PreconnectManager::FinishPreresolveJob(PreresolveJobId job_id,
                                            bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
  DCHECK(job);

  bool need_preconnect = success && job->need_preconnect();
  if (need_preconnect) {
    PreconnectUrl(job->url, job->num_sockets, job->allow_credentials,
                  job->network_anonymization_key);
  }

  PreresolveInfo* info = job->info;
  if (info) {
    info->stats->requests_stats.emplace_back(url::Origin::Create(job->url),
                                             need_preconnect);
  }
  preresolve_jobs_.Remove(job_id);
  --inflight_preresolves_count_;
  if (info) {
    DCHECK_LE(1u, info->inflight_count);
    --info->inflight_count;
  }
  if (info && info->is_done())
    AllPreresolvesForUrlFinished(info);
  TryToLaunchPreresolveJobs();
}

void PreconnectManager::AllPreresolvesForUrlFinished(PreresolveInfo* info) {
  DCHECK(info);
  DCHECK(info->is_done());
  auto it = preresolve_info_.find(info->url);
  CHECK(it != preresolve_info_.end(), base::NotFatalUntil::M130);
  DCHECK(info == it->second.get());
  if (delegate_)
    delegate_->PreconnectFinished(std::move(info->stats));
  preresolve_info_.erase(it);
}

network::mojom::NetworkContext* PreconnectManager::GetNetworkContext() const {
  if (network_context_)
    return network_context_;

  auto* network_context =
      browser_context_->GetDefaultStoragePartition()->GetNetworkContext();
  DCHECK(network_context);
  return network_context;
}

}  // namespace predictors
