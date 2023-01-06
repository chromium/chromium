// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_USB_USB_HOST_UI_DELEGATE_H_
#define ASH_COMPONENTS_ARC_USB_USB_HOST_UI_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"

namespace arc {

class ArcUsbHostUiDelegate {
 public:
  using RequestPermissionCallback = base::OnceCallback<void(bool)>;
  // Requests scan device list permission when app tries to get USB device list.
  // Since the calling application will block on the callback being resolved, it
  // should be done as soon as possible to prevent the caller from becoming
  // non-responsive.
  virtual void RequestUsbScanDeviceListPermission(
      const std::string& package_name,
      RequestPermissionCallback callback) = 0;

  // Requests USB device access permission.
  virtual void RequestUsbAccessPermission(
      const std::string& package_name,
      const std::string& guid,
      const std::u16string& serial_number,
      const std::u16string& manufacturer_string,
      const std::u16string& product_string,
      uint16_t vendor_id,
      uint16_t product_id,
      RequestPermissionCallback callback) = 0;

  // Checks if package have access to USB device.
  virtual bool HasUsbAccessPermission(const std::string& package_name,
                                      const std::string& guid,
                                      const std::u16string& serial_number,
                                      uint16_t vendor_id,
                                      uint16_t product_id) const = 0;

  // Temporarily grants package access permission to USB device. This is called
  // when Android launches default package for the USB device. Permission
  // granted through this method should never persist.
  virtual void GrantUsbAccessPermission(const std::string& package_name,
                                        const std::string& guid,
                                        uint16_t vendor_id,
                                        uint16_t product_id) = 0;

  // Gets list of packages which should receive USB device attach/detach event.
  virtual std::unordered_set<std::string> GetEventPackageList(
      const std::string& guid,
      const std::u16string& serial_number,
      uint16_t vendor_id,
      uint16_t product_id) const = 0;

  // Device is detached. Remove pending permission request to the device and
  // ephemeral device permission if the device is not persistent.
  virtual void DeviceRemoved(const std::string& guid) = 0;

  // Clears all pending permission requests. Called when USB host instance
  // connection is closed.
  virtual void ClearPermissionRequests() = 0;

 protected:
  ~ArcUsbHostUiDelegate() = default;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_USB_USB_HOST_UI_DELEGATE_H_
