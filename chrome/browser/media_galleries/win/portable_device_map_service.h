// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_PORTABLE_DEVICE_MAP_SERVICE_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_PORTABLE_DEVICE_MAP_SERVICE_H_

#include <portabledeviceapi.h>
#include <wrl/client.h>

#include <map>
#include <string>

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

// PortableDeviceMapService keeps track of initialized portable device
// interfaces. PortableDeviceMapService owns the portable device interfaces.
class PortableDeviceMapService {
 public:
  static PortableDeviceMapService* GetInstance();

  PortableDeviceMapService(const PortableDeviceMapService&) = delete;
  PortableDeviceMapService& operator=(const PortableDeviceMapService&) = delete;

  // Adds the portable |device| interface to the map service for the device
  // specified by the |device_location|. Called on a blocking pool thread.
  void AddPortableDevice(const std::wstring& device_location,
                         IPortableDevice* device);

  // Marks the IPortableDevice interface of the device specified by the
  // |device_location| for deletion. This helps to cancel all the pending
  // tasks before deleting the IPortableDevice interface.
  //
  // Callers of this function should post a task on a blocking pool thread to
  // remove the IPortableDevice interface from the map service.
  //
  // Called on the IO thread.
  void MarkPortableDeviceForDeletion(const std::wstring& device_location);

  // Removes the IPortableDevice interface from the map service for the device
  // specified by the |device_location|. Callers of this function should have
  // already called MarkPortableDeviceForDeletion() on the IO thread.
  // Called on a blocking pool thread.
  void RemovePortableDevice(const std::wstring& device_location);

  // Gets the IPortableDevice interface associated with the device specified
  // by the |device_location|. Returns NULL if the |device_location| is no
  // longer valid (e.g. the corresponding device is detached etc).
  // Called on a blocking pool thread.
  IPortableDevice* GetPortableDevice(const std::wstring& device_location);

 private:
  friend struct base::LazyInstanceTraitsBase<PortableDeviceMapService>;

  struct PortableDeviceInfo {
    PortableDeviceInfo();  // Necessary for STL.
    explicit PortableDeviceInfo(IPortableDevice* device);
    ~PortableDeviceInfo();

    // The portable device interface.
    Microsoft::WRL::ComPtr<IPortableDevice> portable_device;

    // Set to true if the |portable_device| is marked for deletion.
    bool scheduled_to_delete;
  };

  typedef std::map<const std::wstring, PortableDeviceInfo> PortableDeviceMap;

  // Get access to this class using GetInstance() method.
  PortableDeviceMapService();
  ~PortableDeviceMapService();

  // Mapping of |device_location| and IPortableDevice* object.
  PortableDeviceMap device_map_;
  base::Lock lock_;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_PORTABLE_DEVICE_MAP_SERVICE_H_
