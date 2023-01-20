// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/devtools/device/android_device_manager.h"

namespace crypto {
class RSAPrivateKey;
}

class AndroidUsbDevice;
class Profile;

class UsbDeviceProvider : public AndroidDeviceManager::DeviceProvider {
 public:
  explicit UsbDeviceProvider(Profile* profile);

  void QueryDevices(SerialsCallback callback) override;

  void QueryDeviceInfo(const std::string& serial,
                       DeviceInfoCallback callback) override;

  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  SocketCallback callback) override;

  void ReleaseDevice(const std::string& serial) override;

 private:
  ~UsbDeviceProvider() override;

  void EnumeratedDevices(
      SerialsCallback callback,
      const std::vector<scoped_refptr<AndroidUsbDevice>>& devices);

  typedef std::map<std::string, scoped_refptr<AndroidUsbDevice> > UsbDeviceMap;

  std::unique_ptr<crypto::RSAPrivateKey> rsa_key_;
  UsbDeviceMap device_map_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_USB_DEVICE_PROVIDER_H_
