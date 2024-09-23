// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/strings/strcat.h"
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
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/browser_resources.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/switches.h"
#include "ui/views/controls/webview/webview.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

using content::DevToolsAgentHost;

const char ChromeDevToolsManagerDelegate::kTypeApp[] = "app";
const char ChromeDevToolsManagerDelegate::kTypeBackgroundPage[] =
    "background_page";
const char ChromeDevToolsManagerDelegate::kTypePage[] = "page";

namespace {

std::optional<std::string> GetIsolatedWebAppNameAndVersion(
    content::WebContents* web_contents) {
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id) {
    return std::nullopt;
  }
  const web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebContents(web_contents);
  if (!provider) {
    return std::nullopt;
  }
  // In this case we will not modify any data and reading stale data is
  // fine, since the app will already be installed and open in the case
  // it needs to be checked in DevTools.
  const web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  const web_app::WebApp* web_app = registrar.GetAppById(*app_id);

  if (web_app && registrar.IsIsolated(*app_id)) {
    // Version is a key part of IWA so should be displayed in inspect tool
    return base::StrCat({registrar.GetAppShortName(*app_id), " (",
                         web_app->isolation_data()->version().GetString(),
                         ")"});
  }

  return std::nullopt;
}

bool IsIsolatedWebApp(content::WebContents* web_contents) {
  return GetIsolatedWebAppNameAndVersion(web_contents).has_value();
}

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

  auto view_type = extensions::GetViewType(wc);
  if (view_type == extensions::mojom::ViewType::kExtensionPopup ||
      view_type == extensions::mojom::ViewType::kExtensionSidePanel ||
      view_type == extensions::mojom::ViewType::kOffscreenDocument) {
    // Note that we are intentionally not setting name here, so that we can
    // construct a name based on the URL or page title in
    // RenderFrameDevToolsAgentHost::GetTitle()
    *type = ChromeDevToolsManagerDelegate::kTypePage;
    return true;
  }

  // Set type to other for extensions if not matched previously.
  *type = DevToolsAgentHost::kTypeOther;
  return true;
}

policy::DeveloperToolsPolicyHandler::Availability GetDevToolsAvailability(
    Profile* profile) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS disable dev tools for captive portal signin windows to prevent
  // them from being used for general navigation.
  if (chromeos::features::IsCaptivePortalPopupWindowEnabled() &&
      availability != Availability::kDisallowed) {
    const PrefService::Preference* const captive_portal_pref =
        profile->GetPrefs()->FindPreference(
            chromeos::prefs::kCaptivePortalSignin);
    if (captive_portal_pref && captive_portal_pref->GetValue()->GetBool()) {
      availability = Availability::kDisallowed;
    }
  }
#endif
  return availability;
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
  if ((command_line->HasSwitch(switches::kNoStartupWindow) ||
       command_line->HasSwitch(switches::kHeadless)) &&
      (command_line->HasSwitch(switches::kRemoteDebuggingPipe) ||
       command_line->HasSwitch(switches::kRemoteDebuggingPort))) {
    // If running without a startup window with remote debugging,
    // we are controlled entirely by the automation process.
    // Keep the application running until explicit close through DevTools
    // protocol.
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::REMOTE_DEBUGGING, KeepAliveRestartOption::DISABLED);

    // Also keep the initial profile alive so that TargetHandler::CreateTarget()
    // can retrieve it without risking disk access even when all pages are
    // closed. Keep-a-living the very first loaded profile looks like a
    // reasonable option.
    if (Profile* profile = ProfileManager::GetLastUsedProfile()) {
      if (profile->IsOffTheRecord()) {
        profile = profile->GetOriginalProfile();
      }
      profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
          profile, ProfileKeepAliveOrigin::kRemoteDebugging);
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

ChromeDevToolsManagerDelegate::~ChromeDevToolsManagerDelegate() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

void ChromeDevToolsManagerDelegate::Inspect(
    content::DevToolsAgentHost* agent_host) {
  DevToolsWindow::OpenDevToolsWindow(agent_host, nullptr,
                                     DevToolsOpenedByAction::kInspectLink);
}

void ChromeDevToolsManagerDelegate::Activate(
    content::DevToolsAgentHost* agent_host) {
  auto* web_contents = agent_host->GetWebContents();
  if (!web_contents) {
    return;
  }

  // Brings the tab to foreground. We need to do this in case the devtools
  // window is undocked and this is being called from another tab that is in
  // the foreground.
  web_contents->GetDelegate()->ActivateContents(web_contents);

  // Brings a undocked devtools window to the foreground.
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(
          agent_host->GetWebContents());
  if (!devtools_window) {
    return;
  }
  devtools_window->ActivateWindow();
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
    NOTREACHED_IN_MIGRATION();
    return;
  }
  it->second->HandleCommand(message, std::move(callback));
}

std::string ChromeDevToolsManagerDelegate::GetTargetType(
    content::WebContents* web_contents) {
  if (IsIsolatedWebApp(web_contents)) {
    return ChromeDevToolsManagerDelegate::kTypeApp;
  }

  if (base::Contains(AllTabContentses(), web_contents))
    return DevToolsAgentHost::kTypePage;

  std::string extension_name;
  std::string extension_type;
  if (GetExtensionInfo(web_contents, &extension_name, &extension_type)) {
    return extension_type;
  }

  if (views::WebView::IsWebViewContents(web_contents)) {
    return DevToolsAgentHost::kTypePage;
  }

  return DevToolsAgentHost::kTypeOther;
}

std::string ChromeDevToolsManagerDelegate::GetTargetTitle(
    content::WebContents* web_contents) {
  if (auto iwa_name_version = GetIsolatedWebAppNameAndVersion(web_contents)) {
    return *iwa_name_version;
  }

  std::string extension_name;
  std::string extension_type;
  if (!GetExtensionInfo(web_contents, &extension_name, &extension_type)) {
    return std::string();
  }

  return extension_name;
}

bool ChromeDevToolsManagerDelegate::AllowInspectingRenderFrameHost(
    content::RenderFrameHost* rfh) {
  Profile* profile =
      Profile::FromBrowserContext(rfh->GetProcess()->GetBrowserContext());
  auto* process_manager = extensions::ProcessManager::Get(profile);
  auto* extension = process_manager
                        ? process_manager->GetExtensionForRenderFrameHost(rfh)
                        : nullptr;
  if (extension || !web_app::AreWebAppsEnabled(profile)) {
    return AllowInspection(profile, extension);
  }

  if (auto* web_app_provider =
          web_app::WebAppProvider::GetForWebApps(profile)) {
    std::optional<webapps::AppId> app_id =
        web_app_provider->registrar_unsafe().FindAppWithUrlInScope(
            rfh->GetMainFrame()->GetLastCommittedURL());
    if (app_id) {
      const auto* web_app =
          web_app_provider->registrar_unsafe().GetAppById(app_id.value());
      return AllowInspection(profile, web_app);
    }
  }
  // |extension| is always nullptr here.
  return AllowInspection(profile, extension);
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
    if (extension || !web_app::AreWebAppsEnabled(profile)) {
      return AllowInspection(profile, extension);
    }

    const webapps::AppId* app_id =
        web_app::WebAppTabHelper::GetAppId(web_contents);
    auto* web_app_provider =
        web_app::WebAppProvider::GetForWebContents(web_contents);
    if (app_id && web_app_provider) {
      const web_app::WebApp* web_app =
          web_app_provider->registrar_unsafe().GetAppById(*app_id);
      return AllowInspection(profile, web_app);
    }
  }
  // |extension| is always nullptr here.
  return AllowInspection(profile, extension);
}

// static
bool ChromeDevToolsManagerDelegate::AllowInspection(
    Profile* profile,
    const extensions::Extension* extension) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability;
  if (extension) {
    availability =
        policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  } else {
    // Perform additional checks for browser windows (extension == null).
    availability = GetDevToolsAvailability(profile);
  }
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions:
      if (!extension) {
        return true;
      }
      if (extensions::Manifest::IsPolicyLocation(extension->location())) {
        return false;
      }
      // We also disallow inspecting component extensions, but only for managed
      // profiles.
      if (extensions::Manifest::IsComponentLocation(extension->location()) &&
          profile->GetProfilePolicyConnector()->IsManaged()) {
        return false;
      }
      return true;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown developer tools policy";
      return true;
  }
}

// static
bool ChromeDevToolsManagerDelegate::AllowInspection(
    Profile* profile,
    const web_app::WebApp* web_app) {
  using Availability = policy::DeveloperToolsPolicyHandler::Availability;
  Availability availability =
      policy::DeveloperToolsPolicyHandler::GetEffectiveAvailability(profile);
  switch (availability) {
    case Availability::kDisallowed:
      return false;
    case Availability::kAllowed:
      return true;
    case Availability::kDisallowedForForceInstalledExtensions: {
      if (!web_app) {
        return true;
      }
      // DevTools should be blocked for Kiosk apps and policy-installed IWAs.
      if (web_app->IsKioskInstalledApp() ||
          web_app->IsIwaPolicyInstalledApp()) {
        return false;
      }
      return true;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown developer tools policy";
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
    DevToolsManagerDelegate::TargetType target_type) {
  NavigateParams params(ProfileManager::GetLastUsedProfile(), url,
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return nullptr;
  return target_type == DevToolsManagerDelegate::kTab
             ? DevToolsAgentHost::GetOrCreateForTab(
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
      CHECK(it2 != remote_locations_.end(), base::NotFatalUntil::M130);
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
        // Do not keep the application running anymore, we got an explicit
        // request to close.
        AllowBrowserToClose();
        chrome::ExitIgnoreUnloadHandlers();
      }));
}

// static
void ChromeDevToolsManagerDelegate::AllowBrowserToClose() {
  if (auto* instance = GetInstance()) {
    instance->keep_alive_.reset();
  }
}
