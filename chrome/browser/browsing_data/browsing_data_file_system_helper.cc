// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_quota_util.h"
#include "storage/common/fileapi/file_system_types.h"

using content::BrowserThread;

namespace storage {
class FileSystemContext;
}

namespace {

// An implementation of the BrowsingDataFileSystemHelper interface that pulls
// data from a given |filesystem_context| and returns a list of FileSystemInfo
// items to a client.
class BrowsingDataFileSystemHelperImpl : public BrowsingDataFileSystemHelper {
 public:
  // BrowsingDataFileSystemHelper implementation
  explicit BrowsingDataFileSystemHelperImpl(
      storage::FileSystemContext* filesystem_context);
  void StartFetching(const FetchCallback& callback) override;
  void DeleteFileSystemOrigin(const GURL& origin) override;

 private:
  ~BrowsingDataFileSystemHelperImpl() override;

  // Enumerates all filesystem files, storing the resulting list into
  // file_system_file_ for later use. This must be called on the file
  // task runner.
  void FetchFileSystemInfoInFileThread(const FetchCallback& callback);

  // Deletes all file systems associated with |origin|. This must be called on
  // the file task runner.
  void DeleteFileSystemOriginInFileThread(const GURL& origin);

  // Returns the file task runner for the |filesystem_context_|.
  base::SequencedTaskRunner* file_task_runner() {
    return filesystem_context_->default_file_task_runner();
  }

  // Keep a reference to the FileSystemContext object for the current profile
  // for use on the file task runner.
  scoped_refptr<storage::FileSystemContext> filesystem_context_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFileSystemHelperImpl);
};

BrowsingDataFileSystemHelperImpl::BrowsingDataFileSystemHelperImpl(
    storage::FileSystemContext* filesystem_context)
    : filesystem_context_(filesystem_context) {
  DCHECK(filesystem_context_.get());
}

BrowsingDataFileSystemHelperImpl::~BrowsingDataFileSystemHelperImpl() {
}

void BrowsingDataFileSystemHelperImpl::StartFetching(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataFileSystemHelperImpl::FetchFileSystemInfoInFileThread,
          this, callback));
}

void BrowsingDataFileSystemHelperImpl::DeleteFileSystemOrigin(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BrowsingDataFileSystemHelperImpl::DeleteFileSystemOriginInFileThread,
          this, origin));
}

void BrowsingDataFileSystemHelperImpl::FetchFileSystemInfoInFileThread(
    const FetchCallback& callback) {
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
  typedef std::map<GURL, FileSystemInfo> OriginInfoMap;
  OriginInfoMap file_system_info_map;
  for (size_t i = 0; i < arraysize(types); ++i) {
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
              .insert(std::make_pair(current, FileSystemInfo(current)))
              .first;
      inserted->second.usage_map[type] = usage;
    }
  }

  for (const auto& iter : file_system_info_map)
    result.push_back(iter.second);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, result));
}

void BrowsingDataFileSystemHelperImpl::DeleteFileSystemOriginInFileThread(
    const GURL& origin) {
  DCHECK(file_task_runner()->RunsTasksInCurrentSequence());
  filesystem_context_->DeleteDataForOriginOnFileTaskRunner(origin);
}

}  // namespace

BrowsingDataFileSystemHelper::FileSystemInfo::FileSystemInfo(
    const GURL& origin) : origin(origin) {}

BrowsingDataFileSystemHelper::FileSystemInfo::FileSystemInfo(
    const FileSystemInfo& other) = default;

BrowsingDataFileSystemHelper::FileSystemInfo::~FileSystemInfo() {}

// static
BrowsingDataFileSystemHelper* BrowsingDataFileSystemHelper::Create(
    storage::FileSystemContext* filesystem_context) {
  return new BrowsingDataFileSystemHelperImpl(filesystem_context);
}

CannedBrowsingDataFileSystemHelper::CannedBrowsingDataFileSystemHelper(
    Profile* profile) {
}

CannedBrowsingDataFileSystemHelper::~CannedBrowsingDataFileSystemHelper() {}

void CannedBrowsingDataFileSystemHelper::AddFileSystem(
    const GURL& origin,
    const storage::FileSystemType type,
    const int64_t size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This canned implementation of AddFileSystem uses an O(n^2) algorithm; which
  // is fine, as it isn't meant for use in a high-volume context. If it turns
  // out that we want to start using this in a context with many, many origins,
  // we should think about reworking the implementation.
  bool duplicate_origin = false;
  for (FileSystemInfo& file_system : file_system_info_) {
    if (file_system.origin == origin) {
      file_system.usage_map[type] = size;
      duplicate_origin = true;
      break;
    }
  }
  if (duplicate_origin)
    return;

  if (!BrowsingDataHelper::HasWebScheme(origin))
    return;  // Non-websafe state is not considered browsing data.

  FileSystemInfo info(origin);
  info.usage_map[type] = size;
  file_system_info_.push_back(info);
}

void CannedBrowsingDataFileSystemHelper::Reset() {
  file_system_info_.clear();
}

bool CannedBrowsingDataFileSystemHelper::empty() const {
  return file_system_info_.empty();
}

size_t CannedBrowsingDataFileSystemHelper::GetFileSystemCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return file_system_info_.size();
}

void CannedBrowsingDataFileSystemHelper::StartFetching(
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(callback, file_system_info_));
}
