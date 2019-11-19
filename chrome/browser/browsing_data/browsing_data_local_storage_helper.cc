// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace {

// Only websafe state is considered browsing data.
bool HasStorageScheme(const GURL& origin_url) {
  return BrowsingDataHelper::HasWebScheme(origin_url);
}

void GetUsageInfoCallback(
    BrowsingDataLocalStorageHelper::FetchCallback callback,
    const std::vector<content::StorageUsageInfo>& infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const content::StorageUsageInfo& info : infos) {
    if (!HasStorageScheme(info.origin.GetURL()))
      continue;
    result.push_back(info);
  }

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

}  // namespace

BrowsingDataLocalStorageHelper::BrowsingDataLocalStorageHelper(Profile* profile)
    : dom_storage_context_(BrowserContext::GetDefaultStoragePartition(profile)
                               ->GetDOMStorageContext()) {
  DCHECK(dom_storage_context_);
}

BrowsingDataLocalStorageHelper::~BrowsingDataLocalStorageHelper() {
}

void BrowsingDataLocalStorageHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  dom_storage_context_->GetLocalStorageUsage(
      base::BindOnce(&GetUsageInfoCallback, std::move(callback)));
}

void BrowsingDataLocalStorageHelper::DeleteOrigin(const url::Origin& origin,
                                                  base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  dom_storage_context_->DeleteLocalStorage(origin, std::move(callback));
}

//---------------------------------------------------------

CannedBrowsingDataLocalStorageHelper::CannedBrowsingDataLocalStorageHelper(
    Profile* profile)
    : BrowsingDataLocalStorageHelper(profile) {
}

void CannedBrowsingDataLocalStorageHelper::Add(const url::Origin& origin) {
  if (!HasStorageScheme(origin.GetURL()))
    return;
  pending_origins_.insert(origin);
}

void CannedBrowsingDataLocalStorageHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataLocalStorageHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataLocalStorageHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedBrowsingDataLocalStorageHelper::GetOrigins()
    const {
  return pending_origins_;
}

void CannedBrowsingDataLocalStorageHelper::StartFetching(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<content::StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataLocalStorageHelper::DeleteOrigin(
    const url::Origin& origin,
    base::OnceClosure callback) {
  pending_origins_.erase(origin);
  BrowsingDataLocalStorageHelper::DeleteOrigin(origin, std::move(callback));
}

CannedBrowsingDataLocalStorageHelper::~CannedBrowsingDataLocalStorageHelper() {}
