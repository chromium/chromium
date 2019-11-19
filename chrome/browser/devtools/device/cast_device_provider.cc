// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/cast_device_provider.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"

using local_discovery::ServiceDescription;
using local_discovery::ServiceDiscoveryDeviceLister;
using local_discovery::ServiceDiscoverySharedClient;

namespace {

const int kCastInspectPort = 9222;
const char kCastServiceType[] = "_googlecast._tcp.local";
const char kUnknownCastDevice[] = "Unknown Cast Device";

using ServiceTxtRecordMap = std::map<std::string, std::string>;

// Parses TXT record strings into a map. TXT key-value strings are assumed to
// follow the form "$key(=$value)?", where $key must contain at least one
// character, and $value may be empty.
std::unique_ptr<ServiceTxtRecordMap> ParseServiceTxtRecord(
    const std::vector<std::string>& record) {
  std::unique_ptr<ServiceTxtRecordMap> record_map(new ServiceTxtRecordMap());
  for (const auto& key_value_str : record) {
    if (key_value_str.empty())
      continue;

    size_t index = key_value_str.find("=", 0);
    if (index == std::string::npos) {
      // Some strings may only define a key (no '=' in the key/value string).
      // The chosen behavior is to assume the value is the empty string.
      record_map->insert(std::make_pair(key_value_str, ""));
    } else {
      std::string key = key_value_str.substr(0, index);
      std::string value = key_value_str.substr(index + 1);
      record_map->insert(std::make_pair(key, value));
    }
  }
  return record_map;
}

// Returns the value in the TXT |record_map| for the two-character |key|, or
// |default_value| if |key| was not found.
std::string GetServiceMapValue(const ServiceTxtRecordMap& record_map,
                               const std::string& key,
                               const char* default_value) {
  const auto it = record_map.find(key);
  return (it != record_map.end() && !it->second.empty()) ? it->second
                                                         : default_value;
}

AndroidDeviceManager::DeviceInfo ServiceDescriptionToDeviceInfo(
    const ServiceDescription& service_description) {
  std::unique_ptr<ServiceTxtRecordMap> record_map =
      ParseServiceTxtRecord(service_description.metadata);

  AndroidDeviceManager::DeviceInfo device_info;
  device_info.connected = true;
  device_info.model = GetServiceMapValue(*record_map, "md", kUnknownCastDevice);
  AndroidDeviceManager::BrowserInfo browser_info;
  browser_info.socket_name = base::NumberToString(kCastInspectPort);
  browser_info.display_name = GetServiceMapValue(*record_map, "fn", "");
  browser_info.type = AndroidDeviceManager::BrowserInfo::kTypeChrome;
  device_info.browser_info.push_back(browser_info);
  return device_info;
}

}  // namespace

// The purpose of this class is to route lister delegate signals from
// ServiceDiscoveryDeviceLister (on the UI thread) to CastDeviceProvider (on the
// DevTools ADB thread). Cancellable callbacks are necessary since
// CastDeviceProvider and ServiceDiscoveryDeviceLister are destroyed on
// different threads in undefined order.
//
// TODO(crbug.com/963216): Consolidate DNS-SD implementations for Cast.
class CastDeviceProvider::DeviceListerDelegate
    : public ServiceDiscoveryDeviceLister::Delegate,
      public base::SupportsWeakPtr<DeviceListerDelegate> {
 public:
  DeviceListerDelegate(base::WeakPtr<CastDeviceProvider> provider,
                       scoped_refptr<base::SingleThreadTaskRunner> runner)
      : provider_(provider), runner_(runner) {}

  virtual ~DeviceListerDelegate() {}

  void StartDiscovery() {
    // This must be called on the UI thread; ServiceDiscoverySharedClient and
    // ServiceDiscoveryDeviceLister are thread protected.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (device_lister_)
      return;
    service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
    device_lister_ = ServiceDiscoveryDeviceLister::Create(
        this, service_discovery_client_.get(), kCastServiceType);
    device_lister_->Start();
    device_lister_->DiscoverNewDevices();
  }

  // ServiceDiscoveryDeviceLister::Delegate implementation:
  void OnDeviceChanged(const std::string& service_type,
                       bool added,
                       const ServiceDescription& service_description) override {
    runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CastDeviceProvider::OnDeviceChanged, provider_,
                       service_type, added, service_description));
  }

  void OnDeviceRemoved(const std::string& service_type,
                       const std::string& service_name) override {
    runner_->PostTask(FROM_HERE,
                      base::BindOnce(&CastDeviceProvider::OnDeviceRemoved,
                                     provider_, service_type, service_name));
  }

  void OnDeviceCacheFlushed(const std::string& service_type) override {
    runner_->PostTask(FROM_HERE,
                      base::BindOnce(&CastDeviceProvider::OnDeviceCacheFlushed,
                                     provider_, service_type));
  }

 private:
  // The device provider to notify of device changes.
  base::WeakPtr<CastDeviceProvider> provider_;
  // Runner for the thread the WeakPtr was created on (this is where device
  // messages will be posted).
  scoped_refptr<base::SingleThreadTaskRunner> runner_;
  scoped_refptr<ServiceDiscoverySharedClient> service_discovery_client_;
  std::unique_ptr<ServiceDiscoveryDeviceLister> device_lister_;
};

CastDeviceProvider::CastDeviceProvider() {}

CastDeviceProvider::~CastDeviceProvider() {}

void CastDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  if (!lister_delegate_) {
    lister_delegate_.reset(new DeviceListerDelegate(
        weak_factory_.GetWeakPtr(), base::ThreadTaskRunnerHandle::Get()));
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&DeviceListerDelegate::StartDiscovery,
                                  lister_delegate_->AsWeakPtr()));
  }
  std::set<net::HostPortPair> targets;
  for (const auto& device_entry : device_info_map_)
    targets.insert(net::HostPortPair(device_entry.first, kCastInspectPort));
  tcp_provider_ = new TCPDeviceProvider(targets);
  tcp_provider_->QueryDevices(callback);
}

void CastDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                         const DeviceInfoCallback& callback) {
  auto it_device = device_info_map_.find(serial);
  if (it_device == device_info_map_.end())
    return;
  callback.Run(it_device->second);
}

void CastDeviceProvider::OpenSocket(const std::string& serial,
                                    const std::string& socket_name,
                                    const SocketCallback& callback) {
  tcp_provider_->OpenSocket(serial, socket_name, callback);
}

void CastDeviceProvider::OnDeviceChanged(
    const std::string& service_type,
    bool added,
    const ServiceDescription& service_description) {
  VLOG(1) << "Device " << (added ? "added: " : "changed: ")
          << service_description.service_name;
  if (service_description.service_type() != kCastServiceType)
    return;
  const net::IPAddress& ip_address = service_description.ip_address;
  if (!ip_address.IsValid()) {
    // An invalid IP address is not queryable.
    return;
  }
  const std::string& name = service_description.service_name;
  std::string host = ip_address.ToString();
  service_hostname_map_[name] = host;
  device_info_map_[host] = ServiceDescriptionToDeviceInfo(service_description);
}

void CastDeviceProvider::OnDeviceRemoved(const std::string& service_type,
                                         const std::string& service_name) {
  VLOG(1) << "Device removed: " << service_name;
  auto it = service_hostname_map_.find(service_name);
  if (it == service_hostname_map_.end())
    return;
  const std::string& hostname = it->second;
  device_info_map_.erase(hostname);
  service_hostname_map_.erase(it);
}

void CastDeviceProvider::OnDeviceCacheFlushed(const std::string& service_type) {
  VLOG(1) << "Device cache flushed";
  service_hostname_map_.clear();
  device_info_map_.clear();
}
