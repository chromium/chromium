// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "content/public/browser/browser_thread.h"
#include "storage/common/file_system/file_system_util.h"

using storage::FileSystemURL;
using storage::FileSystemURLSet;

namespace sync_file_system {

namespace {

using OriginAndType = LocalFileSyncStatus::OriginAndType;

OriginAndType GetOriginAndType(const storage::FileSystemURL& url) {
  return std::make_pair(url.origin().GetURL(), url.type());
}

base::FilePath NormalizePath(const base::FilePath& path) {
  // Ensure |path| has single trailing path-separator, so that we can use
  // prefix-match to find descendants of |path| in an ordered container.
  return base::FilePath(path.StripTrailingSeparators().value() +
                        storage::VirtualPath::kSeparator);
}

struct SetKeyHelper {
  template <typename Iterator>
  static const base::FilePath& GetKey(Iterator itr) {
    return *itr;
  }
};

struct MapKeyHelper {
  template <typename Iterator>
  static const base::FilePath& GetKey(Iterator itr) {
    return itr->first;
  }
};

template <typename Container, typename GetKeyHelper>
bool ContainsChildOrParent(const Container& paths,
                           const base::FilePath& path,
                           const GetKeyHelper& get_key_helper) {
  base::FilePath normalized_path = NormalizePath(path);

  // Check if |paths| has a child of |normalized_path|.
  // Note that descendants of |normalized_path| are stored right after
  // |normalized_path| since |normalized_path| has trailing path separator.
  auto upper = paths.upper_bound(normalized_path);

  if (upper != paths.end() &&
      normalized_path.IsParent(get_key_helper.GetKey(upper)))
    return true;

  // Check if any ancestor of |normalized_path| is in |writing_|.
  while (true) {
    if (base::Contains(paths, normalized_path))
      return true;

    if (storage::VirtualPath::IsRootPath(normalized_path))
      return false;

    normalized_path =
        NormalizePath(storage::VirtualPath::DirName(normalized_path));
  }
}

}  // namespace

LocalFileSyncStatus::LocalFileSyncStatus() {}

LocalFileSyncStatus::~LocalFileSyncStatus() {}

void LocalFileSyncStatus::StartWriting(const FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!IsChildOrParentSyncing(url));
  writing_[GetOriginAndType(url)][NormalizePath(url.path())]++;
}

void LocalFileSyncStatus::EndWriting(const FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::FilePath normalized_path = NormalizePath(url.path());
  OriginAndType origin_and_type = GetOriginAndType(url);

  int count = --writing_[origin_and_type][normalized_path];
  if (count == 0) {
    writing_[origin_and_type].erase(normalized_path);
    if (writing_[origin_and_type].empty())
      writing_.erase(origin_and_type);
    for (auto& observer : observer_list_)
      observer.OnSyncEnabled(url);
  }
}

void LocalFileSyncStatus::StartSyncing(const FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!IsChildOrParentWriting(url));
  DCHECK(!IsChildOrParentSyncing(url));
  syncing_[GetOriginAndType(url)].insert(NormalizePath(url.path()));
}

void LocalFileSyncStatus::EndSyncing(const FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::FilePath normalized_path = NormalizePath(url.path());
  OriginAndType origin_and_type = GetOriginAndType(url);

  syncing_[origin_and_type].erase(normalized_path);
  if (syncing_[origin_and_type].empty())
    syncing_.erase(origin_and_type);
  for (auto& observer : observer_list_)
    observer.OnSyncEnabled(url);
  for (auto& observer : observer_list_)
    observer.OnWriteEnabled(url);
}

bool LocalFileSyncStatus::IsWriting(const FileSystemURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return IsChildOrParentWriting(url);
}

bool LocalFileSyncStatus::IsWritable(const FileSystemURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return !IsChildOrParentSyncing(url);
}

bool LocalFileSyncStatus::IsSyncable(const FileSystemURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return !IsChildOrParentSyncing(url) && !IsChildOrParentWriting(url);
}

void LocalFileSyncStatus::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  observer_list_.AddObserver(observer);
}

void LocalFileSyncStatus::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  observer_list_.RemoveObserver(observer);
}

bool LocalFileSyncStatus::IsChildOrParentWriting(
    const FileSystemURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto found = writing_.find(GetOriginAndType(url));
  if (found == writing_.end())
    return false;
  return ContainsChildOrParent(found->second, url.path(),
                               MapKeyHelper());
}

bool LocalFileSyncStatus::IsChildOrParentSyncing(
    const FileSystemURL& url) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto found = syncing_.find(GetOriginAndType(url));
  if (found == syncing_.end())
    return false;
  return ContainsChildOrParent(found->second, url.path(),
                               SetKeyHelper());
}

}  // namespace sync_file_system
