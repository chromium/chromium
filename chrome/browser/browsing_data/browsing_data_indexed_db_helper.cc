// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/storage_usage_info.h"
#include "url/origin.h"

using content::BrowserThread;
using content::IndexedDBContext;
using content::StorageUsageInfo;

BrowsingDataIndexedDBHelper::BrowsingDataIndexedDBHelper(
    IndexedDBContext* indexed_db_context)
    : indexed_db_context_(indexed_db_context) {
  DCHECK(indexed_db_context_.get());
}

BrowsingDataIndexedDBHelper::~BrowsingDataIndexedDBHelper() {
}

void BrowsingDataIndexedDBHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataIndexedDBHelper::FetchIndexedDBInfoInIndexedDBThread,
          this, std::move(callback)));
}

void BrowsingDataIndexedDBHelper::DeleteIndexedDB(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataIndexedDBHelper::DeleteIndexedDBInIndexedDBThread, this,
          origin));
}

void BrowsingDataIndexedDBHelper::FetchIndexedDBInfoInIndexedDBThread(
    FetchCallback callback) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());
  std::vector<StorageUsageInfo> origins =
      indexed_db_context_->GetAllOriginsInfo();
  std::list<content::StorageUsageInfo> result;
  for (const StorageUsageInfo& origin : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin.origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void BrowsingDataIndexedDBHelper::DeleteIndexedDBInIndexedDBThread(
    const GURL& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());
  indexed_db_context_->DeleteForOrigin(url::Origin::Create(origin));
}

CannedBrowsingDataIndexedDBHelper::CannedBrowsingDataIndexedDBHelper(
    content::IndexedDBContext* context)
    : BrowsingDataIndexedDBHelper(context) {
}

CannedBrowsingDataIndexedDBHelper::~CannedBrowsingDataIndexedDBHelper() {}

void CannedBrowsingDataIndexedDBHelper::Add(const url::Origin& origin) {
  if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_origins_.insert(origin);
}

void CannedBrowsingDataIndexedDBHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataIndexedDBHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataIndexedDBHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedBrowsingDataIndexedDBHelper::GetOrigins()
    const {
  return pending_origins_;
}

void CannedBrowsingDataIndexedDBHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const GURL& origin) {
  pending_origins_.erase(url::Origin::Create(origin));
  BrowsingDataIndexedDBHelper::DeleteIndexedDB(origin);
}
