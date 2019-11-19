// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_usage_info.h"

using content::BrowserThread;
using content::ServiceWorkerContext;
using content::StorageUsageInfo;

namespace {

void GetAllOriginsInfoForServiceWorkerCallback(
    BrowsingDataServiceWorkerHelper::FetchCallback callback,
    const std::vector<StorageUsageInfo>& origins) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const StorageUsageInfo& origin : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin.origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }

  content::RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                                 base::BindOnce(std::move(callback), result));
}

}  // namespace

BrowsingDataServiceWorkerHelper::BrowsingDataServiceWorkerHelper(
    ServiceWorkerContext* service_worker_context)
    : service_worker_context_(service_worker_context) {
  DCHECK(service_worker_context_);
}

BrowsingDataServiceWorkerHelper::~BrowsingDataServiceWorkerHelper() {}

void BrowsingDataServiceWorkerHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  content::RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BrowsingDataServiceWorkerHelper::
                         FetchServiceWorkerUsageInfoOnCoreThread,
                     this, std::move(callback)));
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          &BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnCoreThread,
          this, origin));
}

void BrowsingDataServiceWorkerHelper::FetchServiceWorkerUsageInfoOnCoreThread(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(!callback.is_null());

  service_worker_context_->GetAllOriginsInfo(base::BindOnce(
      &GetAllOriginsInfoForServiceWorkerCallback, std::move(callback)));
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnCoreThread(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  service_worker_context_->DeleteForOrigin(origin, base::DoNothing());
}

CannedBrowsingDataServiceWorkerHelper::CannedBrowsingDataServiceWorkerHelper(
    content::ServiceWorkerContext* context)
    : BrowsingDataServiceWorkerHelper(context) {
}

CannedBrowsingDataServiceWorkerHelper::
    ~CannedBrowsingDataServiceWorkerHelper() {
}

void CannedBrowsingDataServiceWorkerHelper::Add(const url::Origin& origin) {
  if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_origins_.insert(origin);
}

void CannedBrowsingDataServiceWorkerHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataServiceWorkerHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataServiceWorkerHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedBrowsingDataServiceWorkerHelper::GetOrigins()
    const {
  return pending_origins_;
}

void CannedBrowsingDataServiceWorkerHelper::StartFetching(
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataServiceWorkerHelper::DeleteServiceWorkers(
    const GURL& origin) {
  pending_origins_.erase(url::Origin::Create(origin));
  BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(origin);
}
