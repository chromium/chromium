// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"
#include "chrome/browser/extensions/api/tabs/tabs_util.h"
#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"
#include "chrome/browser/extensions/chrome_extension_host_delegate.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/chrome_extensions_browser_api_provider.h"
#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"
#include "chrome/browser/extensions/chrome_kiosk_delegate.h"
#include "chrome/browser/extensions/chrome_process_manager_delegate.h"
#include "chrome/browser/extensions/chrome_url_request_util.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/updater/chrome_update_client_config.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/bluetooth/chrome_extension_bluetooth_chooser.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/update_client/update_client.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/core_extensions_browser_api_provider.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_interface_binders.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_channel.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/extension_cache_impl.h"
#include "components/user_manager/user_manager.h"
#else
#include "extensions/browser/updater/null_extension_cache.h"
#endif

namespace extensions {

namespace {

const char kCrxUrlPath[] = "/service/update2/crx";
const char kJsonUrlPath[] = "/service/update2/json";

// If true, the extensions client will behave as though there is always a
// new chrome update.
bool g_did_chrome_update_for_testing = false;

// The fake metrics logger instance to use for testing.
MediaRouterExtensionAccessLogger* g_media_router_access_logger_for_testing =
    nullptr;

bool ExtensionsDisabled(const base::CommandLine& command_line) {
  return command_line.HasSwitch(::switches::kDisableExtensions) ||
         command_line.HasSwitch(::switches::kDisableExtensionsExcept);
}

}  // namespace

ChromeExtensionsBrowserClient::ChromeExtensionsBrowserClient() {
  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
  AddAPIProvider(std::make_unique<ChromeExtensionsBrowserAPIProvider>());

  process_manager_delegate_ = std::make_unique<ChromeProcessManagerDelegate>();
  api_client_ = std::make_unique<ChromeExtensionsAPIClient>();
  SetCurrentChannel(chrome::GetChannel());
  resource_manager_ =
      std::make_unique<ChromeComponentExtensionResourceManager>();
}

ChromeExtensionsBrowserClient::~ChromeExtensionsBrowserClient() {}

bool ChromeExtensionsBrowserClient::IsShuttingDown() {
  return g_browser_process->IsShuttingDown();
}

bool ChromeExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return ExtensionsDisabled(command_line) ||
         profile->GetPrefs()->GetBoolean(prefs::kDisableExtensions);
}

bool ChromeExtensionsBrowserClient::IsValidContext(
    content::BrowserContext* context) {
  DCHECK(context);
  if (!g_browser_process) {
    LOG(ERROR) << "Unexpected null g_browser_process";
    NOTREACHED();
    return false;
  }
  Profile* profile = static_cast<Profile*>(context);
  return g_browser_process->profile_manager() &&
         g_browser_process->profile_manager()->IsValidProfile(profile);
}

bool ChromeExtensionsBrowserClient::IsSameContext(
    content::BrowserContext* first,
    content::BrowserContext* second) {
  Profile* first_profile = Profile::FromBrowserContext(first);
  Profile* second_profile = Profile::FromBrowserContext(second);
  return first_profile->IsSameOrParent(second_profile);
}

bool ChromeExtensionsBrowserClient::HasOffTheRecordContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->HasPrimaryOTRProfile();
}

content::BrowserContext* ChromeExtensionsBrowserClient::GetOffTheRecordContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrimaryOTRProfile();
}

content::BrowserContext* ChromeExtensionsBrowserClient::GetOriginalContext(
    content::BrowserContext* context) {
  DCHECK(context);
  return static_cast<Profile*>(context)->GetOriginalProfile();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string ChromeExtensionsBrowserClient::GetUserIdHashFromContext(
    content::BrowserContext* context) {
  return chromeos::ProfileHelper::GetUserIdHashFromProfile(
      static_cast<Profile*>(context));
}
#endif

bool ChromeExtensionsBrowserClient::IsGuestSession(
    content::BrowserContext* context) const {
  return static_cast<Profile*>(context)->IsGuestSession();
}

bool ChromeExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return IsGuestSession(context) ||
         util::IsIncognitoEnabled(extension_id, context);
}

bool ChromeExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return IsGuestSession(context) || util::CanCrossIncognito(extension, context);
}

base::FilePath ChromeExtensionsBrowserClient::GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) const {
  return chrome_url_request_util::GetBundleResourcePath(
      request, extension_resources_path, resource_id);
}

void ChromeExtensionsBrowserClient::LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    const std::string& content_security_policy,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    bool send_cors_header) {
  chrome_url_request_util::LoadResourceFromResourceBundle(
      request, std::move(loader), resource_relative_path, resource_id,
      content_security_policy, std::move(client), send_cors_header);
}

bool ChromeExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map) {
  bool allowed = false;
  if (chrome_url_request_util::AllowCrossRendererResourceLoad(
          request, destination, page_transition, child_id, is_incognito,
          extension, extensions, process_map, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* ChromeExtensionsBrowserClient::GetPrefServiceForContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrefs();
}

void ChromeExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<EarlyExtensionPrefsObserver*>* observers) const {
  observers->push_back(ContentSettingsService::Get(context));
}

ProcessManagerDelegate*
ChromeExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return process_manager_delegate_.get();
}

std::unique_ptr<ExtensionHostDelegate>
ChromeExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return std::unique_ptr<ExtensionHostDelegate>(
      new ChromeExtensionHostDelegate);
}

bool ChromeExtensionsBrowserClient::DidVersionUpdate(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);

  // Unit tests may not provide prefs; assume everything is up to date.
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile);
  if (!extension_prefs)
    return false;

  if (g_did_chrome_update_for_testing)
    return true;

  // If we're inside a browser test, then assume prefs are all up to date.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(::switches::kTestType))
    return false;

  PrefService* pref_service = extension_prefs->pref_service();
  base::Version last_version;
  if (pref_service->HasPrefPath(pref_names::kLastChromeVersion)) {
    std::string last_version_str =
        pref_service->GetString(pref_names::kLastChromeVersion);
    last_version = base::Version(last_version_str);
  }

  std::string current_version_str = version_info::GetVersionNumber();
  const base::Version& current_version = version_info::GetVersion();
  pref_service->SetString(pref_names::kLastChromeVersion, current_version_str);

  // If there was no version string in prefs, assume we're out of date.
  if (!last_version.IsValid())
    return true;
  // If the current version string is invalid, assume we didn't update.
  if (!current_version.IsValid())
    return false;

  return last_version < current_version;
}

void ChromeExtensionsBrowserClient::PermitExternalProtocolHandler() {
  ExternalProtocolHandler::PermitLaunchUrl();
}

bool ChromeExtensionsBrowserClient::IsInDemoMode() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const chromeos::DemoSession* const demo_session =
      chromeos::DemoSession::Get();
  return demo_session && demo_session->started();
#else
  return false;
#endif
}

bool ChromeExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return app_id == chromeos::DemoSession::GetScreensaverAppId() &&
         IsInDemoMode();
#endif
  return false;
}

bool ChromeExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return chrome::IsRunningInForcedAppMode();
}

bool ChromeExtensionsBrowserClient::IsAppModeForcedForApp(
    const ExtensionId& extension_id) {
  return chrome::IsRunningInForcedAppModeForApp(extension_id);
}

bool ChromeExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return user_manager::UserManager::Get()->IsLoggedInAsPublicAccount();
#else
  return false;
#endif
}

ExtensionSystemProvider*
ChromeExtensionsBrowserClient::GetExtensionSystemFactory() {
  return ExtensionSystemFactory::GetInstance();
}

void ChromeExtensionsBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  PopulateExtensionFrameBinders(binder_map, render_frame_host, extension);
  PopulateChromeFrameBindersForExtension(binder_map, render_frame_host,
                                         extension);
}

std::unique_ptr<RuntimeAPIDelegate>
ChromeExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return std::unique_ptr<RuntimeAPIDelegate>(
      new ChromeRuntimeAPIDelegate(context));
}

const ComponentExtensionResourceManager*
ChromeExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return resource_manager_.get();
}

void ChromeExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args,
    bool dispatch_to_off_the_record_profiles) {
  g_browser_process->extension_event_router_forwarder()
      ->BroadcastEventToRenderers(histogram_value, event_name, std::move(args),
                                  GURL(), dispatch_to_off_the_record_profiles);
}

ExtensionCache* ChromeExtensionsBrowserClient::GetExtensionCache() {
  if (!extension_cache_.get()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    extension_cache_.reset(new ExtensionCacheImpl(
        std::make_unique<ChromeOSExtensionCacheDelegate>()));
#else
    extension_cache_ = std::make_unique<NullExtensionCache>();
#endif
  }
  return extension_cache_.get();
}

bool ChromeExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kDisableBackgroundNetworking);
}

bool ChromeExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  const base::Version& browser_version = version_info::GetVersion();
  base::Version browser_min_version(min_version);
  return !browser_version.IsValid() || !browser_min_version.IsValid() ||
         browser_min_version.CompareTo(browser_version) <= 0;
}

ExtensionWebContentsObserver*
ChromeExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return ChromeExtensionWebContentsObserver::FromWebContents(web_contents);
}

void ChromeExtensionsBrowserClient::ReportError(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionError> error) {
  ErrorConsole::Get(context)->ReportError(std::move(error));
}

void ChromeExtensionsBrowserClient::CleanUpWebView(
    content::BrowserContext* browser_context,
    int embedder_process_id,
    int view_instance_id) {
  // Clean up context menus for the WebView.
  auto* menu_manager =
      MenuManager::Get(Profile::FromBrowserContext(browser_context));
  menu_manager->RemoveAllContextItems(
      MenuItem::ExtensionKey("", embedder_process_id, view_instance_id));
}

void ChromeExtensionsBrowserClient::AttachExtensionTaskManagerTag(
    content::WebContents* web_contents,
    mojom::ViewType view_type) {
  switch (view_type) {
    case mojom::ViewType::kAppWindow:
    case mojom::ViewType::kComponent:
    case mojom::ViewType::kExtensionBackgroundPage:
    case mojom::ViewType::kExtensionDialog:
    case mojom::ViewType::kExtensionPopup:
      // These are the only types that are tracked by the ExtensionTag.
      task_manager::WebContentsTags::CreateForExtension(web_contents,
                                                        view_type);
      return;

    case mojom::ViewType::kBackgroundContents:
    case mojom::ViewType::kExtensionGuest:
    case mojom::ViewType::kTabContents:
      // Those types are tracked by other tags:
      // BACKGROUND_CONTENTS --> task_manager::BackgroundContentsTag.
      // GUEST --> extensions::ChromeGuestViewManagerDelegate.
      // PANEL --> task_manager::PanelTag.
      // TAB_CONTENTS --> task_manager::TabContentsTag.
      // These tags are created and attached to the web_contents in other
      // locations, and they must be ignored here.
      return;

    case mojom::ViewType::kInvalid:
      NOTREACHED();
      return;
  }
}

scoped_refptr<update_client::UpdateClient>
ChromeExtensionsBrowserClient::CreateUpdateClient(
    content::BrowserContext* context) {
  base::Optional<GURL> override_url;
  GURL update_url = extension_urls::GetWebstoreUpdateUrl();
  if (update_url != extension_urls::GetDefaultWebstoreUpdateUrl()) {
    if (update_url.path() == kCrxUrlPath) {
      override_url = update_url.GetWithEmptyPath().Resolve(kJsonUrlPath);
    } else {
      override_url = update_url;
    }
  }
  return update_client::UpdateClientFactory(
      ChromeUpdateClientConfig::Create(context, override_url));
}

std::unique_ptr<content::BluetoothChooser>
ChromeExtensionsBrowserClient::CreateBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
  return std::make_unique<ChromeExtensionBluetoothChooser>(frame,
                                                           event_handler);
}

bool ChromeExtensionsBrowserClient::IsActivityLoggingEnabled(
    content::BrowserContext* context) {
  ActivityLog* activity_log = ActivityLog::GetInstance(context);
  return activity_log && activity_log->is_active();
}

void ChromeExtensionsBrowserClient::GetTabAndWindowIdForWebContents(
    content::WebContents* web_contents,
    int* tab_id,
    int* window_id) {
  sessions::SessionTabHelper* session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(web_contents);
  if (session_tab_helper) {
    *tab_id = session_tab_helper->session_id().id();
    *window_id = session_tab_helper->window_id().id();
  } else {
    *tab_id = -1;
    *window_id = -1;
  }
}

KioskDelegate* ChromeExtensionsBrowserClient::GetKioskDelegate() {
  if (!kiosk_delegate_)
    kiosk_delegate_ = std::make_unique<ChromeKioskDelegate>();
  return kiosk_delegate_.get();
}

bool ChromeExtensionsBrowserClient::IsLockScreenContext(
    content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return chromeos::ProfileHelper::IsLockScreenAppProfile(
      Profile::FromBrowserContext(context));
#else
  return false;
#endif
}

std::string ChromeExtensionsBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeExtensionsBrowserClient::IsExtensionEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return ExtensionSystem::Get(context)->extension_service()->IsExtensionEnabled(
      extension_id);
}

bool ChromeExtensionsBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return ChromeWebUIControllerFactory::IsWebUIAllowedToMakeNetworkRequests(
      origin);
}

network::mojom::NetworkContext*
ChromeExtensionsBrowserClient::GetSystemNetworkContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_browser_process->system_network_context_manager()->GetContext();
}

UserScriptListener* ChromeExtensionsBrowserClient::GetUserScriptListener() {
  return &user_script_listener_;
}

std::string ChromeExtensionsBrowserClient::GetUserAgent() const {
  return embedder_support::GetUserAgent();
}

bool ChromeExtensionsBrowserClient::ShouldSchemeBypassNavigationChecks(
    const std::string& scheme) const {
  if (scheme == chrome::kChromeSearchScheme)
    return true;

  return ExtensionsBrowserClient::ShouldSchemeBypassNavigationChecks(scheme);
}

base::FilePath ChromeExtensionsBrowserClient::GetSaveFilePath(
    content::BrowserContext* context) {
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(context);
  return download_prefs->SaveFilePath();
}

void ChromeExtensionsBrowserClient::SetLastSaveFilePath(
    content::BrowserContext* context,
    const base::FilePath& path) {
  DownloadPrefs* download_prefs = DownloadPrefs::FromBrowserContext(context);
  download_prefs->SetSaveFilePath(path);
}

const MediaRouterExtensionAccessLogger*
ChromeExtensionsBrowserClient::GetMediaRouterAccessLogger() const {
  return g_media_router_access_logger_for_testing
             ? g_media_router_access_logger_for_testing
             : &media_router_access_logger_;
}

bool ChromeExtensionsBrowserClient::HasIsolatedStorage(
    const std::string& extension_id,
    content::BrowserContext* context) {
  return extensions::util::HasIsolatedStorage(extension_id, context);
}

bool ChromeExtensionsBrowserClient::IsScreenshotRestricted(
    content::WebContents* web_contents) const {
  return tabs_util::IsScreenshotRestricted(web_contents);
}

bool ChromeExtensionsBrowserClient::IsValidTabId(
    content::BrowserContext* context,
    int tab_id) const {
  return ExtensionTabUtil::GetTabById(
      tab_id, context, true /* include_incognito */, nullptr /* contents */);
}

// static
void ChromeExtensionsBrowserClient::SetMediaRouterAccessLoggerForTesting(
    MediaRouterExtensionAccessLogger* media_router_access_logger) {
  g_media_router_access_logger_for_testing = media_router_access_logger;
}

// static
void ChromeExtensionsBrowserClient::set_did_chrome_update_for_testing(
    bool did_update) {
  g_did_chrome_update_for_testing = did_update;
}

}  // namespace extensions
