// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_quota_helper_impl.h"

#include <map>
#include <set>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using content::BrowserThread;
using content::BrowserContext;

// static
BrowsingDataQuotaHelper* BrowsingDataQuotaHelper::Create(Profile* profile) {
  return new BrowsingDataQuotaHelperImpl(
      BrowserContext::GetDefaultStoragePartition(profile)->GetQuotaManager());
}

void BrowsingDataQuotaHelperImpl::StartFetching(FetchResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataQuotaHelperImpl::FetchQuotaInfoOnIOThread,
                     this, std::move(callback)));
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuota(const std::string& host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataQuotaHelperImpl::RevokeHostQuotaOnIOThread,
                     this, host));
}

BrowsingDataQuotaHelperImpl::BrowsingDataQuotaHelperImpl(
    storage::QuotaManager* quota_manager)
    : BrowsingDataQuotaHelper(), quota_manager_(quota_manager) {
  DCHECK(quota_manager);
}

BrowsingDataQuotaHelperImpl::~BrowsingDataQuotaHelperImpl() {}

void BrowsingDataQuotaHelperImpl::FetchQuotaInfoOnIOThread(
    FetchResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const StorageType types[] = {StorageType::kTemporary,
                               StorageType::kPersistent,
                               StorageType::kSyncable};

  // Query hosts for each storage types. When complete, process the collected
  // hosts.
  PendingHosts* pending_hosts = new PendingHosts();
  base::RepeatingClosure completion = base::BarrierClosure(
      base::size(types),
      base::BindOnce(&BrowsingDataQuotaHelperImpl::OnGetOriginsComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(pending_hosts)));

  for (const StorageType& type : types) {
    quota_manager_->GetOriginsModifiedSince(
        type, base::Time(),
        base::BindOnce(&BrowsingDataQuotaHelperImpl::GotOrigins,
                       weak_factory_.GetWeakPtr(), pending_hosts, completion));
  }
}

void BrowsingDataQuotaHelperImpl::GotOrigins(
    PendingHosts* pending_hosts,
    base::OnceClosure completion,
    const std::set<url::Origin>& origins,
    StorageType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const url::Origin& origin : origins) {
    if (!BrowsingDataHelper::IsWebScheme(origin.scheme()))
      continue;  // Non-websafe state is not considered browsing data.
    pending_hosts->insert(std::make_pair(origin.host(), type));
  }
  std::move(completion).Run();
}

void BrowsingDataQuotaHelperImpl::OnGetOriginsComplete(
    FetchResultCallback callback,
    PendingHosts* pending_hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Query usage for each (host, type). When complete, process the results.
  QuotaInfoMap* quota_info = new QuotaInfoMap();
  base::RepeatingClosure completion = base::BarrierClosure(
      pending_hosts->size(),
      base::BindOnce(&BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(quota_info)));

  for (const auto& itr : *pending_hosts) {
    const std::string& host = itr.first;
    StorageType type = itr.second;
    quota_manager_->GetHostUsage(
        host, type,
        base::BindOnce(&BrowsingDataQuotaHelperImpl::GotHostUsage,
                       weak_factory_.GetWeakPtr(), quota_info, completion, host,
                       type));
  }
}

void BrowsingDataQuotaHelperImpl::GotHostUsage(QuotaInfoMap* quota_info,
                                               base::OnceClosure completion,
                                               const std::string& host,
                                               StorageType type,
                                               int64_t usage) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (type) {
    case StorageType::kTemporary:
      (*quota_info)[host].temporary_usage = usage;
      break;
    case StorageType::kPersistent:
      (*quota_info)[host].persistent_usage = usage;
      break;
    case StorageType::kSyncable:
      (*quota_info)[host].syncable_usage = usage;
      break;
    default:
      NOTREACHED();
  }
  std::move(completion).Run();
}

void BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete(
    FetchResultCallback callback,
    QuotaInfoMap* quota_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  QuotaInfoArray result;
  for (auto& pair : *quota_info) {
    QuotaInfo& info = pair.second;
    // Skip unused entries
    if (info.temporary_usage <= 0 && info.persistent_usage <= 0 &&
        info.syncable_usage <= 0)
      continue;

    info.host = pair.first;
    result.push_back(info);
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void BrowsingDataQuotaHelperImpl::RevokeHostQuotaOnIOThread(
    const std::string& host) {
  quota_manager_->SetPersistentHostQuota(
      host, 0,
      base::BindOnce(&BrowsingDataQuotaHelperImpl::DidRevokeHostQuota,
                     weak_factory_.GetWeakPtr()));
}

void BrowsingDataQuotaHelperImpl::DidRevokeHostQuota(
    blink::mojom::QuotaStatusCode /*status*/,
    int64_t /*quota*/) {}
