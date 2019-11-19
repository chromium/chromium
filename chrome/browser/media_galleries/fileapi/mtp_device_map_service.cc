// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_async_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"

namespace {

base::LazyInstance<MTPDeviceMapService>::DestructorAtExit
    g_mtp_device_map_service = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MTPDeviceMapService* MTPDeviceMapService::GetInstance() {
  return g_mtp_device_map_service.Pointer();
}

void MTPDeviceMapService::RegisterMTPFileSystem(
    const base::FilePath::StringType& device_location,
    const std::string& filesystem_id,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_location.empty());
  DCHECK(!filesystem_id.empty());

  const AsyncDelegateKey key = GetAsyncDelegateKey(device_location, read_only);
  if (!base::Contains(mtp_device_usage_map_, key)) {
    // Note that this initializes the delegate asynchronously, but since
    // the delegate will only be used from the IO thread, it is guaranteed
    // to be created before use of it expects it to be there.
    CreateMTPDeviceAsyncDelegate(
        device_location, read_only,
        base::Bind(&MTPDeviceMapService::AddAsyncDelegate,
                   base::Unretained(this), device_location, read_only));
    mtp_device_usage_map_[key] = 0;
  }

  mtp_device_usage_map_[key]++;
  mtp_device_map_[filesystem_id] = make_pair(device_location, read_only);
}

void MTPDeviceMapService::RevokeMTPFileSystem(
    const std::string& filesystem_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!filesystem_id.empty());

  MTPDeviceFileSystemMap::iterator it = mtp_device_map_.find(filesystem_id);
  if (it != mtp_device_map_.end()) {
    const base::FilePath::StringType device_location = it->second.first;
    const bool read_only = it->second.second;

    mtp_device_map_.erase(it);

    const AsyncDelegateKey key =
        GetAsyncDelegateKey(device_location, read_only);
    MTPDeviceUsageMap::iterator delegate_it = mtp_device_usage_map_.find(key);
    DCHECK(delegate_it != mtp_device_usage_map_.end());

    mtp_device_usage_map_[key]--;
    if (mtp_device_usage_map_[key] == 0) {
      mtp_device_usage_map_.erase(delegate_it);
      RemoveAsyncDelegate(device_location, read_only);
    }
  }
}

void MTPDeviceMapService::AddAsyncDelegate(
    const base::FilePath::StringType& device_location,
    const bool read_only,
    MTPDeviceAsyncDelegate* delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(delegate);
  DCHECK(!device_location.empty());

  const AsyncDelegateKey key = GetAsyncDelegateKey(device_location, read_only);
  if (base::Contains(async_delegate_map_, key))
    return;
  async_delegate_map_[key] = delegate;
}

void MTPDeviceMapService::RemoveAsyncDelegate(
    const base::FilePath::StringType& device_location,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_location.empty());

  const AsyncDelegateKey key = GetAsyncDelegateKey(device_location, read_only);
  AsyncDelegateMap::iterator it = async_delegate_map_.find(key);
  DCHECK(it != async_delegate_map_.end());
  it->second->CancelPendingTasksAndDeleteDelegate();
  async_delegate_map_.erase(it);
}

// static
MTPDeviceMapService::AsyncDelegateKey MTPDeviceMapService::GetAsyncDelegateKey(
    const base::FilePath::StringType& device_location,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::FilePath::StringType key;
  key.append(read_only ? FILE_PATH_LITERAL("ReadOnly")
                       : FILE_PATH_LITERAL("ReadWrite"));
  key.append(FILE_PATH_LITERAL("|"));
  key.append(device_location);
  return key;
}

MTPDeviceAsyncDelegate* MTPDeviceMapService::GetMTPDeviceAsyncDelegate(
    const storage::FileSystemURL& filesystem_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!filesystem_url.filesystem_id().empty());

  const std::string& filesystem_id = filesystem_url.filesystem_id();
  // File system may be already revoked on ExternalMountPoints side, we check
  // here that the file system is still valid.
  base::FilePath device_path;
  if (!storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          filesystem_id, &device_path)) {
    return NULL;
  }

  const base::FilePath::StringType& device_location = device_path.value();

  MTPDeviceFileSystemMap::const_iterator mtp_device_map_it =
      mtp_device_map_.find(filesystem_id);
  if (mtp_device_map_it == mtp_device_map_.end())
    return NULL;

  DCHECK_EQ(device_path.value(), mtp_device_map_it->second.first);
  const bool read_only = mtp_device_map_it->second.second;
  const AsyncDelegateKey key = GetAsyncDelegateKey(device_location, read_only);

  AsyncDelegateMap::const_iterator async_delegate_map_it =
      async_delegate_map_.find(key);
  return (async_delegate_map_it != async_delegate_map_.end())
             ? async_delegate_map_it->second
             : NULL;
}

MTPDeviceMapService::MTPDeviceMapService() {
}

MTPDeviceMapService::~MTPDeviceMapService() {
  DCHECK(mtp_device_usage_map_.empty());
}
