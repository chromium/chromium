// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/chrome_devtools_session.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol/target_handler.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/grit/browser_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#endif

using content::DevToolsAgentHost;

const char ChromeDevToolsManagerDelegate::kTypeApp[] = "app";
const char ChromeDevToolsManagerDelegate::kTypeBackgroundPage[] =
    "background_page";

namespace {

bool GetExtensionInfo(content::WebContents* wc,
                      std::string* name,
                      std::string* type) {
  Profile* profile = Profile::FromBrowserContext(wc->GetBrowserContext());
  if (!profile)
    return false;
  const extensions::Extension* extension =
      extensions::ProcessManager::Get(profile)->GetExtensionForWebContents(wc);
  if (!extension)
    return false;
  extensions::ExtensionHost* extension_host =
      extensions::ProcessManager::Get(profile)->GetBackgroundHostForExtension(
          extension->id());
  if (extension_host && extension_host->host_contents() == wc) {
    *name = extension->name();
    *type = ChromeDevToolsManagerDelegate::kTypeBackgroundPage;
    return true;
  }
  if (extension->is_hosted_app() || extension->is_legacy_packaged_app() ||
      extension->is_platform_app()) {
    *name = extension->name();
    *type = ChromeDevToolsManagerDelegate::kTypeApp;
    return true;
  }
  return false;
}

ChromeDevToolsManagerDelegate* g_instance;

}  // namespace

// static
ChromeDevToolsManagerDelegate* ChromeDevToolsManagerDelegate::GetInstance() {
  return g_instance;
}

ChromeDevToolsManagerDelegate::ChromeDevToolsManagerDelegate() {
  DCHECK(!g_instance);
  g_instance = this;
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::DevToolsAgentHost* agent_host) {
  DevToolsWindow::OpenDevToolsWindow(agent_host, nullptr);
}

void ChromeDevToolsManagerDelegate::HandleCommand(
    DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client,
    const std::string& method,
    const std::string& message,
    NotHandledCallback callback) {
  if (sessions_.find(client) == sessions_.end()) {
    std::move(callback).Run(message);
    // This should not happen, but happens. NOTREACHED tries to get
    // a repro in some test.
    NOTREACHED();
    return;
  }
  sessions_[client]->HandleCommand(method, message, std::move(callback));
}

std::string ChromeDevToolsManagerDelegate::GetTargetType(
    content::WebContents* web_contents) {
  if (base::Contains(AllTabContentses(), web_contents))
    return DevToolsAgentHost::kTypePage;

  std::string extension_name;
  std::string extension_type;
  if (!GetExtensionInfo(web_contents, &extension_name, &extension_type))
    return DevToolsAgentHost::kTypeOther;
  return extension_type;
}

std::string ChromeDevToolsManagerDelegate::GetTargetTitle(
    content::WebContents* web_contents) {
  std::string extension_name;
  std::string extension_type;
  if (!GetExtensionInfo(web_contents, &extension_name, &extension_type))
    return std::string();
  return extension_name;
}

bool ChromeDevToolsManagerDelegate::AllowInspectingRenderFrameHost(
    content::RenderFrameHost* rfh) {
  Profile* profile =
      Profile::FromBrowserContext(rfh->GetProcess()->GetBrowserContext());
  return AllowInspection(profile, extensions::ProcessManager::Get(profile)
                                      ->GetExtensionForRenderFrameHost(rfh));
}

// static
bool ChromeDevToolsManagerDelegate::AllowInspection(
    Profile* profile,
    content::WebContents* web_contents) {
  const extensions::Extension* extension = nullptr;
  if (web_contents) {
    extension =
        extensions::ProcessManager::Get(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()))
            ->GetExtensionForWebContents(web_contents);
  }
  return AllowInspection(profile, extension);
}

// static
bool ChromeDevToolsManagerDelegate::AllowInspection(
    Profile* profile,
    const extensions::Extension* extension) {
#if defined(OS_CHROMEOS)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(chromeos::switches::kForceDevToolsAvailable))
    return true;
#endif

  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetDevToolsAvailability(
          profile->GetPrefs());
#if defined(OS_CHROMEOS)
  // Do not create DevTools if it's disabled for primary profile.
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
  if (primary_profile &&
      policy::DeveloperToolsPolicyHandler::IsDevToolsAvailabilitySetByPolicy(
          primary_profile->GetPrefs())) {
    availability =
        policy::DeveloperToolsPolicyHandler::GetMostRestrictiveAvailability(
            availability,
            policy::DeveloperToolsPolicyHandler::GetDevToolsAvailability(
                primary_profile->GetPrefs()));
  }
#endif

  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
      return !extension ||
             !extensions::Manifest::IsPolicyLocation(extension->location());
    default:
      NOTREACHED() << "Unknown developer tools policy";
      return true;
  }
}

void ChromeDevToolsManagerDelegate::ClientAttached(
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client) {
  DCHECK(sessions_.find(client) == sessions_.end());
  sessions_[client] =
      std::make_unique<ChromeDevToolsSession>(agent_host, client);
}

void ChromeDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client) {
  sessions_.erase(client);
}

scoped_refptr<DevToolsAgentHost>
ChromeDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  NavigateParams params(ProfileManager::GetLastUsedProfile(), url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return nullptr;
  return DevToolsAgentHost::GetOrCreateFor(
      params.navigated_or_inserted_contents);
}

std::string ChromeDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DEVTOOLS_DISCOVERY_PAGE_HTML);
}

std::vector<content::BrowserContext*>
ChromeDevToolsManagerDelegate::GetBrowserContexts() {
  return DevToolsBrowserContextManager::GetInstance().GetBrowserContexts();
}

content::BrowserContext*
ChromeDevToolsManagerDelegate::GetDefaultBrowserContext() {
  return DevToolsBrowserContextManager::GetInstance()
      .GetDefaultBrowserContext();
}

content::BrowserContext* ChromeDevToolsManagerDelegate::CreateBrowserContext() {
  return DevToolsBrowserContextManager::GetInstance().CreateBrowserContext();
}

void ChromeDevToolsManagerDelegate::DisposeBrowserContext(
    content::BrowserContext* context,
    DisposeCallback callback) {
  DevToolsBrowserContextManager::GetInstance().DisposeBrowserContext(
      context, std::move(callback));
}

bool ChromeDevToolsManagerDelegate::HasBundledFrontendResources() {
  return true;
}

void ChromeDevToolsManagerDelegate::DevicesAvailable(
    const DevToolsDeviceDiscovery::CompleteDevices& devices) {
  DevToolsAgentHost::List remote_targets;
  for (const auto& complete : devices) {
    for (const auto& browser : complete.second->browsers()) {
      for (const auto& page : browser->pages())
        remote_targets.push_back(page->CreateTarget());
    }
  }
  remote_agent_hosts_.swap(remote_targets);
}

void ChromeDevToolsManagerDelegate::UpdateDeviceDiscovery() {
  RemoteLocations remote_locations;
  for (const auto& it : sessions_) {
    TargetHandler* target_handler = it.second->target_handler();
    if (!target_handler)
      continue;
    RemoteLocations& locations = target_handler->remote_locations();
    remote_locations.insert(locations.begin(), locations.end());
  }

  bool equals = remote_locations.size() == remote_locations_.size();
  if (equals) {
    auto it1 = remote_locations.begin();
    auto it2 = remote_locations_.begin();
    while (it1 != remote_locations.end()) {
      DCHECK(it2 != remote_locations_.end());
      if (!(*it1).Equals(*it2))
        equals = false;
      ++it1;
      ++it2;
    }
    DCHECK(it2 == remote_locations_.end());
  }

  if (equals)
    return;

  if (remote_locations.empty()) {
    device_discovery_.reset();
    remote_agent_hosts_.clear();
  } else {
    if (!device_manager_)
      device_manager_ = AndroidDeviceManager::Create();

    AndroidDeviceManager::DeviceProviders providers;
    providers.push_back(new TCPDeviceProvider(remote_locations));
    device_manager_->SetDeviceProviders(providers);

    device_discovery_.reset(new DevToolsDeviceDiscovery(device_manager_.get(),
        base::Bind(&ChromeDevToolsManagerDelegate::DevicesAvailable,
                   base::Unretained(this))));
  }
  remote_locations_.swap(remote_locations);
}

void ChromeDevToolsManagerDelegate::ResetAndroidDeviceManagerForTesting() {
  device_manager_.reset();

  // We also need |device_discovery_| to go away because there may be a pending
  // task using a raw pointer to the DeviceManager we just deleted.
  device_discovery_.reset();
}
