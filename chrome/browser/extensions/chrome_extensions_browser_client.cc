// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/api/favicon/favicon_util.h"
#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"
#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
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
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/update_client/update_client.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/content_settings/content_settings_service.h"
#include "extensions/browser/api/core_extensions_browser_api_provider.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_interface_binders.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/scoped_extension_updater_keep_alive.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/permissions/permission_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

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

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace extensions {

namespace {

const char kCrxUrlPath[] = "/service/update2/crx";
const char kJsonUrlPath[] = "/service/update2/json";

// If true, the extensions client will behave as though there is always a
// new chrome update.
bool g_did_chrome_update_for_testing = false;

bool ExtensionsDisabled(const base::CommandLine& command_line) {
  return command_line.HasSwitch(::switches::kDisableExtensions) ||
         command_line.HasSwitch(::switches::kDisableExtensionsExcept);
}

class UpdaterKeepAlive : public ScopedExtensionUpdaterKeepAlive {
 public:
  UpdaterKeepAlive(Profile* profile, ProfileKeepAliveOrigin origin)
      : profile_keep_alive_(profile, origin) {}
  ~UpdaterKeepAlive() override = default;

 private:
  ScopedProfileKeepAlive profile_keep_alive_;
};

bool ShouldLogExtensionAction(content::BrowserContext* browser_context,
                              const std::string& extension_id) {
  // We only send these IPCs if activity logging is enabled, but due to race
  // conditions (e.g. logging gets disabled but the renderer sends the message
  // before it gets updated), we still need this check here.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return browser_context &&
         g_browser_process->profile_manager()->IsValidProfile(
             browser_context) &&
         ActivityLog::GetInstance(browser_context) &&
         ActivityLog::GetInstance(browser_context)->ShouldLog(extension_id);
}

// Logs an action to the extension activity log for the specified profile.
void AddActionToExtensionActivityLog(content::BrowserContext* browser_context,
                                     scoped_refptr<Action> action) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If the action included a URL, check whether it is for an incognito profile.
  // The check is performed here so that it can safely be done from the UI
  // thread.
  if (action->page_url().is_valid() || !action->page_title().empty()) {
    action->set_page_incognito(browser_context->IsOffTheRecord());
  }
  ActivityLog::GetInstance(browser_context)->LogAction(action);
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

void ChromeExtensionsBrowserClient::StartTearDown() {
  user_script_listener_.StartTearDown();
}

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
  return static_cast<Profile*>(context)->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
}

content::BrowserContext* ChromeExtensionsBrowserClient::GetOriginalContext(
    content::BrowserContext* context) {
  DCHECK(context);
  return static_cast<Profile*>(context)->GetOriginalProfile();
}

content::BrowserContext*
ChromeExtensionsBrowserClient::GetRedirectedContextInIncognito(
    content::BrowserContext* context,
    bool force_guest_profile,
    bool force_system_profile) {
  const ProfileSelections selections =
      ProfileSelections::BuildRedirectedInIncognito(force_guest_profile,
                                                    force_system_profile);
  return selections.ApplyProfileSelection(Profile::FromBrowserContext(context));
}

content::BrowserContext*
ChromeExtensionsBrowserClient::GetContextForRegularAndIncognito(
    content::BrowserContext* context,
    bool force_guest_profile,
    bool force_system_profile) {
  const ProfileSelections selections =
      ProfileSelections::BuildForRegularAndIncognito(force_guest_profile,
                                                     force_system_profile);
  return selections.ApplyProfileSelection(Profile::FromBrowserContext(context));
}

content::BrowserContext* ChromeExtensionsBrowserClient::GetRegularProfile(
    content::BrowserContext* context,
    bool force_guest_profile,
    bool force_system_profile) {
  const ProfileSelections selections = ProfileSelections::BuildDefault(
      force_guest_profile, force_system_profile);
  return selections.ApplyProfileSelection(Profile::FromBrowserContext(context));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string ChromeExtensionsBrowserClient::GetUserIdHashFromContext(
    content::BrowserContext* context) {
  return ash::ProfileHelper::GetUserIdHashFromProfile(
      static_cast<Profile*>(context));
}
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ChromeExtensionsBrowserClient::IsFromMainProfile(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->IsMainProfile();
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
    scoped_refptr<net::HttpResponseHeaders> headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  chrome_url_request_util::LoadResourceFromResourceBundle(
      request, std::move(loader), resource_relative_path, resource_id,
      std::move(headers), std::move(client));
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
  return ash::DemoSession::IsDeviceInDemoMode();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::BrowserParamsProxy::Get()->DeviceMode() ==
         crosapi::mojom::DeviceMode::kDemo;
#else
  return false;
#endif
}

bool ChromeExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return app_id == ash::DemoSession::GetScreensaverAppId() && IsInDemoMode();
#else
  return false;
#endif
}

bool ChromeExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return chrome::IsRunningInForcedAppMode();
}

bool ChromeExtensionsBrowserClient::IsAppModeForcedForApp(
    const ExtensionId& extension_id) {
  return chrome::IsRunningInForcedAppModeForApp(extension_id);
}

bool ChromeExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return profiles::IsPublicSession();
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
    base::Value::List args,
    bool dispatch_to_off_the_record_profiles) {
  g_browser_process->extension_event_router_forwarder()
      ->BroadcastEventToRenderers(histogram_value, event_name, std::move(args),
                                  GURL(), dispatch_to_off_the_record_profiles);
}

ExtensionCache* ChromeExtensionsBrowserClient::GetExtensionCache() {
  if (!extension_cache_.get()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    extension_cache_ = std::make_unique<ExtensionCacheImpl>(
        std::make_unique<ChromeOSExtensionCacheDelegate>());
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
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    return;
  }

  // Clean up context menus for the WebView.
  auto* menu_manager = MenuManager::Get(profile);
  DCHECK(menu_manager);
  menu_manager->RemoveAllContextItems(
      MenuItem::ExtensionKey("", embedder_process_id, view_instance_id));
}

void ChromeExtensionsBrowserClient::ClearBackForwardCache() {
  ExtensionTabUtil::ClearBackForwardCache();
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
    case mojom::ViewType::kOffscreenDocument:
    case mojom::ViewType::kExtensionSidePanel:
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
  absl::optional<GURL> override_url;
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

std::unique_ptr<ScopedExtensionUpdaterKeepAlive>
ChromeExtensionsBrowserClient::CreateUpdaterKeepAlive(
    content::BrowserContext* context) {
  return std::make_unique<UpdaterKeepAlive>(
      Profile::FromBrowserContext(context),
      ProfileKeepAliveOrigin::kExtensionUpdater);
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
  return ash::ProfileHelper::IsLockScreenAppProfile(
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

void ChromeExtensionsBrowserClient::SignalContentScriptsLoaded(
    content::BrowserContext* context) {
  user_script_listener_.OnScriptsLoaded(context);
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

bool ChromeExtensionsBrowserClient::HasIsolatedStorage(
    const std::string& extension_id,
    content::BrowserContext* context) {
  return extensions::util::HasIsolatedStorage(extension_id, context);
}

bool ChromeExtensionsBrowserClient::IsScreenshotRestricted(
    content::WebContents* web_contents) const {
#if !BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return policy::DlpContentManager::Get()->IsScreenshotApiRestricted(
      web_contents);
#endif
}

bool ChromeExtensionsBrowserClient::IsValidTabId(
    content::BrowserContext* context,
    int tab_id) const {
  return ExtensionTabUtil::GetTabById(
      tab_id, context, true /* include_incognito */, nullptr /* contents */);
}

void ChromeExtensionsBrowserClient::NotifyExtensionApiTabExecuteScript(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& code) const {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::kExtensionTelemetryTabsExecuteScriptSignal)) {
    return;
  }

  auto signal = std::make_unique<safe_browsing::TabsExecuteScriptSignal>(
      extension_id, code);
  telemetry_service->AddSignal(std::move(signal));
}

bool ChromeExtensionsBrowserClient::IsExtensionTelemetryServiceEnabled(
    content::BrowserContext* context) const {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return telemetry_service && telemetry_service->enabled();
}

void ChromeExtensionsBrowserClient::NotifyExtensionApiDeclarativeNetRequest(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::vector<api::declarative_net_request::Rule>& rules) const {
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::kExtensionTelemetryDeclarativeNetRequestSignal)) {
    return;
  }

  // The telemetry service will consume and release the signal object inside the
  // `AddSignal()` call.
  auto signal = std::make_unique<safe_browsing::DeclarativeNetRequestSignal>(
      extension_id, rules);
  telemetry_service->AddSignal(std::move(signal));
}

void ChromeExtensionsBrowserClient::NotifyExtensionRemoteHostContacted(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const GURL& url) const {
  safe_browsing::RemoteHostInfo::ProtocolType protocol =
      safe_browsing::RemoteHostInfo::UNSPECIFIED;
  if (base::FeatureList::IsEnabled(
          safe_browsing::kExtensionTelemetryReportContactedHosts) &&
      url.SchemeIsHTTPOrHTTPS()) {
    protocol = safe_browsing::RemoteHostInfo::HTTP_HTTPS;
  } else if (base::FeatureList::IsEnabled(
                 safe_browsing::
                     kExtensionTelemetryReportHostsContactedViaWebSocket) &&
             url.SchemeIsWSOrWSS()) {
    protocol = safe_browsing::RemoteHostInfo::WEBSOCKET;
  } else {
    return;
  }
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }
  auto remote_host_signal =
      std::make_unique<safe_browsing::RemoteHostContactedSignal>(extension_id,
                                                                 url, protocol);
  telemetry_service->AddSignal(std::move(remote_host_signal));
}

// static
void ChromeExtensionsBrowserClient::set_did_chrome_update_for_testing(
    bool did_update) {
  g_did_chrome_update_for_testing = did_update;
}

bool ChromeExtensionsBrowserClient::IsUsbDeviceAllowedByPolicy(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    int vendor_id,
    int product_id) const {
  url::Origin origin =
      extensions::Extension::CreateOriginFromExtensionId(extension_id);

  UsbChooserContext* usb_chooser_context =
      UsbChooserContextFactory::GetForProfile(static_cast<Profile*>(context));
  // This will never be null as even incognito mode has its own instance.
  DCHECK(usb_chooser_context);

  // Check against WebUsbAllowDevicesForUrls.
  return usb_chooser_context->usb_policy_allowed_devices().IsDeviceAllowed(
      origin, {vendor_id, product_id});
}

void ChromeExtensionsBrowserClient::GetFavicon(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const GURL& url,
    base::CancelableTaskTracker* tracker,
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory> bitmap_data)>
        callback) const {
  favicon_util::GetFaviconForExtensionRequest(browser_context, extension, url,
                                              tracker, std::move(callback));
}

std::vector<content::BrowserContext*>
ChromeExtensionsBrowserClient::GetRelatedContextsForExtension(
    content::BrowserContext* browser_context,
    const Extension& extension) const {
  return util::GetAllRelatedProfiles(
      Profile::FromBrowserContext(browser_context), extension);
}

void ChromeExtensionsBrowserClient::AddAdditionalAllowedHosts(
    const PermissionSet& desired_permissions,
    PermissionSet* granted_permissions) const {
  auto get_new_host_patterns = [](const URLPatternSet& desired_patterns,
                                  const URLPatternSet& granted_patterns) {
    URLPatternSet new_patterns = granted_patterns.Clone();
    for (const URLPattern& pattern : desired_patterns) {
      // The chrome://favicon permission is special. It is requested by
      // extensions to access stored favicons, but is not a traditional
      // host permission. Since it cannot be reasonably runtime-granted
      // while the user is on the site (i.e., the user never visits
      // chrome://favicon/), we auto-grant it and treat it like an API
      // permission.
      bool is_chrome_favicon = pattern.scheme() == content::kChromeUIScheme &&
                               pattern.host() == chrome::kChromeUIFaviconHost;
      if (is_chrome_favicon)
        new_patterns.AddPattern(pattern);
    }
    return new_patterns;
  };

  URLPatternSet new_explicit_hosts =
      get_new_host_patterns(desired_permissions.explicit_hosts(),
                            granted_permissions->explicit_hosts());
  URLPatternSet new_scriptable_hosts =
      get_new_host_patterns(desired_permissions.scriptable_hosts(),
                            granted_permissions->scriptable_hosts());
  granted_permissions->SetExplicitHosts(std::move(new_explicit_hosts));
  granted_permissions->SetScriptableHosts(std::move(new_scriptable_hosts));
}

void ChromeExtensionsBrowserClient::AddAPIActionToActivityLog(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const std::string& extra) {
  AddAPIActionOrEventToActivityLog(browser_context, extension_id,
                                   Action::ACTION_API_CALL, call_name,
                                   std::move(args), extra);
}

void ChromeExtensionsBrowserClient::AddEventToActivityLog(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const std::string& extra) {
  AddAPIActionOrEventToActivityLog(browser_context, extension_id,
                                   Action::ACTION_API_EVENT, call_name,
                                   std::move(args), extra);
}

void ChromeExtensionsBrowserClient::AddDOMActionToActivityLog(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const GURL& url,
    const std::u16string& url_title,
    int call_type) {
  if (!ShouldLogExtensionAction(browser_context, extension_id)) {
    return;
  }

  auto action = base::MakeRefCounted<Action>(
      extension_id, base::Time::Now(), Action::ACTION_DOM_ACCESS, call_name);
  action->set_args(std::move(args));
  action->set_page_url(url);
  action->set_page_title(base::UTF16ToUTF8(url_title));
  action->mutable_other().Set(activity_log_constants::kActionDomVerb,
                              call_type);
  AddActionToExtensionActivityLog(browser_context, action);
}

void ChromeExtensionsBrowserClient::AddAPIActionOrEventToActivityLog(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    Action::ActionType action_type,
    const std::string& call_name,
    base::Value::List args,
    const std::string& extra) {
  if (!ShouldLogExtensionAction(browser_context, extension_id)) {
    return;
  }

  auto action = base::MakeRefCounted<Action>(extension_id, base::Time::Now(),
                                             action_type, call_name);
  action->set_args(std::move(args));
  if (!extra.empty()) {
    action->mutable_other().Set(activity_log_constants::kActionExtra, extra);
  }
  AddActionToExtensionActivityLog(browser_context, action);
}

content::StoragePartitionConfig
ChromeExtensionsBrowserClient::GetWebViewStoragePartitionConfig(
    content::BrowserContext* browser_context,
    content::SiteInstance* owner_site_instance,
    const std::string& partition_name,
    bool in_memory) {
  const GURL& owner_site_url = owner_site_instance->GetSiteURL();
  if (owner_site_url.SchemeIs(chrome::kIsolatedAppScheme)) {
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
        web_app::IsolatedWebAppUrlInfo::Create(owner_site_url);
    DCHECK(url_info.has_value()) << url_info.error();
    return url_info->GetStoragePartitionConfigForControlledFrame(
        browser_context, partition_name, in_memory);
  }

  return ExtensionsBrowserClient::GetWebViewStoragePartitionConfig(
      browser_context, owner_site_instance, partition_name, in_memory);
}

void ChromeExtensionsBrowserClient::CreatePasswordReuseDetectionManager(
    content::WebContents* web_contents) const {
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);
}

}  // namespace extensions
