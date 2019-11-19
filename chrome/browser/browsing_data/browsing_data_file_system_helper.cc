// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_quota_util.h"
#include "storage/common/file_system/file_system_types.h"

using content::BrowserThread;

namespace storage {
class FileSystemContext;
}

BrowsingDataFileSystemHelper::BrowsingDataFileSystemHelper(
    storage::FileSystemContext* filesystem_context)
    : filesystem_context_(filesystem_context) {
  DCHECK(filesystem_context_.get());
}

BrowsingDataFileSystemHelper::~BrowsingDataFileSystemHelper() {}

base::SequencedTaskRunner* BrowsingDataFileSystemHelper::file_task_runner() {
  return filesystem_context_->default_file_task_runner();
}

void BrowsingDataFileSystemHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataFileSystemHelper::FetchFileSystemInfoInFileThread, this,
          std::move(callback)));
}

void BrowsingDataFileSystemHelper::DeleteFileSystemOrigin(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataFileSystemHelper::DeleteFileSystemOriginInFileThread,
          this, origin));
}

void BrowsingDataFileSystemHelper::FetchFileSystemInfoInFileThread(
    FetchCallback callback) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  DCHECK(!callback.is_null());

  // We check usage for these filesystem types.
  const storage::FileSystemType types[] = {
    storage::kFileSystemTypeTemporary,
    storage::kFileSystemTypePersistent,
#if BUILDFLAG(ENABLE_EXTENSIONS)
    storage::kFileSystemTypeSyncable,
#endif
  };

  std::list<FileSystemInfo> result;
  std::map<GURL, FileSystemInfo> file_system_info_map;
  for (size_t i = 0; i < base::size(types); ++i) {
    storage::FileSystemType type = types[i];
    storage::FileSystemQuotaUtil* quota_util =
        filesystem_context_->GetQuotaUtil(type);
    DCHECK(quota_util);
    std::set<GURL> origins;
    quota_util->GetOriginsForTypeOnFileTaskRunner(type, &origins);
    for (const GURL& current : origins) {
      if (!BrowsingDataHelper::HasWebScheme(current))
        continue;  // Non-websafe state is not considered browsing data.
      int64_t usage = quota_util->GetOriginUsageOnFileTaskRunner(
          filesystem_context_.get(), current, type);
      auto inserted =
          file_system_info_map
              .insert(std::make_pair(
                  current, FileSystemInfo(url::Origin::Create(current))))
              .first;
      inserted->second.usage_map[type] = usage;
    }
  }

  for (const auto& iter : file_system_info_map)
    result.push_back(iter.second);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void BrowsingDataFileSystemHelper::DeleteFileSystemOriginInFileThread(
    const url::Origin& origin) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  filesystem_context_->DeleteDataForOriginOnFileTaskRunner(origin.GetURL());
}

BrowsingDataFileSystemHelper::FileSystemInfo::FileSystemInfo(
    const url::Origin& origin)
    : origin(origin) {}

BrowsingDataFileSystemHelper::FileSystemInfo::FileSystemInfo(
    const FileSystemInfo& other) = default;

BrowsingDataFileSystemHelper::FileSystemInfo::~FileSystemInfo() {}

// static
BrowsingDataFileSystemHelper* BrowsingDataFileSystemHelper::Create(
    storage::FileSystemContext* filesystem_context) {
  return new BrowsingDataFileSystemHelper(filesystem_context);
}

CannedBrowsingDataFileSystemHelper::CannedBrowsingDataFileSystemHelper(
    storage::FileSystemContext* filesystem_context)
    : BrowsingDataFileSystemHelper(filesystem_context) {}

CannedBrowsingDataFileSystemHelper::~CannedBrowsingDataFileSystemHelper() {}

void CannedBrowsingDataFileSystemHelper::Add(const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.
  pending_origins_.insert(origin);
}

void CannedBrowsingDataFileSystemHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataFileSystemHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataFileSystemHelper::GetCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return pending_origins_.size();
}

void CannedBrowsingDataFileSystemHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<FileSystemInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin);

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataFileSystemHelper::DeleteFileSystemOrigin(
    const url::Origin& origin) {
  pending_origins_.erase(origin);
  BrowsingDataFileSystemHelper::DeleteFileSystemOrigin(origin);
}
