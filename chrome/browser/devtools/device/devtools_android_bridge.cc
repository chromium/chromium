// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/devtools_android_bridge.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/device/adb/adb_device_provider.h"
#include "chrome/browser/devtools/device/port_forwarding_controller.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/remote_debugging_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/devtools/device/cast_device_provider.h"
#endif

using content::BrowserThread;
using content::DevToolsAgentHost;

namespace {

const char kNewPageRequestWithURL[] = "/json/new?%s";
const char kChromeDiscoveryURL[] = "localhost:9222";
const char kNodeDiscoveryURL[] = "localhost:9229";

bool BrowserIdFromString(const std::string& browser_id_str,
                         std::string* serial,
                         std::string* browser_id) {
  size_t colon_pos = browser_id_str.find(':');
  if (colon_pos == std::string::npos)
    return false;
  *serial = browser_id_str.substr(0, colon_pos);
  *browser_id = browser_id_str.substr(colon_pos + 1);
  return true;
}

}  // namespace

// static
DevToolsAndroidBridge::Factory* DevToolsAndroidBridge::Factory::GetInstance() {
  return base::Singleton<DevToolsAndroidBridge::Factory>::get();
}

// static
DevToolsAndroidBridge* DevToolsAndroidBridge::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<DevToolsAndroidBridge*>(
      GetInstance()->GetServiceForBrowserContext(profile->GetOriginalProfile(),
                                                 true));
}

DevToolsAndroidBridge::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "DevToolsAndroidBridge",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

DevToolsAndroidBridge::Factory::~Factory() {}

KeyedService* DevToolsAndroidBridge::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  return new DevToolsAndroidBridge(profile);
}

void DevToolsAndroidBridge::Shutdown() {
  // Needed for Chrome_DevToolsADBThread to shut down gracefully in tests.
  device_manager_.reset();
}

scoped_refptr<content::DevToolsAgentHost>
DevToolsAndroidBridge::GetBrowserAgentHost(
    scoped_refptr<RemoteBrowser> browser) {
  auto it = device_map_.find(browser->serial());
  if (it == device_map_.end())
    return nullptr;

  return DevToolsDeviceDiscovery::CreateBrowserAgentHost(it->second, browser);
}

void DevToolsAndroidBridge::SendJsonRequest(const std::string& browser_id_str,
                                            const std::string& url,
                                            JsonRequestCallback callback) {
  std::string serial;
  std::string browser_id;
  if (!BrowserIdFromString(browser_id_str, &serial, &browser_id)) {
    std::move(callback).Run(net::ERR_FAILED, std::string());
    return;
  }
  auto it = device_map_.find(serial);
  if (it == device_map_.end()) {
    std::move(callback).Run(net::ERR_FAILED, std::string());
    return;
  }
  it->second->SendJsonRequest(browser_id, url, std::move(callback));
}

void DevToolsAndroidBridge::OpenRemotePage(scoped_refptr<RemoteBrowser> browser,
                                           const std::string& input_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GURL gurl(input_url);
  if (!gurl.is_valid()) {
    gurl = GURL("http://" + input_url);
    if (!gurl.is_valid())
      return;
  }
  std::string url = gurl.spec();
  RemoteBrowser::ParsedVersion parsed_version = browser->GetParsedVersion();

  std::string query = base::EscapeQueryParamValue(url, false /* use_plus */);
  std::string request =
      base::StringPrintf(kNewPageRequestWithURL, query.c_str());
  SendJsonRequest(browser->GetId(), request, base::DoNothing());
}

DevToolsAndroidBridge::DevToolsAndroidBridge(Profile* profile)
    : profile_(profile),
      device_manager_(AndroidDeviceManager::Create()),
      port_forwarding_controller_(new PortForwardingController(profile)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::BindRepeating(&DevToolsAndroidBridge::CreateDeviceProviders,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kDevToolsTCPDiscoveryConfig,
      base::BindRepeating(&DevToolsAndroidBridge::CreateDeviceProviders,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kDevToolsDiscoverTCPTargetsEnabled,
      base::BindRepeating(&DevToolsAndroidBridge::CreateDeviceProviders,
                          base::Unretained(this)));
  base::Value::List target_discovery;
  target_discovery.Append(kChromeDiscoveryURL);
  target_discovery.Append(kNodeDiscoveryURL);
  profile->GetPrefs()->SetDefaultPrefValue(
      prefs::kDevToolsTCPDiscoveryConfig,
      base::Value(std::move(target_discovery)));
  CreateDeviceProviders();
}

void DevToolsAndroidBridge::AddDeviceListListener(
    DeviceListListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool polling_was_off = !NeedsDeviceListPolling();
  device_list_listeners_.push_back(listener);
  if (polling_was_off)
    StartDeviceListPolling();
}

void DevToolsAndroidBridge::RemoveDeviceListListener(
    DeviceListListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = base::ranges::find(device_list_listeners_, listener);
  CHECK(it != device_list_listeners_.end(), base::NotFatalUntil::M130);
  device_list_listeners_.erase(it);
  if (!NeedsDeviceListPolling())
    StopDeviceListPolling();
}

void DevToolsAndroidBridge::AddDeviceCountListener(
    DeviceCountListener* listener) {
  device_count_listeners_.push_back(listener);
  if (device_count_listeners_.size() == 1)
    StartDeviceCountPolling();
}

void DevToolsAndroidBridge::RemoveDeviceCountListener(
    DeviceCountListener* listener) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = base::ranges::find(device_count_listeners_, listener);
  CHECK(it != device_count_listeners_.end(), base::NotFatalUntil::M130);
  device_count_listeners_.erase(it);
  if (device_count_listeners_.empty())
    StopDeviceCountPolling();
}

void DevToolsAndroidBridge::AddPortForwardingListener(
    PortForwardingListener* listener) {
  bool polling_was_off = !NeedsDeviceListPolling();
  port_forwarding_listeners_.push_back(listener);
  if (polling_was_off)
    StartDeviceListPolling();
}

void DevToolsAndroidBridge::RemovePortForwardingListener(
    PortForwardingListener* listener) {
  auto it = base::ranges::find(port_forwarding_listeners_, listener);
  CHECK(it != port_forwarding_listeners_.end(), base::NotFatalUntil::M130);
  port_forwarding_listeners_.erase(it);
  if (!NeedsDeviceListPolling())
    StopDeviceListPolling();
}

DevToolsAndroidBridge::~DevToolsAndroidBridge() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(device_list_listeners_.empty());
  DCHECK(device_count_listeners_.empty());
  DCHECK(port_forwarding_listeners_.empty());
}

void DevToolsAndroidBridge::StartDeviceListPolling() {
  device_discovery_ = std::make_unique<DevToolsDeviceDiscovery>(
      device_manager_.get(),
      base::BindRepeating(&DevToolsAndroidBridge::ReceivedDeviceList,
                          base::Unretained(this)));
  if (!task_scheduler_.is_null())
    device_discovery_->SetScheduler(task_scheduler_);
}

void DevToolsAndroidBridge::StopDeviceListPolling() {
  device_discovery_.reset();
  device_map_.clear();
  port_forwarding_controller_->CloseAllConnections();
}

bool DevToolsAndroidBridge::NeedsDeviceListPolling() {
  return !device_list_listeners_.empty() || !port_forwarding_listeners_.empty();
}

void DevToolsAndroidBridge::ReceivedDeviceList(
    const CompleteDevices& complete_devices) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  device_map_.clear();
  RemoteDevices remote_devices;
  for (const auto& pair : complete_devices) {
    device_map_[pair.first->serial()] = pair.first;
    remote_devices.push_back(pair.second);
  }

  DeviceListListeners copy(device_list_listeners_);
  for (DevToolsAndroidBridge::DeviceListListener* listener : copy) {
    listener->DeviceListChanged(remote_devices);
  }

  ForwardingStatus status =
      port_forwarding_controller_->DeviceListChanged(complete_devices);
  PortForwardingListeners forwarding_listeners(port_forwarding_listeners_);
  for (DevToolsAndroidBridge::PortForwardingListener* listener :
       forwarding_listeners) {
    listener->PortStatusChanged(status);
  }
}

void DevToolsAndroidBridge::StartDeviceCountPolling() {
  device_count_callback_.Reset(base::BindRepeating(
      &DevToolsAndroidBridge::ReceivedDeviceCount, AsWeakPtr()));
  RequestDeviceCount(device_count_callback_.callback());
}

void DevToolsAndroidBridge::StopDeviceCountPolling() {
  device_count_callback_.Cancel();
}

void DevToolsAndroidBridge::RequestDeviceCount(
    base::RepeatingCallback<void(int)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (device_count_listeners_.empty() || callback.IsCancelled())
    return;

  device_manager_->CountDevices(std::move(callback));
}

void DevToolsAndroidBridge::ReceivedDeviceCount(int count) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DeviceCountListeners copy(device_count_listeners_);
  for (DevToolsAndroidBridge::DeviceCountListener* listener : copy) {
    listener->DeviceCountChanged(count);
  }

  if (device_count_listeners_.empty())
    return;

  task_scheduler_.Run(base::BindOnce(&DevToolsAndroidBridge::RequestDeviceCount,
                                     AsWeakPtr(),
                                     device_count_callback_.callback()));
}

static std::set<net::HostPortPair> ParseTargetDiscoveryPreferenceValue(
    const base::Value::List* preferenceValue) {
  std::set<net::HostPortPair> targets;
  if (!preferenceValue || preferenceValue->empty())
    return targets;
  for (const auto& address : *preferenceValue) {
    if (!address.is_string())
      continue;
    net::HostPortPair target =
        net::HostPortPair::FromString(address.GetString());
    if (target.IsEmpty()) {
      LOG(WARNING) << "Invalid target: " << address;
      continue;
    }
    targets.insert(target);
  }
  return targets;
}

static scoped_refptr<TCPDeviceProvider> CreateTCPDeviceProvider(
    const base::Value::List* targetDiscoveryConfig) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::set<net::HostPortPair> targets =
      ParseTargetDiscoveryPreferenceValue(targetDiscoveryConfig);
  if (targets.empty() &&
      !command_line->HasSwitch(switches::kRemoteDebuggingTargets))
    return nullptr;
  std::string value =
      command_line->GetSwitchValueASCII(switches::kRemoteDebuggingTargets);
  std::vector<std::string> addresses = base::SplitString(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& address : addresses) {
    net::HostPortPair target = net::HostPortPair::FromString(address);
    if (target.IsEmpty()) {
      LOG(WARNING) << "Invalid target: " << address;
      continue;
    }
    targets.insert(target);
  }
  if (targets.empty())
    return nullptr;
  return new TCPDeviceProvider(targets);
}

void DevToolsAndroidBridge::CreateDeviceProviders() {
  AndroidDeviceManager::DeviceProviders device_providers;
  PrefService* service = profile_->GetPrefs();
  const base::Value::List* targets =
      service->GetBoolean(prefs::kDevToolsDiscoverTCPTargetsEnabled)
          ? std::addressof(service->GetList(prefs::kDevToolsTCPDiscoveryConfig))
          : nullptr;
  scoped_refptr<TCPDeviceProvider> provider = CreateTCPDeviceProvider(targets);
  if (tcp_provider_callback_)
    tcp_provider_callback_.Run(provider);

  if (provider)
    device_providers.push_back(provider);

#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
  device_providers.push_back(new CastDeviceProvider());
#endif

  device_providers.push_back(new AdbDeviceProvider());

  const PrefService::Preference* pref =
      service->FindPreference(prefs::kDevToolsDiscoverUsbDevicesEnabled);
  const base::Value* pref_value = pref->GetValue();

  if (pref_value->is_bool() && pref_value->GetBool()) {
    device_providers.push_back(new UsbDeviceProvider(profile_));
  }

  device_manager_->SetDeviceProviders(device_providers);
  if (NeedsDeviceListPolling()) {
    StopDeviceListPolling();
    StartDeviceListPolling();
  }
}

void DevToolsAndroidBridge::set_tcp_provider_callback_for_test(
    TCPProviderCallback callback) {
  tcp_provider_callback_ = std::move(callback);
  CreateDeviceProviders();
}

void DevToolsAndroidBridge::set_usb_device_manager_for_test(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_usb_manager) {
  device_manager_->set_usb_device_manager_for_test(std::move(fake_usb_manager));
}
