// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/devtools/chrome_devtools_session.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "chrome/browser/devtools/device/tcp_device_provider.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol/target_handler.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/browser_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

using content::DevToolsAgentHost;

const char ChromeDevToolsManagerDelegate::kTypeApp[] = "app";
const char ChromeDevToolsManagerDelegate::kTypeBackgroundPage[] =
    "background_page";

namespace {

bool GetExtensionInfo(content::WebContents* wc,
                      std::string* name,
                      std::string* type) {
  auto* process_manager =
      extensions::ProcessManager::Get(wc->GetBrowserContext());
  if (!process_manager) {
    return false;
  }
  const extensions::Extension* extension =
      process_manager->GetExtensionForWebContents(wc);
  if (!extension)
    return false;
  extensions::ExtensionHost* extension_host =
      process_manager->GetBackgroundHostForExtension(extension->id());
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

#if !BUILDFLAG(IS_CHROMEOS)
  // Only create and hold keep alive for automation test for non ChromeOS.
  // ChromeOS automation test (aka tast) manages chrome instance via session
  // manager daemon. The extra keep alive is not needed and makes ChromeOS
  // not able to shutdown chrome properly. See https://crbug.com/1174627.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoStartupWindow) &&
      (command_line->HasSwitch(switches::kRemoteDebuggingPipe) ||
       command_line->HasSwitch(switches::kRemoteDebuggingPort))) {
    // If running without a startup window with remote debugging,
    // we are controlled entirely by the automation process.
    // Keep the application running until explicit close through DevTools
    // protocol.
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::REMOTE_DEBUGGING, KeepAliveRestartOption::DISABLED);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
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
    content::DevToolsAgentHostClientChannel* channel,
    base::span<const uint8_t> message,
    NotHandledCallback callback) {
  auto it = sessions_.find(channel);
  if (it == sessions_.end()) {
    std::move(callback).Run(message);
    // This should not happen, but happens. NOTREACHED tries to get
    // a repro in some test.
    NOTREACHED();
    return;
  }
  it->second->HandleCommand(message, std::move(callback));
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
  auto* process_manager = extensions::ProcessManager::Get(profile);
  return AllowInspection(
      profile, process_manager
                   ? process_manager->GetExtensionForRenderFrameHost(rfh)
                   : nullptr);
}

// static
bool ChromeDevToolsManagerDelegate::AllowInspection(
    Profile* profile,
    content::WebContents* web_contents) {
  const extensions::Extension* extension = nullptr;
  if (web_contents) {
    if (auto* process_manager = extensions::ProcessManager::Get(
            web_contents->GetBrowserContext())) {
      extension = process_manager->GetExtensionForWebContents(web_contents);
    }
  }
  return AllowInspection(profile, extension);
}

// static
bool ChromeDevToolsManagerDelegate::AllowInspection(
    Profile* profile,
    const extensions::Extension* extension) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kForceDevToolsAvailable))
    return true;
#endif

  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetDevToolsAvailability(
          profile->GetPrefs());
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    content::DevToolsAgentHostClientChannel* channel) {
  DCHECK(sessions_.find(channel) == sessions_.end());
  sessions_.emplace(channel, std::make_unique<ChromeDevToolsSession>(channel));
}

void ChromeDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHostClientChannel* channel) {
  sessions_.erase(channel);
}

scoped_refptr<DevToolsAgentHost> ChromeDevToolsManagerDelegate::CreateNewTarget(
    const GURL& url,
    bool for_tab) {
  NavigateParams params(ProfileManager::GetLastUsedProfile(), url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return nullptr;
  return for_tab ? DevToolsAgentHost::GetOrCreateForTab(
                       params.navigated_or_inserted_contents)
                 : DevToolsAgentHost::GetOrCreateFor(
                       params.navigated_or_inserted_contents);
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

    device_discovery_ = std::make_unique<DevToolsDeviceDiscovery>(
        device_manager_.get(),
        base::BindRepeating(&ChromeDevToolsManagerDelegate::DevicesAvailable,
                            base::Unretained(this)));
  }
  remote_locations_.swap(remote_locations);
}

void ChromeDevToolsManagerDelegate::ResetAndroidDeviceManagerForTesting() {
  device_manager_.reset();

  // We also need |device_discovery_| to go away because there may be a pending
  // task using a raw pointer to the DeviceManager we just deleted.
  device_discovery_.reset();
}

// static
void ChromeDevToolsManagerDelegate::CloseBrowserSoon() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        if (GetInstance()) {
          // Do not keep the application running anymore, we got an explicit
          // request to close.
          GetInstance()->keep_alive_.reset();
        }
        chrome::ExitIgnoreUnloadHandlers();
      }));
}
