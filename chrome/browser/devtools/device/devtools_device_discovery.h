// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_DEVICE_DISCOVERY_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_DEVICE_DISCOVERY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "content/public/browser/devtools_agent_host.h"

class DevToolsDeviceDiscovery {
 public:
  class RemotePage : public base::RefCountedThreadSafe<RemotePage> {
   public:
    scoped_refptr<AndroidDeviceManager::Device> device() { return device_; }
    const std::string& socket() { return browser_id_; }
    const std::string& frontend_url() { return frontend_url_; }
    scoped_refptr<content::DevToolsAgentHost> CreateTarget();

   private:
    friend class base::RefCountedThreadSafe<RemotePage>;
    friend class DevToolsDeviceDiscovery;

    RemotePage(scoped_refptr<AndroidDeviceManager::Device> device,
               const std::string& browser_id,
               const std::string& browser_version,
               base::Value dict);

    virtual ~RemotePage();

    scoped_refptr<AndroidDeviceManager::Device> device_;
    std::string browser_id_;
    std::string browser_version_;
    std::string frontend_url_;
    base::Value dict_;
    scoped_refptr<content::DevToolsAgentHost> agent_host_;

    DISALLOW_COPY_AND_ASSIGN(RemotePage);
  };

  using RemotePages = std::vector<scoped_refptr<RemotePage>>;

  class RemoteBrowser : public base::RefCountedThreadSafe<RemoteBrowser> {
   public:
    const std::string& serial() { return serial_; }
    const std::string& socket() { return browser_id_; }
    const std::string& display_name() { return display_name_; }
    const std::string& user() { return user_; }
    const std::string& version() { return version_; }
    const std::string& browser_target_id() { return browser_target_id_; }
    const RemotePages& pages() { return pages_; }

    bool IsChrome();
    std::string GetId();

    using ParsedVersion = std::vector<int>;
    ParsedVersion GetParsedVersion();

   private:
    friend class base::RefCountedThreadSafe<RemoteBrowser>;
    friend class DevToolsDeviceDiscovery;

    RemoteBrowser(const std::string& serial,
                  const AndroidDeviceManager::BrowserInfo& browser_info);

    virtual ~RemoteBrowser();

    std::string serial_;
    std::string browser_id_;
    std::string display_name_;
    std::string user_;
    AndroidDeviceManager::BrowserInfo::Type type_;
    std::string version_;
    std::string browser_target_id_;
    RemotePages pages_;

    DISALLOW_COPY_AND_ASSIGN(RemoteBrowser);
  };

  using RemoteBrowsers = std::vector<scoped_refptr<RemoteBrowser>>;

  class RemoteDevice : public base::RefCountedThreadSafe<RemoteDevice> {
   public:
    std::string serial() { return serial_; }
    std::string model() { return model_; }
    bool is_connected() { return connected_; }
    RemoteBrowsers& browsers() { return browsers_; }
    gfx::Size screen_size() { return screen_size_; }

   private:
    friend class base::RefCountedThreadSafe<RemoteDevice>;
    friend class DevToolsDeviceDiscovery;

    RemoteDevice(const std::string& serial,
                 const AndroidDeviceManager::DeviceInfo& device_info);

    virtual ~RemoteDevice();

    std::string serial_;
    std::string model_;
    bool connected_;
    RemoteBrowsers browsers_;
    gfx::Size screen_size_;

    DISALLOW_COPY_AND_ASSIGN(RemoteDevice);
  };

  using RemoteDevices = std::vector<scoped_refptr<RemoteDevice>>;

  using CompleteDevice =
      std::pair<scoped_refptr<AndroidDeviceManager::Device>,
                scoped_refptr<RemoteDevice>>;
  using CompleteDevices = std::vector<CompleteDevice>;
  using DeviceListCallback = base::Callback<void(const CompleteDevices&)>;

  DevToolsDeviceDiscovery(
      AndroidDeviceManager* device_manager,
      const DeviceListCallback& callback);
  ~DevToolsDeviceDiscovery();

  void SetScheduler(base::Callback<void(const base::Closure&)> scheduler);

  static scoped_refptr<content::DevToolsAgentHost> CreateBrowserAgentHost(
      scoped_refptr<AndroidDeviceManager::Device> device,
      scoped_refptr<RemoteBrowser> browser);

 private:
  class DiscoveryRequest;

  void RequestDeviceList();
  void ReceivedDeviceList(const CompleteDevices& complete_devices);

  AndroidDeviceManager* device_manager_;
  const DeviceListCallback callback_;
  base::Callback<void(const base::Closure&)> task_scheduler_;
  base::WeakPtrFactory<DevToolsDeviceDiscovery> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DevToolsDeviceDiscovery);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_DEVICE_DISCOVERY_H_
