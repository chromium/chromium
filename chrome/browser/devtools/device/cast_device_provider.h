// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_CAST_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_CAST_DEVICE_PROVIDER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/local_discovery/service_discovery_device_lister.h"
#include "content/public/browser/browser_thread.h"

// Supplies Cast device information for the purposes of remote debugging Cast
// applications over ADB.
class CastDeviceProvider
    : public AndroidDeviceManager::DeviceProvider,
      public local_discovery::ServiceDiscoveryDeviceLister::Delegate {
 public:
  CastDeviceProvider();

  // DeviceProvider implementation:
  void QueryDevices(const SerialsCallback& callback) override;
  void QueryDeviceInfo(const std::string& serial,
                       const DeviceInfoCallback& callback) override;
  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  const SocketCallback& callback) override;

  // ServiceDiscoveryDeviceLister::Delegate implementation:
  void OnDeviceChanged(
      const std::string& service_type,
      bool added,
      const local_discovery::ServiceDescription& service_description) override;
  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override;
  void OnDeviceCacheFlushed(const std::string& service_type) override;

 private:
  class DeviceListerDelegate;

  ~CastDeviceProvider() override;

  scoped_refptr<TCPDeviceProvider> tcp_provider_;
  std::unique_ptr<DeviceListerDelegate,
                  content::BrowserThread::DeleteOnUIThread>
      lister_delegate_;

  // Keyed on the hostname (IP address).
  std::map<std::string, AndroidDeviceManager::DeviceInfo> device_info_map_;
  // Maps a service name to the hostname (IP address).
  std::map<std::string, std::string> service_hostname_map_;

  base::WeakPtrFactory<CastDeviceProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastDeviceProvider);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_CAST_DEVICE_PROVIDER_H_
