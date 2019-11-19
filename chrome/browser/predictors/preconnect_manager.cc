// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/preconnect_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/profiles/profile.h"
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

PreresolveJob::PreresolveJob(const GURL& url,
                             int num_sockets,
                             bool allow_credentials,
                             net::NetworkIsolationKey network_isolation_key,
                             PreresolveInfo* info)
    : url(url),
      num_sockets(num_sockets),
      allow_credentials(allow_credentials),
      network_isolation_key(std::move(network_isolation_key)),
      info(info) {
  DCHECK_GE(num_sockets, 0);
}

PreresolveJob::PreresolveJob(PreconnectRequest preconnect_request,
                             PreresolveInfo* info)
    : url(preconnect_request.origin.GetURL()),
      num_sockets(preconnect_request.num_sockets),
      allow_credentials(preconnect_request.allow_credentials),
      network_isolation_key(
          std::move(preconnect_request.network_isolation_key)),
      info(info) {
  DCHECK_GE(num_sockets, 0);
}

PreresolveJob::PreresolveJob(PreresolveJob&& other) = default;
PreresolveJob::~PreresolveJob() = default;

PreconnectManager::PreconnectManager(base::WeakPtr<Delegate> delegate,
                                     Profile* profile)
    : delegate_(std::move(delegate)),
      profile_(profile),
      inflight_preresolves_count_(0) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
}

PreconnectManager::~PreconnectManager() = default;

void PreconnectManager::Start(const GURL& url,
                              std::vector<PreconnectRequest> requests) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const std::string host = url.host();
  if (preresolve_info_.find(host) != preresolve_info_.end())
    return;

  auto iterator_and_whether_inserted = preresolve_info_.emplace(
      host, std::make_unique<PreresolveInfo>(url, requests.size()));
  PreresolveInfo* info = iterator_and_whether_inserted.first->second.get();

  for (auto request_it = requests.begin(); request_it != requests.end();
       ++request_it) {
    PreresolveJobId job_id = preresolve_jobs_.Add(
        std::make_unique<PreresolveJob>(std::move(*request_it), info));
    queued_jobs_.push_back(job_id);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreresolveHost(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  PreresolveJobId job_id = preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
      url.GetOrigin(), 0, kAllowCredentialsOnPreconnectByDefault,
      net::NetworkIsolationKey(), nullptr));
  queued_jobs_.push_front(job_id);

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreresolveHosts(
    const std::vector<std::string>& hostnames) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Push jobs in front of the queue due to higher priority.
  for (auto it = hostnames.rbegin(); it != hostnames.rend(); ++it) {
    PreresolveJobId job_id =
        preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
            GURL("http://" + *it), 0, kAllowCredentialsOnPreconnectByDefault,
            net::NetworkIsolationKey(), nullptr));
    queued_jobs_.push_front(job_id);
  }

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::StartPreconnectUrl(
    const GURL& url,
    bool allow_credentials,
    net::NetworkIsolationKey network_isolation_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  PreresolveJobId job_id = preresolve_jobs_.Add(std::make_unique<PreresolveJob>(
      url.GetOrigin(), 1, allow_credentials, std::move(network_isolation_key),
      nullptr));
  queued_jobs_.push_front(job_id);

  TryToLaunchPreresolveJobs();
}

void PreconnectManager::Stop(const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = preresolve_info_.find(url.host());
  if (it == preresolve_info_.end()) {
    return;
  }

  it->second->was_canceled = true;
}

void PreconnectManager::PreconnectUrl(
    const GURL& url,
    int num_sockets,
    bool allow_credentials,
    const net::NetworkIsolationKey& network_isolation_key) const {
  DCHECK(url.GetOrigin() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  if (observer_)
    observer_->OnPreconnectUrl(url, num_sockets, allow_credentials);

  auto* network_context = GetNetworkContext();
  if (!network_context)
    return;

  network_context->PreconnectSockets(num_sockets, url, allow_credentials,
                                     network_isolation_key);
}

std::unique_ptr<ResolveHostClientImpl> PreconnectManager::PreresolveUrl(
    const GURL& url,
    ResolveHostCallback callback) const {
  DCHECK(url.GetOrigin() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  auto* network_context = GetNetworkContext();
  if (!network_context) {
    // Cannot invoke the callback right away because it would cause the
    // use-after-free after returning from this function.
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, content::BrowserTaskType::kPreconnect},
        base::BindOnce(std::move(callback), false));
    return nullptr;
  }

  return std::make_unique<ResolveHostClientImpl>(url, std::move(callback),
                                                 network_context);
}

std::unique_ptr<ProxyLookupClientImpl> PreconnectManager::LookupProxyForUrl(
    const GURL& url,
    ProxyLookupCallback callback) const {
  DCHECK(url.GetOrigin() == url);
  DCHECK(url.SchemeIsHTTPOrHTTPS());

  auto* network_context = GetNetworkContext();
  if (!network_context) {
    std::move(callback).Run(false);
    return nullptr;
  }

  return std::make_unique<ProxyLookupClientImpl>(url, std::move(callback),
                                                 network_context);
}

void PreconnectManager::TryToLaunchPreresolveJobs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  while (!queued_jobs_.empty() &&
         inflight_preresolves_count_ < kMaxInflightPreresolves) {
    auto job_id = queued_jobs_.front();
    queued_jobs_.pop_front();
    PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
    DCHECK(job);
    PreresolveInfo* info = job->info;

    if (!(info && info->was_canceled)) {
      // This is used to avoid issuing DNS requests when a fixed proxy
      // configuration is in place, which improves efficiency, and is also
      // important if the unproxied DNS may contain incorrect entries.
      job->proxy_lookup_client = LookupProxyForUrl(
          job->url, base::BindOnce(&PreconnectManager::OnProxyLookupFinished,
                                   weak_factory_.GetWeakPtr(), job_id));
      if (info)
        ++info->inflight_count;
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
    observer_->OnPreresolveFinished(job->url, success);

  job->resolve_host_client = nullptr;
  FinishPreresolveJob(job_id, success);
}

void PreconnectManager::OnProxyLookupFinished(PreresolveJobId job_id,
                                              bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PreresolveJob* job = preresolve_jobs_.Lookup(job_id);
  DCHECK(job);

  if (observer_)
    observer_->OnProxyLookupFinished(job->url, success);

  job->proxy_lookup_client = nullptr;
  if (success) {
    FinishPreresolveJob(job_id, success);
  } else {
    job->resolve_host_client = PreresolveUrl(
        job->url, base::BindOnce(&PreconnectManager::OnPreresolveFinished,
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
                  job->network_isolation_key);
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
  auto it = preresolve_info_.find(info->url.host());
  DCHECK(it != preresolve_info_.end());
  DCHECK(info == it->second.get());
  if (delegate_)
    delegate_->PreconnectFinished(std::move(info->stats));
  preresolve_info_.erase(it);
}

network::mojom::NetworkContext* PreconnectManager::GetNetworkContext() const {
  if (network_context_)
    return network_context_;

  if (profile_->AsTestingProfile()) {
    // We're testing and |network_context_| wasn't set. Return nullptr to avoid
    // hitting the network.
    return nullptr;
  }

  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetNetworkContext();
}

}  // namespace predictors
