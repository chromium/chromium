// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_navigation_throttles.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"
#include "chrome/browser/enterprise/data_protection/view_source_navigation_throttle.h"
#include "chrome/browser/first_party_sets/first_party_sets_navigation_throttle.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"
#include "chrome/browser/policy/chrome_policy_blocklist_service_factory.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/https_defaulted_callbacks.h"
#include "chrome/browser/ssl/https_upgrades_navigation_throttle.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/browser/ui/login/login_navigation_throttle.h"
#include "chrome/browser/ui/passwords/password_manager_navigation_throttle.h"
#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"
#include "chrome/browser/ui/web_applications/navigation_capturing_redirection_throttle.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/contextual_tasks/public/features.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/history/content/browser/visited_link_navigation_throttle.h"
#include "components/lens/lens_features.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/payments/content/payment_handler_navigation_throttle.h"
#include "components/policy/content/policy_blocklist_navigation_throttle.h"
#include "components/policy/content/safe_search_service.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_interstitials/content/insecure_form_navigation_throttle.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "pdf/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/android/features/dev_ui/buildflags.h"
#include "chrome/browser/download/android/intercept_oma_download_navigation_throttle.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"

#if BUILDFLAG(DFMIFY_DEV_UI)
#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"
#endif  // BUILDFLAG(DFMIFY_DEV_UI)

#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/actor_navigation_throttle.h"
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"
#include "chrome/browser/apps/link_capturing/web_app_link_capturing_delegate.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_navigation_throttle.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/page_info/web_view_side_panel_throttle.h"
#include "chrome/browser/preloading/preview/preview_navigation_throttle.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_navigation_throttle.h"
#include "chrome/browser/ui/read_anything/read_anything_side_panel_navigation_throttle.h"
#include "chrome/browser/ui/search/new_tab_page_navigation_throttle.h"
#include "chrome/browser/ui/web_applications/tabbed_web_app_navigation_throttle.h"
#include "chrome/browser/ui/web_applications/webui_web_app_navigation_throttle.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_response_capture_navigation_throttle.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_throttle.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_install/app_install_navigation_throttle.h"
#include "chrome/browser/apps/intent_helper/chromeos_disabled_apps_throttle.h"
#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_delegate.h"
#include "chrome/browser/apps/link_capturing/chromeos_reimpl_navigation_capturing_throttle.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_navigation_throttle.h"
#include "chrome/browser/ash/login/signin/merge_session_navigation_throttle.h"
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#include "chrome/browser/chromeos/app_mode/kiosk_settings_navigation_throttle.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/browser/apps/platform_apps/platform_app_navigation_redirector.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_APPS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"
#include "components/pdf/browser/pdf_navigation_throttle.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"
#include "chrome/browser/enterprise/profile_management/oidc_auth_response_capture_navigation_throttle.h"
#include "chrome/browser/enterprise/profile_management/profile_management_navigation_throttle.h"
#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"
#include "chrome/browser/enterprise/webstore/chrome_web_store_navigation_throttle.h"
#include "chrome/browser/enterprise/webstore/features.h"
#include "chrome/browser/ui/webui/app_settings/web_app_settings_navigation_throttle.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/safe_browsing/delayed_warning_navigation_throttle.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_throttle.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/mac/auth_session_request.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_navigation_throttle.h"
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/platform_auth/platform_auth_navigation_throttle.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#include "chrome/browser/enterprise/incognito/incognito_navigation_throttle.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extensions_browser_client.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

namespace {

// Wrapper for SSLErrorHandler::HandleSSLError() that supplies //chrome-level
// parameters.
void HandleSSLErrorWrapper(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    SSLErrorHandler::BlockingPageReadyCallback blocking_page_ready_callback) {
  DCHECK(request_url.SchemeIsCryptographic());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // Profile should always outlive a WebContents
  DCHECK(profile);

  captive_portal::CaptivePortalService* captive_portal_service = nullptr;

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal_service = CaptivePortalServiceFactory::GetForProfile(profile);
#endif

  const bool is_ssl_error_override_allowed_for_origin =
      policy::IsOriginInAllowlist(request_url, profile->GetPrefs(),
                                  prefs::kSSLErrorOverrideAllowedForOrigins,
                                  prefs::kSSLErrorOverrideAllowed);

  SSLErrorHandler::HandleSSLError(
      web_contents, cert_error, ssl_info, request_url,
      std::move(blocking_page_ready_callback),
      g_browser_process->network_time_tracker(), captive_portal_service,
      std::make_unique<ChromeSecurityBlockingPageFactory>(),
      is_ssl_error_override_allowed_for_origin);
}

// Returns whether `web_contents` is within a hosted app.
bool IsInHostedApp(content::WebContents* web_contents) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  return tab && web_app::AppBrowserController::IsWebApp(
                    tab->GetBrowserWindowInterface());
#else
  return false;
#endif
}

bool IsErrorPageAutoReloadEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableAutomation)) {
    return false;
  }
  if (command_line.HasSwitch(switches::kEnableAutoReload)) {
    return true;
  }
  if (command_line.HasSwitch(switches::kDisableAutoReload)) {
    return false;
  }
  return true;
}

// NOTE: MaybeCreateVisitedLinkNavigationThrottleFor is defined here due to
// usage of Profile code which lives in chrome/. The rest of the
// VisitedLinkNavigationThrottle class lives in components/, which cannot access
// chrome/ code due to layering.
void MaybeCreateAndAddVisitedLinkNavigationThrottle(
    content::NavigationThrottleRegistry& registry) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
    return;
  }
  Profile* profile = Profile::FromBrowserContext(
      registry.GetNavigationHandle().GetWebContents()->GetBrowserContext());
  // Off-the-record profiles do not record history or visited links.
  if (profile->IsOffTheRecord()) {
    return;
  }
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::IMPLICIT_ACCESS);
  if (!history_service) {
    return;
  }
  registry.AddThrottle(std::make_unique<VisitedLinkNavigationThrottle>(
      registry, history_service));
}

}  // namespace

void CreateAndAddChromeThrottlesForNavigation(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (handle.IsInMainFrame()) {
    // MetricsNavigationThrottle requires that it runs before
    // NavigationThrottles that may delay or cancel navigations, so only
    // NavigationThrottles that don't delay or cancel navigations (e.g.
    // throttles that are only observing callbacks without affecting navigation
    // behavior) should be added before MetricsNavigationThrottle.
    // TODO(https://crbug.com/412524375): This assumption is fragile. This
    // should be cared by adding an attribute flag to
    // NavigationThrottleRegistry::AddThrottle().
    page_load_metrics::MetricsNavigationThrottle::CreateAndAdd(registry);
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(davidben): This is insufficient to integrate with prerender properly.
  // https://crbug.com/370595
  prerender::NoStatePrefetchContents* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          handle.GetWebContents());
  if (!no_state_prefetch_contents) {
    navigation_interception::InterceptNavigationDelegate::MaybeCreateAndAdd(
        registry, navigation_interception::SynchronyMode::kAsync);
  }
  InterceptOMADownloadNavigationThrottle::CreateAndAdd(registry);

#if BUILDFLAG(DFMIFY_DEV_UI)
  // If the DevUI DFM is already installed, then this is a no-op, except for the
  // side effect of ensuring that the DevUI DFM is loaded.
  dev_ui::DevUiLoaderThrottle::MaybeCreateAndAdd(registry);
#endif  // BUILDFLAG(DFMIFY_DEV_UI)

#elif BUILDFLAG(ENABLE_PLATFORM_APPS)
  // Redirect some navigations to apps that have registered matching URL
  // handlers ('url_handlers' in the manifest).
  PlatformAppNavigationRedirector::MaybeCreateAndAdd(registry);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  // Check if we need to add merge session throttle. This throttle will postpone
  // loading of main frames.
  if (handle.IsInMainFrame()) {
    // Add interstitial page while merge session process (cookie reconstruction
    // from OAuth2 refresh token in ChromeOS login) is still in progress while
    // we are attempting to load a google property.
    if (ash::merge_session_throttling_utils::ShouldAttachNavigationThrottle() &&
        !ash::merge_session_throttling_utils::AreAllSessionMergedAlready() &&
        registry.IsHTTPOrHTTPS()) {
      ash::MergeSessionNavigationThrottle::CreateAndAdd(registry);
    }
  }

  apps::ChromeOsDisabledAppsThrottle::MaybeCreateAndAdd(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<apps::LinkCapturingNavigationThrottle::Delegate>
      link_capturing_delegate;

#if BUILDFLAG(IS_CHROMEOS)
  link_capturing_delegate =
      std::make_unique<apps::ChromeOsLinkCapturingDelegate>();
  bool url_to_apps_throttle_created =
#else   // BUILDFLAG(IS_CHROMEOS)
  link_capturing_delegate =
      std::make_unique<web_app::WebAppLinkCapturingDelegate>();
#endif  // BUILDFLAG(IS_CHROMEOS)
      apps::LinkCapturingNavigationThrottle::MaybeCreateAndAdd(
          registry, std::move(link_capturing_delegate));
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/366547977): This currently does nothing and allows all
  // navigations to proceed if v2 is enabled on ChromeOS. Implement.
  bool chromeos_reimpl_navigation_throttle_created =
      apps::ChromeOsReimplNavigationCapturingThrottle::MaybeCreateAndAdd(
          registry);
  // Verify the v1 and reimpl throttles have not been created at the same time.
  CHECK(!chromeos_reimpl_navigation_throttle_created ||
        !url_to_apps_throttle_created);
#endif  // BUILDFLAG(IS_CHROMEOS)

  web_app::NavigationCapturingRedirectionThrottle::MaybeCreateAndAdd(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

  Profile* profile =
      Profile::FromBrowserContext(handle.GetWebContents()->GetBrowserContext());

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (!extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    registry.AddThrottle(
        std::make_unique<extensions::ExtensionNavigationThrottle>(registry));

    extensions::ExtensionsBrowserClient::Get()
        ->GetUserScriptListener()
        ->CreateAndAddNavigationThrottle(registry);
  }
#endif

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  extensions::WebViewGuest::MaybeCreateAndAddNavigationThrottle(registry);
#endif

  SupervisedUserGoogleAuthNavigationThrottle::MaybeCreateAndAdd(registry);
  supervised_user::ClassifyUrlNavigationThrottle::MaybeCreateAndAdd(registry);

  if (auto* throttle_manager =
          subresource_filter::ContentSubresourceFilterThrottleManager::
              FromNavigationHandle(handle)) {
    throttle_manager->MaybeCreateAndAddNavigationThrottles(registry);
  }

  LookalikeUrlNavigationThrottle::MaybeCreateAndAdd(registry);

  PDFIFrameNavigationThrottle::MaybeCreateAndAdd(registry);

#if BUILDFLAG(ENABLE_PDF)
  registry.AddThrottle(std::make_unique<pdf::PdfNavigationThrottle>(
      registry, std::make_unique<ChromePdfStreamDelegate>()));
#endif  // BUILDFLAG(ENABLE_PDF)

  TabUnderNavigationThrottle::MaybeCreateAndAdd(registry);

  WellKnownChangePasswordNavigationThrottle::MaybeCreateAndAdd(registry);

  PasswordManagerNavigationThrottle::MaybeCreateAndAdd(registry);

  content::BrowserContext* context =
      handle.GetWebContents()->GetBrowserContext();
  registry.AddThrottle(std::make_unique<PolicyBlocklistNavigationThrottle>(
      registry, user_prefs::UserPrefs::Get(context),
      ChromePolicyBlocklistServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context)),
      SafeSearchFactory::GetForBrowserContext(context)));

  // Before setting up SSL error detection, configure SSLErrorHandler to invoke
  // the relevant extension API whenever an SSL interstitial is shown.
  SSLErrorHandler::SetClientCallbackOnInterstitialsShown(
      base::BindRepeating(&MaybeTriggerSecurityInterstitialShownEvent));
  registry.AddThrottle(std::make_unique<SSLErrorNavigationThrottle>(
      registry, base::BindOnce(&HandleSSLErrorWrapper),
      base::BindOnce(&IsInHostedApp),
      base::BindOnce(
          &ShouldIgnoreSslInterstitialBecauseNavigationDefaultedToHttps)));

  registry.AddThrottle(std::make_unique<LoginNavigationThrottle>(registry));

  if (base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps)) {
    TypedNavigationUpgradeThrottle::MaybeCreateAndAdd(registry);
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  WebAppSettingsNavigationThrottle::MaybeCreateAndAdd(registry);
  profile_management::ProfileManagementNavigationThrottle::MaybeCreateAndAdd(
      registry);
  profile_management::OidcAuthResponseCaptureNavigationThrottle::
      MaybeCreateAndAdd(registry);

  ManagedProfileRequiredNavigationThrottle::MaybeCreateAndAdd(registry);

  if (base::FeatureList::IsEnabled(
          enterprise::webstore::kChromeWebStoreNavigationThrottle)) {
    registry.AddThrottle(
        std::make_unique<enterprise_webstore::ChromeWebStoreNavigationThrottle>(
            registry));
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  enterprise_connectors::DeviceTrustNavigationThrottle::MaybeCreateAndAdd(
      registry);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    contextual_tasks::ContextualTasksNavigationThrottle::MaybeCreateAndAdd(
        registry);
  }

  DevToolsWindow::MaybeCreateAndAddNavigationThrottle(registry);

  NewTabPageNavigationThrottle::MaybeCreateAndAdd(registry);

  web_app::TabbedWebAppNavigationThrottle::MaybeCreateAndAdd(registry);

  web_app::WebUIWebAppNavigationThrottle::MaybeCreateAndAdd(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // g_browser_process->safe_browsing_service() may be null in unittests.
  safe_browsing::SafeBrowsingUIManager* ui_manager =
      g_browser_process->safe_browsing_service()
          ? g_browser_process->safe_browsing_service()->ui_manager().get()
          : nullptr;
  safe_browsing::SafeBrowsingNavigationThrottle::MaybeCreateAndAdd(registry,
                                                                   ui_manager);

  if (base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings)) {
    registry.AddThrottle(
        std::make_unique<safe_browsing::DelayedWarningNavigationThrottle>(
            registry));
  }
  enterprise_data_protection::ViewSourceNavigationThrottle::MaybeCreateAndAdd(
      registry, ui_manager);
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  browser_switcher::BrowserSwitcherNavigationThrottle::MaybeCreateAndAdd(
      registry);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
  chromeos::KioskSettingsNavigationThrottle::MaybeCreateAndAdd(registry);

  ash::OnTaskLockedSessionNavigationThrottle::MaybeCreateAndAdd(registry);
#endif

#if BUILDFLAG(IS_MAC)
  MaybeCreateAndAddAuthSessionNavigationThrottle(registry);
#endif

  if (profile && profile->GetPrefs()) {
    security_interstitials::InsecureFormNavigationThrottle::MaybeCreateAndAdd(
        registry, std::make_unique<ChromeSecurityBlockingPageFactory>(),
        profile->GetPrefs());
  }

  if (IsErrorPageAutoReloadEnabled()) {
    error_page::NetErrorAutoReloader::MaybeCreateAndAddNavigationThrottle(
        registry);
  }

  payments::PaymentHandlerNavigationThrottle::MaybeCreateAndAdd(registry);

#if !BUILDFLAG(IS_ANDROID)
  ReadAnythingSidePanelNavigationThrottle::CreateAndAdd(registry);

  if (lens::features::IsLensOverlayEnabled()) {
    if (profile) {
      if (ThemeService* theme_service =
              ThemeServiceFactory::GetForProfile(profile)) {
        lens::LensOverlaySidePanelNavigationThrottle::MaybeCreateAndAdd(
            registry, theme_service);
      }
    }
  }

  NtpMicrosoftAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(
      registry);
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageNavigationThrottle::MaybeCreateAndAdd(registry);
#endif

  if (profile) {
    HttpsUpgradesNavigationThrottle::MaybeCreateAndAdd(
        registry, std::make_unique<ChromeSecurityBlockingPageFactory>(),
        profile);
  }

#if !BUILDFLAG(IS_ANDROID)
  MaybeCreateAndAddWebViewSidePanelThrottle(registry);
#endif

  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  if (privacy_sandbox_settings &&
      privacy_sandbox_settings->AreRelatedWebsiteSetsEnabled()) {
    first_party_sets::FirstPartySetsNavigationThrottle::MaybeCreateAndAdd(
        registry);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Don't perform platform authentication in incognito and guest profiles.
  if (profile && !profile->IsOffTheRecord()) {
    enterprise_auth::PlatformAuthNavigationThrottle::MaybeCreateAndAdd(
        registry);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(b:296844164) Handle captive portal signin properly.
  if (profile && profile->IsIncognitoProfile() && profile->IsOffTheRecord() &&
      !profile->GetOTRProfileID().IsCaptivePortal()) {
    enterprise_incognito::IncognitoNavigationThrottle::MaybeCreateAndAdd(
        registry);
  }

  apps::AppInstallNavigationThrottle::MaybeCreateAndAdd(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (profile && profile->IsIncognitoProfile() && profile->IsOffTheRecord()) {
    enterprise_incognito::IncognitoNavigationThrottle::MaybeCreateAndAdd(
        registry);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if !BUILDFLAG(IS_ANDROID)
  PreviewNavigationThrottle::MaybeCreateAndAdd(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

  MaybeCreateAndAddVisitedLinkNavigationThrottle(registry);

  data_sharing::DataSharingNavigationThrottle::MaybeCreateAndAdd(registry);

#if !BUILDFLAG(IS_ANDROID)
  web_app::IsolatedWebAppThrottle::MaybeCreateAndAdd(registry);

  actor::ActorNavigationThrottle::MaybeCreateAndAdd(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

  dom_distiller::DistillerPageWebContents::MaybeCreateAndAddNavigationThrottle(
      registry);
}
