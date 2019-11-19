// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "chrome/browser/devtools/device/devtools_device_discovery.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/gfx/geometry/size.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}

class PortForwardingController;
class Profile;
class TCPDeviceProvider;

class DevToolsAndroidBridge : public KeyedService {
 public:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    // Returns singleton instance of DevToolsAndroidBridge.
    static Factory* GetInstance();

    // Returns DevToolsAndroidBridge associated with |profile|.
    static DevToolsAndroidBridge* GetForProfile(Profile* profile);

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory overrides:
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  using RemotePage = DevToolsDeviceDiscovery::RemotePage;
  using RemotePages = DevToolsDeviceDiscovery::RemotePages;
  using RemoteBrowser = DevToolsDeviceDiscovery::RemoteBrowser;
  using RemoteBrowsers = DevToolsDeviceDiscovery::RemoteBrowsers;
  using RemoteDevice = DevToolsDeviceDiscovery::RemoteDevice;
  using RemoteDevices = DevToolsDeviceDiscovery::RemoteDevices;
  using CompleteDevice = DevToolsDeviceDiscovery::CompleteDevice;
  using CompleteDevices = DevToolsDeviceDiscovery::CompleteDevices;
  using DeviceListCallback = DevToolsDeviceDiscovery::DeviceListCallback;

  using JsonRequestCallback = base::Callback<void(int, const std::string&)>;

  class DeviceListListener {
   public:
    virtual void DeviceListChanged(const RemoteDevices& devices) = 0;
   protected:
    virtual ~DeviceListListener() {}
  };

  explicit DevToolsAndroidBridge(Profile* profile);
  void AddDeviceListListener(DeviceListListener* listener);
  void RemoveDeviceListListener(DeviceListListener* listener);

  class DeviceCountListener {
   public:
    virtual void DeviceCountChanged(int count) = 0;
   protected:
    virtual ~DeviceCountListener() {}
  };

  void AddDeviceCountListener(DeviceCountListener* listener);
  void RemoveDeviceCountListener(DeviceCountListener* listener);

  using PortStatus = int;
  using PortStatusMap = std::map<int, PortStatus>;
  using BrowserStatus = std::pair<scoped_refptr<RemoteBrowser>, PortStatusMap>;
  using ForwardingStatus = std::vector<BrowserStatus>;

  class PortForwardingListener {
   public:
    using PortStatusMap = DevToolsAndroidBridge::PortStatusMap;
    using BrowserStatus = DevToolsAndroidBridge::BrowserStatus;
    using ForwardingStatus = DevToolsAndroidBridge::ForwardingStatus;

    virtual void PortStatusChanged(const ForwardingStatus&) = 0;
   protected:
    virtual ~PortForwardingListener() {}
  };

  void AddPortForwardingListener(PortForwardingListener* listener);
  void RemovePortForwardingListener(PortForwardingListener* listener);

  void set_device_providers_for_test(
      const AndroidDeviceManager::DeviceProviders& device_providers) {
    device_manager_->SetDeviceProviders(device_providers);
  }

  void set_task_scheduler_for_test(
      base::Callback<void(const base::Closure&)> scheduler) {
    task_scheduler_ = scheduler;
  }

  using RemotePageCallback = base::Callback<void(scoped_refptr<RemotePage>)>;
  void OpenRemotePage(scoped_refptr<RemoteBrowser> browser,
                      const std::string& url);

  scoped_refptr<content::DevToolsAgentHost> GetBrowserAgentHost(
      scoped_refptr<RemoteBrowser> browser);

  void SendJsonRequest(const std::string& browser_id_str,
                       const std::string& url,
                       const JsonRequestCallback& callback);

  using TCPProviderCallback =
      base::Callback<void(scoped_refptr<TCPDeviceProvider>)>;
  void set_tcp_provider_callback_for_test(TCPProviderCallback callback);
  void set_usb_device_manager_for_test(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager);

  void Shutdown() override;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DevToolsAndroidBridge>;

  ~DevToolsAndroidBridge() override;

  void StartDeviceListPolling();
  void StopDeviceListPolling();
  bool NeedsDeviceListPolling();

  void RequestDeviceList(const DeviceListCallback& callback);
  void ReceivedDeviceList(const CompleteDevices& complete_devices);

  void StartDeviceCountPolling();
  void StopDeviceCountPolling();
  void RequestDeviceCount(const base::Callback<void(int)>& callback);
  void ReceivedDeviceCount(int count);

  static void ScheduleTaskDefault(const base::Closure& task);

  void CreateDeviceProviders();

  base::WeakPtr<DevToolsAndroidBridge> AsWeakPtr() {
      return weak_factory_.GetWeakPtr();
  }

  Profile* const profile_;
  std::unique_ptr<AndroidDeviceManager> device_manager_;

  using DeviceMap =
      std::map<std::string, scoped_refptr<AndroidDeviceManager::Device> >;
  DeviceMap device_map_;

  using DeviceListListeners = std::vector<DeviceListListener*>;
  DeviceListListeners device_list_listeners_;

  using DeviceCountListeners = std::vector<DeviceCountListener*>;
  DeviceCountListeners device_count_listeners_;
  base::CancelableCallback<void(int)> device_count_callback_;
  base::Callback<void(const base::Closure&)> task_scheduler_;

  using PortForwardingListeners = std::vector<PortForwardingListener*>;
  PortForwardingListeners port_forwarding_listeners_;
  std::unique_ptr<PortForwardingController> port_forwarding_controller_;

  PrefChangeRegistrar pref_change_registrar_;

  TCPProviderCallback tcp_provider_callback_;

  std::unique_ptr<DevToolsDeviceDiscovery> device_discovery_;

  base::WeakPtrFactory<DevToolsAndroidBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DevToolsAndroidBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
