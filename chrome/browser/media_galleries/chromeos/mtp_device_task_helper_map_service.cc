// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/chromeos/mtp_device_task_helper_map_service.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/media_galleries/chromeos/mtp_device_task_helper.h"
#include "content/public/browser/browser_thread.h"

namespace {

base::LazyInstance<MTPDeviceTaskHelperMapService>::DestructorAtExit
    g_mtp_device_task_helper_map_service = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
MTPDeviceTaskHelperMapService* MTPDeviceTaskHelperMapService::GetInstance() {
  return g_mtp_device_task_helper_map_service.Pointer();
}

MTPDeviceTaskHelper* MTPDeviceTaskHelperMapService::CreateDeviceTaskHelper(
    const std::string& storage_name,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!storage_name.empty());
  const MTPDeviceTaskHelperKey key =
      GetMTPDeviceTaskHelperKey(storage_name, read_only);
  DCHECK(!base::Contains(task_helper_map_, key));
  MTPDeviceTaskHelper* task_helper = new MTPDeviceTaskHelper();
  task_helper_map_[key] = task_helper;
  return task_helper;
}

void MTPDeviceTaskHelperMapService::DestroyDeviceTaskHelper(
    const std::string& storage_name,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const MTPDeviceTaskHelperKey key =
      GetMTPDeviceTaskHelperKey(storage_name, read_only);
  TaskHelperMap::iterator it = task_helper_map_.find(key);
  if (it == task_helper_map_.end())
    return;
  delete it->second;
  task_helper_map_.erase(it);
}

MTPDeviceTaskHelper* MTPDeviceTaskHelperMapService::GetDeviceTaskHelper(
    const std::string& storage_name,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!storage_name.empty());
  const MTPDeviceTaskHelperKey key =
      GetMTPDeviceTaskHelperKey(storage_name, read_only);
  TaskHelperMap::const_iterator it = task_helper_map_.find(key);
  return (it != task_helper_map_.end()) ? it->second : NULL;
}

// static
MTPDeviceTaskHelperMapService::MTPDeviceTaskHelperKey
MTPDeviceTaskHelperMapService::GetMTPDeviceTaskHelperKey(
    const std::string& storage_name,
    const bool read_only) {
  return (read_only ? "ReadOnly" : "ReadWrite") + std::string("|") +
         storage_name;
}

MTPDeviceTaskHelperMapService::MTPDeviceTaskHelperMapService() {
}

MTPDeviceTaskHelperMapService::~MTPDeviceTaskHelperMapService() {
}
