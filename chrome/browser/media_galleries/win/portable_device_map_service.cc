// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/win/portable_device_map_service.h"

#include "base/check_op.h"
#include "content/public/browser/browser_thread.h"

namespace {

base::LazyInstance<PortableDeviceMapService>::DestructorAtExit
    g_portable_device_map_service = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PortableDeviceMapService* PortableDeviceMapService::GetInstance() {
  return g_portable_device_map_service.Pointer();
}

void PortableDeviceMapService::AddPortableDevice(
    const std::wstring& device_location,
    IPortableDevice* device) {
  DCHECK(!device_location.empty());
  DCHECK(device);
  base::AutoLock lock(lock_);
  device_map_[device_location] = PortableDeviceInfo(device);
}

void PortableDeviceMapService::MarkPortableDeviceForDeletion(
    const std::wstring& device_location) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_location.empty());
  base::AutoLock lock(lock_);
  PortableDeviceMap::iterator it = device_map_.find(device_location);
  if (it != device_map_.end())
    it->second.scheduled_to_delete = true;
}

void PortableDeviceMapService::RemovePortableDevice(
    const std::wstring& device_location) {
  DCHECK(!device_location.empty());
  base::AutoLock lock(lock_);
  PortableDeviceMap::const_iterator it = device_map_.find(device_location);
  if ((it != device_map_.end()) && it->second.scheduled_to_delete)
    device_map_.erase(it);
}

IPortableDevice* PortableDeviceMapService::GetPortableDevice(
    const std::wstring& device_location) {
  DCHECK(!device_location.empty());
  base::AutoLock lock(lock_);
  PortableDeviceMap::const_iterator it = device_map_.find(device_location);
  return (it == device_map_.end() || it->second.scheduled_to_delete) ?
      NULL : it->second.portable_device.Get();
}

PortableDeviceMapService::PortableDeviceInfo::PortableDeviceInfo()
    : scheduled_to_delete(false) {
}

PortableDeviceMapService::PortableDeviceInfo::PortableDeviceInfo(
    IPortableDevice* device)
    : portable_device(device),
      scheduled_to_delete(false) {
}

PortableDeviceMapService::PortableDeviceInfo::~PortableDeviceInfo() {
}

PortableDeviceMapService::PortableDeviceMapService() {
}

PortableDeviceMapService::~PortableDeviceMapService() {
}
