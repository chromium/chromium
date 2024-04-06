// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_DEVICE_DISCOVERY_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_DEVICE_DISCOVERY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "content/public/browser/devtools_agent_host.h"

class DevToolsDeviceDiscovery {
 public:
  class RemotePage : public base::RefCountedThreadSafe<RemotePage> {
   public:
    RemotePage(const RemotePage&) = delete;
    RemotePage& operator=(const RemotePage&) = delete;

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
               base::Value::Dict dict);

    virtual ~RemotePage();

    scoped_refptr<AndroidDeviceManager::Device> device_;
    std::string browser_id_;
    std::string browser_version_;
    std::string frontend_url_;
    base::Value::Dict dict_;
    scoped_refptr<content::DevToolsAgentHost> agent_host_;
  };

  using RemotePages = std::vector<scoped_refptr<RemotePage>>;

  class RemoteBrowser : public base::RefCountedThreadSafe<RemoteBrowser> {
   public:
    RemoteBrowser(const RemoteBrowser&) = delete;
    RemoteBrowser& operator=(const RemoteBrowser&) = delete;

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
  };

  using RemoteBrowsers = std::vector<scoped_refptr<RemoteBrowser>>;

  class RemoteDevice : public base::RefCountedThreadSafe<RemoteDevice> {
   public:
    RemoteDevice(const RemoteDevice&) = delete;
    RemoteDevice& operator=(const RemoteDevice&) = delete;

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
  };

  using RemoteDevices = std::vector<scoped_refptr<RemoteDevice>>;

  using CompleteDevice =
      std::pair<scoped_refptr<AndroidDeviceManager::Device>,
                scoped_refptr<RemoteDevice>>;
  using CompleteDevices = std::vector<CompleteDevice>;
  using DeviceListCallback =
      base::RepeatingCallback<void(const CompleteDevices&)>;

  DevToolsDeviceDiscovery(AndroidDeviceManager* device_manager,
                          DeviceListCallback callback);

  DevToolsDeviceDiscovery(const DevToolsDeviceDiscovery&) = delete;
  DevToolsDeviceDiscovery& operator=(const DevToolsDeviceDiscovery&) = delete;

  ~DevToolsDeviceDiscovery();

  void SetScheduler(base::RepeatingCallback<void(base::OnceClosure)> scheduler);

  static scoped_refptr<content::DevToolsAgentHost> CreateBrowserAgentHost(
      scoped_refptr<AndroidDeviceManager::Device> device,
      scoped_refptr<RemoteBrowser> browser);

 private:
  class DiscoveryRequest;

  void RequestDeviceList();
  void ReceivedDeviceList(const CompleteDevices& complete_devices);

  raw_ptr<AndroidDeviceManager, DanglingUntriaged> device_manager_;
  const DeviceListCallback callback_;
  base::RepeatingCallback<void(base::OnceClosure)> task_scheduler_;
  base::WeakPtrFactory<DevToolsDeviceDiscovery> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_DEVICE_DISCOVERY_H_
