// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_quota_helper_impl.h"

#include <map>
#include <set>

#include "base/barrier_closure.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using content::BrowserThread;
using content::BrowserContext;

// static
scoped_refptr<BrowsingDataQuotaHelper> BrowsingDataQuotaHelper::Create(
    Profile* profile) {
  return base::MakeRefCounted<BrowsingDataQuotaHelperImpl>(
      profile->GetDefaultStoragePartition()->GetQuotaManager());
}

void BrowsingDataQuotaHelperImpl::StartFetching(FetchResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataQuotaHelperImpl::FetchQuotaInfoOnIOThread,
                     this, std::move(callback)));
}

void BrowsingDataQuotaHelperImpl::DeleteHostData(const std::string& host,
                                                 StorageType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowsingDataQuotaHelperImpl::DeleteHostDataOnIOThread,
                     this, host, type));
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

  const StorageType types[] = {StorageType::kTemporary, StorageType::kSyncable};

  // Query storage keys for each storage types. When complete, process the
  // collected quota info.
  QuotaInfoMap* quota_info = new QuotaInfoMap();

  base::RepeatingClosure completion = base::BarrierClosure(
      std::size(types),
      base::BindOnce(&BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     base::Owned(quota_info)));

  for (const StorageType& type : types) {
    quota_manager_->GetStorageKeysForType(
        type, base::BindOnce(&BrowsingDataQuotaHelperImpl::GotStorageKeys,
                             weak_factory_.GetWeakPtr(), quota_info, completion,
                             type));
  }
}

void BrowsingDataQuotaHelperImpl::GotStorageKeys(
    QuotaInfoMap* quota_info,
    base::OnceClosure completion,
    StorageType type,
    const std::set<blink::StorageKey>& storage_keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int storage_key_count = base::ranges::count_if(
      storage_keys, [](const blink::StorageKey& storage_key) {
        return browsing_data::IsWebScheme(storage_key.origin().scheme());
      });

  auto storage_key_completion =
      base::BarrierClosure(storage_key_count, std::move(completion));
  for (const blink::StorageKey& storage_key : storage_keys) {
    if (!browsing_data::IsWebScheme(storage_key.origin().scheme()))
      continue;  // Non-websafe state is not considered browsing data.
    quota_manager_->GetStorageKeyUsageWithBreakdown(
        storage_key, type,
        base::BindOnce(&BrowsingDataQuotaHelperImpl::GotStorageKeyUsage,
                       weak_factory_.GetWeakPtr(), quota_info, storage_key,
                       type)
            .Then(storage_key_completion));
  }
}

void BrowsingDataQuotaHelperImpl::GotStorageKeyUsage(
    QuotaInfoMap* quota_info,
    const blink::StorageKey& storage_key,
    StorageType type,
    int64_t usage,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (type) {
    case StorageType::kTemporary:
      (*quota_info)[storage_key.origin().host()].temporary_usage += usage;
      break;
    case StorageType::kSyncable:
      (*quota_info)[storage_key.origin().host()].syncable_usage += usage;
      break;
    default:
      NOTREACHED();
  }
}

void BrowsingDataQuotaHelperImpl::OnGetHostsUsageComplete(
    FetchResultCallback callback,
    QuotaInfoMap* quota_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  QuotaInfoArray result;
  for (auto& pair : *quota_info) {
    QuotaInfo& info = pair.second;
    // Skip unused entries
    if (info.temporary_usage <= 0 && info.syncable_usage <= 0)
      continue;

    info.host = pair.first;
    result.push_back(info);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void BrowsingDataQuotaHelperImpl::DeleteHostDataOnIOThread(
    const std::string& host,
    blink::mojom::StorageType type) {
  quota_manager_->DeleteHostData(
      host, type, base::DoNothingAs<void(blink::mojom::QuotaStatusCode)>());
}
