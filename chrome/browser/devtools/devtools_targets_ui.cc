// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_targets_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/serialize_host_descriptions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_observer.h"

using content::BrowserThread;
using content::DevToolsAgentHost;

namespace {

const char kTargetSourceField[]  = "source";
const char kTargetSourceLocal[]  = "local";
const char kTargetSourceRemote[]  = "remote";

const char kTargetIdField[]  = "id";
const char kTargetTypeField[]  = "type";
const char kAttachedField[]  = "attached";
const char kUrlField[]  = "url";
const char kNameField[]  = "name";
const char kFaviconUrlField[] = "faviconUrl";
const char kDescriptionField[] = "description";

const char kGuestList[] = "guests";

const char kAdbModelField[] = "adbModel";
const char kAdbConnectedField[] = "adbConnected";
const char kAdbSerialField[] = "adbSerial";
const char kAdbBrowsersList[] = "browsers";
const char kAdbDeviceIdFormat[] = "device:%s";

const char kAdbBrowserNameField[] = "adbBrowserName";
const char kAdbBrowserUserField[] = "adbBrowserUser";
const char kAdbBrowserVersionField[] = "adbBrowserVersion";
const char kAdbBrowserChromeVersionField[] = "adbBrowserChromeVersion";
const char kAdbPagesList[] = "pages";

const char kAdbScreenWidthField[] = "adbScreenWidth";
const char kAdbScreenHeightField[] = "adbScreenHeight";

const char kPortForwardingPorts[] = "ports";
const char kPortForwardingBrowserId[] = "browserId";

// CancelableTimer ------------------------------------------------------------

class CancelableTimer {
 public:
  CancelableTimer(base::Closure callback, base::TimeDelta delay)
      : callback_(callback) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CancelableTimer::Fire, weak_factory_.GetWeakPtr()),
        delay);
  }

 private:
  void Fire() { callback_.Run(); }

  base::Closure callback_;
  base::WeakPtrFactory<CancelableTimer> weak_factory_{this};
};

// LocalTargetsUIHandler ---------------------------------------------

class LocalTargetsUIHandler : public DevToolsTargetsUIHandler,
                              public content::DevToolsAgentHostObserver {
 public:
  LocalTargetsUIHandler(const Callback& callback, Profile* profile);
  ~LocalTargetsUIHandler() override;

  // DevToolsTargetsUIHandler overrides.
  void ForceUpdate() override;

private:
 // content::DevToolsAgentHostObserver overrides.
 bool ShouldForceDevToolsAgentHostCreation() override;
 void DevToolsAgentHostCreated(DevToolsAgentHost* agent_host) override;
 void DevToolsAgentHostDestroyed(DevToolsAgentHost* agent_host) override;

 void ScheduleUpdate();
 void UpdateTargets();

 Profile* profile_;
 std::unique_ptr<CancelableTimer> timer_;
 base::WeakPtrFactory<LocalTargetsUIHandler> weak_factory_{this};
};

LocalTargetsUIHandler::LocalTargetsUIHandler(const Callback& callback,
                                             Profile* profile)
    : DevToolsTargetsUIHandler(kTargetSourceLocal, callback),
      profile_(profile) {
  DevToolsAgentHost::AddObserver(this);
  UpdateTargets();
}

LocalTargetsUIHandler::~LocalTargetsUIHandler() {
  DevToolsAgentHost::RemoveObserver(this);
}

bool LocalTargetsUIHandler::ShouldForceDevToolsAgentHostCreation() {
  return true;
}

void LocalTargetsUIHandler::DevToolsAgentHostCreated(DevToolsAgentHost*) {
  ScheduleUpdate();
}

void LocalTargetsUIHandler::DevToolsAgentHostDestroyed(DevToolsAgentHost*) {
  ScheduleUpdate();
}

void LocalTargetsUIHandler::ForceUpdate() {
  ScheduleUpdate();
}

void LocalTargetsUIHandler::ScheduleUpdate() {
  const int kUpdateDelay = 100;
  timer_.reset(
      new CancelableTimer(
          base::Bind(&LocalTargetsUIHandler::UpdateTargets,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(kUpdateDelay)));
}

void LocalTargetsUIHandler::UpdateTargets() {
  content::DevToolsAgentHost::List targets =
      DevToolsAgentHost::GetOrCreateAll();

  std::vector<HostDescriptionNode> hosts;
  hosts.reserve(targets.size());
  targets_.clear();
  for (const scoped_refptr<DevToolsAgentHost>& host : targets) {
    if (Profile::FromBrowserContext(host->GetBrowserContext()) != profile_)
      continue;
    if (!DevToolsWindow::AllowDevToolsFor(profile_, host->GetWebContents()))
      continue;
    targets_[host->GetId()] = host;
    hosts.push_back({host->GetId(), host->GetParentId(),
                     std::move(*Serialize(host.get()))});
  }

  SendSerializedTargets(
      SerializeHostDescriptions(std::move(hosts), kGuestList));
}

// AdbTargetsUIHandler --------------------------------------------------------

class AdbTargetsUIHandler
    : public DevToolsTargetsUIHandler,
      public DevToolsAndroidBridge::DeviceListListener {
 public:
  AdbTargetsUIHandler(const Callback& callback, Profile* profile);
  ~AdbTargetsUIHandler() override;

  void Open(const std::string& browser_id, const std::string& url) override;

  scoped_refptr<DevToolsAgentHost> GetBrowserAgentHost(
      const std::string& browser_id) override;

 private:
  // DevToolsAndroidBridge::Listener overrides.
  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override;

  DevToolsAndroidBridge* GetAndroidBridge();

  Profile* const profile_;
  DevToolsAndroidBridge* const android_bridge_;

  typedef std::map<std::string,
      scoped_refptr<DevToolsAndroidBridge::RemoteBrowser> > RemoteBrowsers;
  RemoteBrowsers remote_browsers_;
};

AdbTargetsUIHandler::AdbTargetsUIHandler(const Callback& callback,
                                         Profile* profile)
    : DevToolsTargetsUIHandler(kTargetSourceRemote, callback),
      profile_(profile),
      android_bridge_(
          DevToolsAndroidBridge::Factory::GetForProfile(profile_)) {
  if (android_bridge_)
    android_bridge_->AddDeviceListListener(this);
}

AdbTargetsUIHandler::~AdbTargetsUIHandler() {
  if (android_bridge_)
    android_bridge_->RemoveDeviceListListener(this);
}

void AdbTargetsUIHandler::Open(const std::string& browser_id,
                               const std::string& url) {
  auto it = remote_browsers_.find(browser_id);
  if (it != remote_browsers_.end() && android_bridge_)
    android_bridge_->OpenRemotePage(it->second, url);
}

scoped_refptr<DevToolsAgentHost>
AdbTargetsUIHandler::GetBrowserAgentHost(
    const std::string& browser_id) {
  auto it = remote_browsers_.find(browser_id);
  if (it == remote_browsers_.end() || !android_bridge_)
    return nullptr;

  return android_bridge_->GetBrowserAgentHost(it->second);
}

void AdbTargetsUIHandler::DeviceListChanged(
    const DevToolsAndroidBridge::RemoteDevices& devices) {
  remote_browsers_.clear();
  targets_.clear();
  if (!android_bridge_)
    return;

  base::ListValue device_list;
  for (auto dit = devices.begin(); dit != devices.end(); ++dit) {
    DevToolsAndroidBridge::RemoteDevice* device = dit->get();
    std::unique_ptr<base::DictionaryValue> device_data(
        new base::DictionaryValue());
    device_data->SetString(kAdbModelField, device->model());
    device_data->SetString(kAdbSerialField, device->serial());
    device_data->SetBoolean(kAdbConnectedField, device->is_connected());
    std::string device_id = base::StringPrintf(
        kAdbDeviceIdFormat,
        device->serial().c_str());
    device_data->SetString(kTargetIdField, device_id);
    auto browser_list = std::make_unique<base::ListValue>();

    DevToolsAndroidBridge::RemoteBrowsers& browsers = device->browsers();
    for (auto bit = browsers.begin(); bit != browsers.end(); ++bit) {
      DevToolsAndroidBridge::RemoteBrowser* browser = bit->get();
      std::unique_ptr<base::DictionaryValue> browser_data(
          new base::DictionaryValue());
      browser_data->SetString(kAdbBrowserNameField, browser->display_name());
      browser_data->SetString(kAdbBrowserUserField, browser->user());
      browser_data->SetString(kAdbBrowserVersionField, browser->version());
      DevToolsAndroidBridge::RemoteBrowser::ParsedVersion parsed =
          browser->GetParsedVersion();
      browser_data->SetInteger(
          kAdbBrowserChromeVersionField,
          browser->IsChrome() && !parsed.empty() ? parsed[0] : 0);
      std::string browser_id = browser->GetId();
      browser_data->SetString(kTargetIdField, browser_id);
      browser_data->SetString(kTargetSourceField, source_id());

      auto page_list = std::make_unique<base::ListValue>();
      remote_browsers_[browser_id] = browser;
      for (const auto& page : browser->pages()) {
        scoped_refptr<DevToolsAgentHost> host = page->CreateTarget();
        std::unique_ptr<base::DictionaryValue> target_data =
            Serialize(host.get());
        // Pass the screen size in the target object to make sure that
        // the caching logic does not prevent the target item from updating
        // when the screen size changes.
        gfx::Size screen_size = device->screen_size();
        target_data->SetInteger(kAdbScreenWidthField, screen_size.width());
        target_data->SetInteger(kAdbScreenHeightField, screen_size.height());
        targets_[host->GetId()] = host;
        page_list->Append(std::move(target_data));
      }
      browser_data->Set(kAdbPagesList, std::move(page_list));
      browser_list->Append(std::move(browser_data));
    }

    device_data->Set(kAdbBrowsersList, std::move(browser_list));
    device_list.Append(std::move(device_data));
  }
  SendSerializedTargets(device_list);
}

} // namespace

// DevToolsTargetsUIHandler ---------------------------------------------------

DevToolsTargetsUIHandler::DevToolsTargetsUIHandler(
    const std::string& source_id,
    const Callback& callback)
    : source_id_(source_id),
      callback_(callback) {
}

DevToolsTargetsUIHandler::~DevToolsTargetsUIHandler() {
}

// static
std::unique_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForLocal(
    const DevToolsTargetsUIHandler::Callback& callback,
    Profile* profile) {
  return std::unique_ptr<DevToolsTargetsUIHandler>(
      new LocalTargetsUIHandler(callback, profile));
}

// static
std::unique_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForAdb(
    const DevToolsTargetsUIHandler::Callback& callback,
    Profile* profile) {
  return std::unique_ptr<DevToolsTargetsUIHandler>(
      new AdbTargetsUIHandler(callback, profile));
}

scoped_refptr<DevToolsAgentHost> DevToolsTargetsUIHandler::GetTarget(
    const std::string& target_id) {
  auto it = targets_.find(target_id);
  if (it != targets_.end())
    return it->second;
  return nullptr;
}

void DevToolsTargetsUIHandler::Open(const std::string& browser_id,
                                    const std::string& url) {
}

scoped_refptr<DevToolsAgentHost>
DevToolsTargetsUIHandler::GetBrowserAgentHost(const std::string& browser_id) {
  return nullptr;
}

std::unique_ptr<base::DictionaryValue> DevToolsTargetsUIHandler::Serialize(
    DevToolsAgentHost* host) {
  auto target_data = std::make_unique<base::DictionaryValue>();
  target_data->SetString(kTargetSourceField, source_id_);
  target_data->SetString(kTargetIdField, host->GetId());
  target_data->SetString(kTargetTypeField, host->GetType());
  target_data->SetBoolean(kAttachedField, host->IsAttached());
  target_data->SetString(kUrlField, host->GetURL().spec());
  target_data->SetString(kNameField, host->GetTitle());
  target_data->SetString(kFaviconUrlField, host->GetFaviconURL().spec());
  target_data->SetString(kDescriptionField, host->GetDescription());
  return target_data;
}

void DevToolsTargetsUIHandler::SendSerializedTargets(
    const base::ListValue& list) {
  callback_.Run(source_id_, list);
}

void DevToolsTargetsUIHandler::ForceUpdate() {
}

// PortForwardingStatusSerializer ---------------------------------------------

PortForwardingStatusSerializer::PortForwardingStatusSerializer(
    const Callback& callback, Profile* profile)
      : callback_(callback),
        profile_(profile) {
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->AddPortForwardingListener(this);
}

PortForwardingStatusSerializer::~PortForwardingStatusSerializer() {
  DevToolsAndroidBridge* android_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (android_bridge)
    android_bridge->RemovePortForwardingListener(this);
}

void PortForwardingStatusSerializer::PortStatusChanged(
    const ForwardingStatus& status) {
  base::DictionaryValue result;
  for (auto sit = status.begin(); sit != status.end(); ++sit) {
    auto port_status_dict = std::make_unique<base::DictionaryValue>();
    const PortStatusMap& port_status_map = sit->second;
    for (auto it = port_status_map.begin(); it != port_status_map.end(); ++it) {
      port_status_dict->SetInteger(base::NumberToString(it->first), it->second);
    }

    auto device_status_dict = std::make_unique<base::DictionaryValue>();
    device_status_dict->Set(kPortForwardingPorts, std::move(port_status_dict));
    device_status_dict->SetString(kPortForwardingBrowserId,
                                  sit->first->GetId());

    std::string device_id = base::StringPrintf(
        kAdbDeviceIdFormat,
        sit->first->serial().c_str());
    result.Set(device_id, std::move(device_status_dict));
  }
  callback_.Run(result);
}
