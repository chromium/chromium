// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"

#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_context.h"

using content::BrowserThread;
using content::IndexedDBContext;
using content::IndexedDBInfo;

BrowsingDataIndexedDBHelper::BrowsingDataIndexedDBHelper(
    IndexedDBContext* indexed_db_context)
    : indexed_db_context_(indexed_db_context) {
  DCHECK(indexed_db_context_.get());
}

BrowsingDataIndexedDBHelper::~BrowsingDataIndexedDBHelper() {
}

void BrowsingDataIndexedDBHelper::StartFetching(const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataIndexedDBHelper::FetchIndexedDBInfoInIndexedDBThread,
          this, callback));
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
    const FetchCallback& callback) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());
  std::vector<IndexedDBInfo> origins = indexed_db_context_->GetAllOriginsInfo();
  std::list<content::IndexedDBInfo> result;
  for (const IndexedDBInfo& origin : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin.origin))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(origin);
  }
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, result));
}

void BrowsingDataIndexedDBHelper::DeleteIndexedDBInIndexedDBThread(
    const GURL& origin) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksInCurrentSequence());
  indexed_db_context_->DeleteForOrigin(origin);
}

CannedBrowsingDataIndexedDBHelper::
PendingIndexedDBInfo::PendingIndexedDBInfo(const GURL& origin,
                                           const base::string16& name)
    : origin(origin),
      name(name) {
}

CannedBrowsingDataIndexedDBHelper::
PendingIndexedDBInfo::~PendingIndexedDBInfo() {
}

bool CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo::operator<(
    const PendingIndexedDBInfo& other) const {
  return std::tie(origin, name) < std::tie(other.origin, other.name);
}

CannedBrowsingDataIndexedDBHelper::CannedBrowsingDataIndexedDBHelper(
    content::IndexedDBContext* context)
    : BrowsingDataIndexedDBHelper(context) {
}

CannedBrowsingDataIndexedDBHelper::~CannedBrowsingDataIndexedDBHelper() {}

void CannedBrowsingDataIndexedDBHelper::AddIndexedDB(
    const GURL& origin, const base::string16& name) {
  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  pending_indexed_db_info_.insert(PendingIndexedDBInfo(origin, name));
}

void CannedBrowsingDataIndexedDBHelper::Reset() {
  pending_indexed_db_info_.clear();
}

bool CannedBrowsingDataIndexedDBHelper::empty() const {
  return pending_indexed_db_info_.empty();
}

size_t CannedBrowsingDataIndexedDBHelper::GetIndexedDBCount() const {
  return pending_indexed_db_info_.size();
}

const std::set<CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo>&
CannedBrowsingDataIndexedDBHelper::GetIndexedDBInfo() const  {
  return pending_indexed_db_info_;
}

void CannedBrowsingDataIndexedDBHelper::StartFetching(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<IndexedDBInfo> result;
  for (const PendingIndexedDBInfo& pending_info : pending_indexed_db_info_) {
    IndexedDBInfo info(pending_info.origin, 0, base::Time(), 0);
    result.push_back(info);
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, result));
}

void CannedBrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const GURL& origin) {
  for (auto it = pending_indexed_db_info_.begin();
       it != pending_indexed_db_info_.end();) {
    if (it->origin == origin)
      pending_indexed_db_info_.erase(it++);
    else
      ++it;
  }
  BrowsingDataIndexedDBHelper::DeleteIndexedDB(origin);
}
