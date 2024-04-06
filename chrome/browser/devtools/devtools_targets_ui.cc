// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_targets_ui.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/serialize_host_descriptions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/media_router/browser/presentation/local_presentation_manager.h"
#include "components/media_router/browser/presentation/local_presentation_manager_factory.h"
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
 void DevToolsAgentHostCreated(DevToolsAgentHost* host) override;
 void DevToolsAgentHostDestroyed(DevToolsAgentHost* host) override;

 void ScheduleUpdate();
 void UpdateTargets();

 bool AllowDevToolsFor(DevToolsAgentHost* host);

 raw_ptr<Profile> profile_;
 raw_ptr<media_router::LocalPresentationManager> local_presentation_manager_;
 std::unique_ptr<base::OneShotTimer> timer_;
 base::WeakPtrFactory<LocalTargetsUIHandler> weak_factory_{this};
};

LocalTargetsUIHandler::LocalTargetsUIHandler(const Callback& callback,
                                             Profile* profile)
    : DevToolsTargetsUIHandler(kTargetSourceLocal, callback),
      profile_(profile),
      local_presentation_manager_(
          media_router::LocalPresentationManagerFactory::
              GetOrCreateForBrowserContext(profile_)) {
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
  timer_ = std::make_unique<base::OneShotTimer>();
  timer_->Start(FROM_HERE, base::Milliseconds(kUpdateDelay),
                base::BindOnce(&LocalTargetsUIHandler::UpdateTargets,
                               base::Unretained(this)));
}

void LocalTargetsUIHandler::UpdateTargets() {
  content::DevToolsAgentHost::List targets =
      DevToolsAgentHost::GetOrCreateAll();

  std::vector<HostDescriptionNode> hosts;
  hosts.reserve(targets.size());
  targets_.clear();
  for (const scoped_refptr<DevToolsAgentHost>& host : targets) {
    if (!AllowDevToolsFor(host.get()))
      continue;

    targets_[host->GetId()] = host;
    hosts.push_back({host->GetId(), host->GetParentId(),
                     base::Value(Serialize(host.get()))});
  }

  SendSerializedTargets(
      base::Value(SerializeHostDescriptions(std::move(hosts), kGuestList)));
}

bool LocalTargetsUIHandler::AllowDevToolsFor(DevToolsAgentHost* host) {
  return local_presentation_manager_->IsLocalPresentation(
             host->GetWebContents()) ||
         (Profile::FromBrowserContext(host->GetBrowserContext()) == profile_ &&
          DevToolsWindow::AllowDevToolsFor(profile_, host->GetWebContents()));
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

  const raw_ptr<Profile> profile_;
  const raw_ptr<DevToolsAndroidBridge> android_bridge_;

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

  base::Value::List device_list;
  for (const auto& device_ptr : devices) {
    DevToolsAndroidBridge::RemoteDevice* device = device_ptr.get();
    base::Value::Dict device_data;
    device_data.Set(kAdbModelField, device->model());
    device_data.Set(kAdbSerialField, device->serial());
    device_data.Set(kAdbConnectedField, device->is_connected());
    std::string device_id = base::StringPrintf(
        kAdbDeviceIdFormat,
        device->serial().c_str());
    device_data.Set(kTargetIdField, device_id);
    base::Value::List browser_list;

    DevToolsAndroidBridge::RemoteBrowsers& browsers = device->browsers();
    for (const auto& browser_ptr : browsers) {
      DevToolsAndroidBridge::RemoteBrowser* browser = browser_ptr.get();
      base::Value::Dict browser_data;
      browser_data.Set(kAdbBrowserNameField, browser->display_name());
      browser_data.Set(kAdbBrowserUserField, browser->user());
      browser_data.Set(kAdbBrowserVersionField, browser->version());
      DevToolsAndroidBridge::RemoteBrowser::ParsedVersion parsed =
          browser->GetParsedVersion();
      browser_data.Set(kAdbBrowserChromeVersionField,
                       browser->IsChrome() && !parsed.empty() ? parsed[0] : 0);
      std::string browser_id = browser->GetId();
      browser_data.Set(kTargetIdField, browser_id);
      browser_data.Set(kTargetSourceField, source_id());

      base::Value::List page_list;
      remote_browsers_[browser_id] = browser;
      for (const auto& page : browser->pages()) {
        scoped_refptr<DevToolsAgentHost> host = page->CreateTarget();
        base::Value::Dict target_data = Serialize(host.get());
        // Pass the screen size in the target object to make sure that
        // the caching logic does not prevent the target item from updating
        // when the screen size changes.
        gfx::Size screen_size = device->screen_size();
        target_data.Set(kAdbScreenWidthField, screen_size.width());
        target_data.Set(kAdbScreenHeightField, screen_size.height());
        targets_[host->GetId()] = host;
        page_list.Append(std::move(target_data));
      }
      browser_data.Set(kAdbPagesList, std::move(page_list));
      browser_list.Append(std::move(browser_data));
    }

    device_data.Set(kAdbBrowsersList, std::move(browser_list));
    device_list.Append(std::move(device_data));
  }
  SendSerializedTargets(base::Value(std::move(device_list)));
}

} // namespace

// DevToolsTargetsUIHandler ---------------------------------------------------

DevToolsTargetsUIHandler::DevToolsTargetsUIHandler(const std::string& source_id,
                                                   Callback callback)
    : source_id_(source_id), callback_(std::move(callback)) {}

DevToolsTargetsUIHandler::~DevToolsTargetsUIHandler() = default;

// static
std::unique_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForLocal(
    DevToolsTargetsUIHandler::Callback callback,
    Profile* profile) {
  return std::unique_ptr<DevToolsTargetsUIHandler>(
      new LocalTargetsUIHandler(callback, profile));
}

// static
std::unique_ptr<DevToolsTargetsUIHandler>
DevToolsTargetsUIHandler::CreateForAdb(
    DevToolsTargetsUIHandler::Callback callback,
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

base::Value::Dict DevToolsTargetsUIHandler::Serialize(DevToolsAgentHost* host) {
  base::Value::Dict target_data;
  target_data.Set(kTargetSourceField, source_id_);
  target_data.Set(kTargetIdField, host->GetId());
  target_data.Set(kTargetTypeField, host->GetType());
  target_data.Set(kAttachedField, host->IsAttached());
  target_data.Set(kUrlField, host->GetURL().spec());
  target_data.Set(kNameField, host->GetTitle());
  target_data.Set(kFaviconUrlField, host->GetFaviconURL().spec());
  target_data.Set(kDescriptionField, host->GetDescription());
  return target_data;
}

void DevToolsTargetsUIHandler::SendSerializedTargets(const base::Value& list) {
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
  base::Value::Dict result;
  for (const auto& status_pair : status) {
    base::Value::Dict port_status_dict;
    const PortStatusMap& port_status_map = status_pair.second;
    for (const auto& p : port_status_map) {
      port_status_dict.Set(base::NumberToString(p.first), p.second);
    }

    base::Value::Dict device_status_dict;
    device_status_dict.Set(kPortForwardingPorts, std::move(port_status_dict));
    device_status_dict.Set(kPortForwardingBrowserId,
                           status_pair.first->GetId());

    std::string device_id = base::StringPrintf(
        kAdbDeviceIdFormat, status_pair.first->serial().c_str());
    result.Set(device_id, std::move(device_status_dict));
  }
  callback_.Run(base::Value(std::move(result)));
}
