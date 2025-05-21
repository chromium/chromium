// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_client.h"

#include <memory>
#include <optional>
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
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/activity_actions.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "chrome/browser/extensions/api/preference/cookie_controls_mode_transformer.h"
#include "chrome/browser/extensions/api/preference/network_prediction_transformer.h"
#include "chrome/browser/extensions/api/preference/privacy_sandbox_transformer.h"
#include "chrome/browser/extensions/api/preference/protected_content_enabled_transformer.h"
#include "chrome/browser/extensions/api/proxy/proxy_pref_transformer.h"
#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"
#include "chrome/browser/extensions/chrome_component_extension_resource_manager.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/chrome_extensions_browser_api_provider.h"
#include "chrome/browser/extensions/chrome_url_request_util.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/favicon/favicon_util.h"
#include "chrome/browser/extensions/pref_mapping.h"
#include "chrome/browser/extensions/updater/chrome_update_client_config.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/media/webrtc/media_device_salt_service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/buildflags.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/update_client/update_client.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/core_extensions_browser_api_provider.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/process_manager_delegate.h"
#include "extensions/browser/updater/scoped_extension_updater_keep_alive.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/mojom/view_type.mojom-shared.h"
#include "extensions/common/permissions/permission_set.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/extensions/updater/chromeos_extension_cache_delegate.h"
#include "chrome/browser/extensions/updater/extension_cache_impl.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/user_manager/user_manager.h"
#else
#include "extensions/browser/updater/null_extension_cache.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_reuse_detection_manager_client.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "components/safe_browsing/core/common/features.h"
#endif

namespace extensions {

namespace {

constexpr std::string_view kCrxUrlPath = "/service/update2/crx";
constexpr std::string_view kJsonUrlPath = "/service/update2/json";

// If true, the extensions client will behave as though there is always a
// new chrome update.
bool g_did_chrome_update_for_testing = false;

class UpdaterKeepAlive : public ScopedExtensionUpdaterKeepAlive {
 public:
  UpdaterKeepAlive(Profile* profile, ProfileKeepAliveOrigin origin)
      : profile_keep_alive_(profile, origin) {}
  UpdaterKeepAlive(const UpdaterKeepAlive&) = delete;
  UpdaterKeepAlive& operator=(const UpdaterKeepAlive&) = delete;
  ~UpdaterKeepAlive() override = default;

 private:
  ScopedProfileKeepAlive profile_keep_alive_;
};

bool ShouldLogExtensionAction(content::BrowserContext* browser_context,
                              const ExtensionId& extension_id) {
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

bool RegisterTransformers() {
  PrefMapping* pref_mapping = PrefMapping::GetInstance();
  pref_mapping->RegisterPrefTransformer(
      prefs::kCookieControlsMode,
      std::make_unique<CookieControlsModeTransformer>());
  pref_mapping->RegisterPrefTransformer(
      proxy_config::prefs::kProxy, std::make_unique<ProxyPrefTransformer>());
  pref_mapping->RegisterPrefTransformer(
      prefetch::prefs::kNetworkPredictionOptions,
      std::make_unique<NetworkPredictionTransformer>());
  pref_mapping->RegisterPrefTransformer(
      prefs::kProtectedContentDefault,
      std::make_unique<ProtectedContentEnabledTransformer>());
  pref_mapping->RegisterPrefTransformer(
      prefs::kPrivacySandboxM1TopicsEnabled,
      std::make_unique<PrivacySandboxTransformer>());
  pref_mapping->RegisterPrefTransformer(
      prefs::kPrivacySandboxM1FledgeEnabled,
      std::make_unique<PrivacySandboxTransformer>());
  pref_mapping->RegisterPrefTransformer(
      prefs::kPrivacySandboxM1AdMeasurementEnabled,
      std::make_unique<PrivacySandboxTransformer>());
  pref_mapping->RegisterPrefTransformer(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
      std::make_unique<PrivacySandboxTransformer>());

  return true;
}

}  // namespace

ChromeExtensionsBrowserClient::ChromeExtensionsBrowserClient()
    : resource_manager_(
          std::make_unique<ChromeComponentExtensionResourceManager>()),
      api_client_(std::make_unique<ChromeExtensionsAPIClient>()),
      event_router_forwarder_(base::MakeRefCounted<EventRouterForwarder>()) {
  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
  AddAPIProvider(std::make_unique<ChromeExtensionsBrowserAPIProvider>());
  // This ensures transformers are only registered once. This is required
  // because testing will create the singleton ChromeExtensionsBrowserClient
  // instance multiple times within the same process.
  static bool registered = RegisterTransformers();
  CHECK(registered);

  SetCurrentChannel(chrome::GetChannel());
}

ChromeExtensionsBrowserClient::~ChromeExtensionsBrowserClient() = default;

void ChromeExtensionsBrowserClient::StartTearDown() {
  user_script_listener_->StartTearDown();
}

bool ChromeExtensionsBrowserClient::IsShuttingDown() {
  return g_browser_process->IsShuttingDown();
}

bool ChromeExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    content::BrowserContext* context) {
  return util::AreExtensionsDisabled(command_line, context);
}

bool ChromeExtensionsBrowserClient::IsValidContext(void* context) {
  DCHECK(context);
  if (!g_browser_process) {
    LOG(ERROR) << "Unexpected null g_browser_process";
    NOTREACHED() << "Unexpected null g_browser_process";
  }
  return g_browser_process->profile_manager() &&
         g_browser_process->profile_manager()->IsValidProfile(context);
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
ChromeExtensionsBrowserClient::GetContextRedirectedToOriginal(
    content::BrowserContext* context) {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kRedirectedToOriginal)
      // TODO(crbug.com/41488885): Check if this service is needed for Ash
      // Internals.
      .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
      .Build()
      .ApplyProfileSelection(Profile::FromBrowserContext(context));
}

content::BrowserContext* ChromeExtensionsBrowserClient::GetContextOwnInstance(
    content::BrowserContext* context) {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kOwnInstance)
      // TODO(crbug.com/41488885): Check if this service is needed for Ash
      // Internals.
      .WithAshInternals(ProfileSelection::kOwnInstance)
      .Build()
      .ApplyProfileSelection(Profile::FromBrowserContext(context));
}

content::BrowserContext*
ChromeExtensionsBrowserClient::GetContextForOriginalOnly(
    content::BrowserContext* context) {
  return ProfileSelections::Builder()
      .WithGuest(ProfileSelection::kOriginalOnly)
      // TODO(crbug.com/41488885): Check if this service is needed for Ash
      // Internals.
      .WithAshInternals(ProfileSelection::kOriginalOnly)
      .Build()
      .ApplyProfileSelection(Profile::FromBrowserContext(context));
}

bool ChromeExtensionsBrowserClient::AreExtensionsDisabledForContext(
    content::BrowserContext* context) {
  return ChromeContentBrowserClientExtensionsPart::
      AreExtensionsDisabledForProfile(Profile::FromBrowserContext(context));
}

#if BUILDFLAG(IS_CHROMEOS)
bool ChromeExtensionsBrowserClient::IsActiveContext(
    content::BrowserContext* browser_context) const {
  // Since we are creating one instance per profile / user, we should be fine
  // comparing against the active user. That said - if we ever change that,
  // this code will need to be changed.
  return static_cast<Profile*>(browser_context)
      ->IsSameOrParent(ProfileManager::GetActiveUserProfile());
}

std::string ChromeExtensionsBrowserClient::GetUserIdHashFromContext(
    content::BrowserContext* context) {
  return ash::ProfileHelper::GetUserIdHashFromProfile(
      static_cast<Profile*>(context));
}
#endif

bool ChromeExtensionsBrowserClient::IsGuestSession(
    content::BrowserContext* context) const {
  return static_cast<Profile*>(context)->IsGuestSession();
}

bool ChromeExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const ExtensionId& extension_id,
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
    const ProcessMap& process_map,
    const GURL& upstream_url) {
  bool allowed = false;
  if (chrome_url_request_util::AllowCrossRendererResourceLoad(
          request, destination, page_transition, child_id, is_incognito,
          extension, extensions, process_map, upstream_url, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* ChromeExtensionsBrowserClient::GetPrefServiceForContext(
    content::BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrefs();
}

bool ChromeExtensionsBrowserClient::DidVersionUpdate(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);

  // Unit tests may not provide prefs; assume everything is up to date.
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile);
  if (!extension_prefs) {
    return false;
  }

  if (g_did_chrome_update_for_testing) {
    return true;
  }

  // If we're inside a browser test, then assume prefs are all up to date.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kTestType)) {
    return false;
  }

  PrefService* pref_service = extension_prefs->pref_service();
  base::Version last_version;
  if (pref_service->HasPrefPath(pref_names::kLastChromeVersion)) {
    std::string last_version_str =
        pref_service->GetString(pref_names::kLastChromeVersion);
    last_version = base::Version(last_version_str);
  }

  std::string current_version_str(version_info::GetVersionNumber());
  const base::Version& current_version = version_info::GetVersion();
  pref_service->SetString(pref_names::kLastChromeVersion, current_version_str);

  // If there was no version string in prefs, assume we're out of date.
  if (!last_version.IsValid()) {
    return true;
  }
  // If the current version string is invalid, assume we didn't update.
  if (!current_version.IsValid()) {
    return false;
  }

  return last_version < current_version;
}

void ChromeExtensionsBrowserClient::PermitExternalProtocolHandler() {
  ExternalProtocolHandler::PermitLaunchUrl();
}

bool ChromeExtensionsBrowserClient::IsInDemoMode() {
#if BUILDFLAG(IS_CHROMEOS)
  return ash::DemoSession::IsDeviceInDemoMode();
#else
  return false;
#endif
}

bool ChromeExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
#if BUILDFLAG(IS_CHROMEOS)
  return app_id == ash::DemoSession::GetScreensaverAppId() && IsInDemoMode();
#else
  return false;
#endif
}

bool ChromeExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return ::IsRunningInForcedAppMode();
}

bool ChromeExtensionsBrowserClient::IsAppModeForcedForApp(
    const ExtensionId& extension_id) {
  return IsRunningInForcedAppModeForApp(extension_id);
}

bool ChromeExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
#if BUILDFLAG(IS_CHROMEOS)
  return chromeos::IsManagedGuestSession();
#else
  return false;
#endif
}

ExtensionSystemProvider*
ChromeExtensionsBrowserClient::GetExtensionSystemFactory() {
  return ChromeExtensionSystemFactory::GetInstance();
}

std::unique_ptr<RuntimeAPIDelegate>
ChromeExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeRuntimeAPIDelegate>(context);
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
  event_router_forwarder_->BroadcastEventToRenderers(
      histogram_value, event_name, std::move(args),
      dispatch_to_off_the_record_profiles);
}

ExtensionCache* ChromeExtensionsBrowserClient::GetExtensionCache() {
  if (!extension_cache_.get()) {
#if BUILDFLAG(IS_CHROMEOS)
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

void ChromeExtensionsBrowserClient::CreateExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  ChromeExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

ExtensionWebContentsObserver*
ChromeExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return ChromeExtensionWebContentsObserver::FromWebContents(web_contents);
}

void ChromeExtensionsBrowserClient::ClearBackForwardCache() {
  ExtensionTabUtil::ClearBackForwardCache();

  if (on_clear_back_forward_cache_for_test_) {
    std::move(on_clear_back_forward_cache_for_test_).Run();
  }
}

void ChromeExtensionsBrowserClient::AttachExtensionTaskManagerTag(
    content::WebContents* web_contents,
    mojom::ViewType view_type) {
  switch (view_type) {
    case mojom::ViewType::kAppWindow:
    case mojom::ViewType::kComponent:
    case mojom::ViewType::kExtensionBackgroundPage:
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
    case mojom::ViewType::kDeveloperTools:
      // Those types are tracked by other tags:
      // BACKGROUND_CONTENTS --> task_manager::BackgroundContentsTag.
      // GUEST --> ChromeGuestViewManagerDelegate.
      // PANEL --> task_manager::PanelTag.
      // TAB_CONTENTS --> task_manager::TabContentsTag.
      // DEVELOPER_TOOLS --> task_manager::DevToolsTag.
      // These tags are created and attached to the web_contents in other
      // locations, and they must be ignored here.
      return;

    case mojom::ViewType::kInvalid:
      NOTREACHED();
  }
}

scoped_refptr<update_client::UpdateClient>
ChromeExtensionsBrowserClient::CreateUpdateClient(
    content::BrowserContext* context) {
  std::optional<GURL> override_url;
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

std::string ChromeExtensionsBrowserClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeExtensionsBrowserClient::IsExtensionEnabled(
    const ExtensionId& extension_id,
    content::BrowserContext* context) const {
  return ExtensionRegistrar::Get(context)->IsExtensionEnabled(extension_id);
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
  return user_script_listener_.get();
}

void ChromeExtensionsBrowserClient::SignalContentScriptsLoaded(
    content::BrowserContext* context) {
  user_script_listener_->OnScriptsLoaded(context);
}

bool ChromeExtensionsBrowserClient::ShouldSchemeBypassNavigationChecks(
    const std::string& scheme) const {
  if (scheme == chrome::kChromeSearchScheme) {
    return true;
  }

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
    const ExtensionId& extension_id,
    content::BrowserContext* context) {
  return util::HasIsolatedStorage(extension_id, context);
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
    int tab_id,
    bool include_incognito,
    content::WebContents** web_contents) const {
  return ExtensionTabUtil::GetTabById(tab_id, context, include_incognito,
                                      web_contents);
}

bool ChromeExtensionsBrowserClient::IsExtensionTelemetryServiceEnabled(
    content::BrowserContext* context) const {
  // TODO(crbug.com/417279245): Add extensions safe browsing to desktop Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  return telemetry_service && telemetry_service->enabled();
#else
  return false;
#endif
}

void ChromeExtensionsBrowserClient::NotifyExtensionApiTabExecuteScript(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& code) const {
  // TODO(crbug.com/417279245): Add extensions safe browsing to desktop Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  auto signal = std::make_unique<safe_browsing::TabsExecuteScriptSignal>(
      extension_id, code);
  telemetry_service->AddSignal(std::move(signal));
#endif
}

void ChromeExtensionsBrowserClient::NotifyExtensionApiDeclarativeNetRequest(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::vector<api::declarative_net_request::Rule>& rules) const {
  // TODO(crbug.com/417279245): Add extensions safe browsing to desktop Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled()) {
    return;
  }

  // The telemetry service will consume and release the signal object inside the
  // `AddSignal()` call.
  auto signal = std::make_unique<safe_browsing::DeclarativeNetRequestSignal>(
      extension_id, rules);
  telemetry_service->AddSignal(std::move(signal));
#endif
}

void ChromeExtensionsBrowserClient::
    NotifyExtensionDeclarativeNetRequestRedirectAction(
        content::BrowserContext* context,
        const ExtensionId& extension_id,
        const GURL& request_url,
        const GURL& redirect_url) const {
  // TODO(crbug.com/417279245): Add extensions safe browsing to desktop Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  auto* telemetry_service =
      safe_browsing::ExtensionTelemetryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context));
  if (!telemetry_service || !telemetry_service->enabled() ||
      !base::FeatureList::IsEnabled(
          safe_browsing::
              kExtensionTelemetryDeclarativeNetRequestActionSignal)) {
    return;
  }

  // The telemetry service will consume and release the signal object inside the
  // `AddSignal()` call.
  auto signal = safe_browsing::DeclarativeNetRequestActionSignal::
      CreateDeclarativeNetRequestRedirectActionSignal(extension_id, request_url,
                                                      redirect_url);
  telemetry_service->AddSignal(std::move(signal));
#endif
}

bool ChromeExtensionsBrowserClient::IsUsbDeviceAllowedByPolicy(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    int vendor_id,
    int product_id) const {
  url::Origin origin = Extension::CreateOriginFromExtensionId(extension_id);

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
      if (is_chrome_favicon) {
        new_patterns.AddPattern(pattern);
      }
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

void ChromeExtensionsBrowserClient::CreatePasswordReuseDetectionManager(
    content::WebContents* web_contents) const {
  // TODO(crbug.com/417279245): Add extensions safe browsing to desktop Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  ChromePasswordReuseDetectionManagerClient::CreateForWebContents(web_contents);
#endif
}

media_device_salt::MediaDeviceSaltService*
ChromeExtensionsBrowserClient::GetMediaDeviceSaltService(
    content::BrowserContext* context) {
  return MediaDeviceSaltServiceFactory::GetInstance()->GetForBrowserContext(
      context);
}

bool ChromeExtensionsBrowserClient::HasControlledFrameCapability(
    content::BrowserContext* context,
    const GURL& url) {
  // This checks for the controlled frame content setting that is generated from
  // controlled frame admin policies (check
  // components/policy/resources/templates/policy_definitions/ContentSettings).
  return HostContentSettingsMapFactory::GetForProfile(context)
             ->GetContentSetting(url, url,
                                 content_settings::mojom::ContentSettingsType::
                                     CONTROLLED_FRAME) == CONTENT_SETTING_ALLOW;
}

// static
void ChromeExtensionsBrowserClient::set_did_chrome_update_for_testing(
    bool did_update) {
  g_did_chrome_update_for_testing = did_update;
}

}  // namespace extensions
