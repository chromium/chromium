// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"

using content::BrowserThread;
using content::ServiceWorkerContext;
using content::ServiceWorkerUsageInfo;

namespace {

void GetAllOriginsInfoForServiceWorkerCallback(
    const BrowsingDataServiceWorkerHelper::FetchCallback& callback,
    const std::vector<ServiceWorkerUsageInfo>& origins) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  std::list<ServiceWorkerUsageInfo> result;
  for (const ServiceWorkerUsageInfo& origin : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin.origin))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, result));
}

}  // namespace

BrowsingDataServiceWorkerHelper::BrowsingDataServiceWorkerHelper(
    ServiceWorkerContext* service_worker_context)
    : service_worker_context_(service_worker_context) {
  DCHECK(service_worker_context_);
}

BrowsingDataServiceWorkerHelper::~BrowsingDataServiceWorkerHelper() {}

void BrowsingDataServiceWorkerHelper::StartFetching(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrowsingDataServiceWorkerHelper::
                         FetchServiceWorkerUsageInfoOnIOThread,
                     this, callback));
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnIOThread,
          this, origin));
}

void BrowsingDataServiceWorkerHelper::FetchServiceWorkerUsageInfoOnIOThread(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback.is_null());

  service_worker_context_->GetAllOriginsInfo(
      base::BindOnce(&GetAllOriginsInfoForServiceWorkerCallback, callback));
}

void BrowsingDataServiceWorkerHelper::DeleteServiceWorkersOnIOThread(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_worker_context_->DeleteForOrigin(origin, base::DoNothing());
}

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    PendingServiceWorkerUsageInfo(const GURL& origin,
                                  const std::vector<GURL>& scopes)
    : origin(origin), scopes(scopes) {
}

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    PendingServiceWorkerUsageInfo(const PendingServiceWorkerUsageInfo& other) =
        default;

CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
    ~PendingServiceWorkerUsageInfo() {
}

bool CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo::
operator<(const PendingServiceWorkerUsageInfo& other) const {
  return std::tie(origin, scopes) < std::tie(other.origin, other.scopes);
}

CannedBrowsingDataServiceWorkerHelper::CannedBrowsingDataServiceWorkerHelper(
    content::ServiceWorkerContext* context)
    : BrowsingDataServiceWorkerHelper(context) {
}

CannedBrowsingDataServiceWorkerHelper::
    ~CannedBrowsingDataServiceWorkerHelper() {
}

void CannedBrowsingDataServiceWorkerHelper::AddServiceWorker(
    const GURL& origin, const std::vector<GURL>& scopes) {
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  pending_service_worker_info_.insert(
      PendingServiceWorkerUsageInfo(origin, scopes));
}

void CannedBrowsingDataServiceWorkerHelper::Reset() {
  pending_service_worker_info_.clear();
}

bool CannedBrowsingDataServiceWorkerHelper::empty() const {
  return pending_service_worker_info_.empty();
}

size_t CannedBrowsingDataServiceWorkerHelper::GetServiceWorkerCount() const {
  return pending_service_worker_info_.size();
}

const std::set<
    CannedBrowsingDataServiceWorkerHelper::PendingServiceWorkerUsageInfo>&
CannedBrowsingDataServiceWorkerHelper::GetServiceWorkerUsageInfo() const {
  return pending_service_worker_info_;
}

void CannedBrowsingDataServiceWorkerHelper::StartFetching(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<ServiceWorkerUsageInfo> result;
  for (const PendingServiceWorkerUsageInfo& pending_info :
       pending_service_worker_info_) {
    ServiceWorkerUsageInfo info(pending_info.origin, pending_info.scopes);
    result.push_back(info);
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, result));
}

void CannedBrowsingDataServiceWorkerHelper::DeleteServiceWorkers(
    const GURL& origin) {
  for (auto it = pending_service_worker_info_.begin();
       it != pending_service_worker_info_.end();) {
    if (it->origin == origin)
      pending_service_worker_info_.erase(it++);
    else
      ++it;
  }
  BrowsingDataServiceWorkerHelper::DeleteServiceWorkers(origin);
}
