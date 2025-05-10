// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_navigation_throttles.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/enterprise_util.h"
#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"
#include "chrome/browser/plugins/pdf_iframe_navigation_throttle.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_blocking_page_factory.h"
#include "chrome/browser/ssl/https_defaulted_callbacks.h"
#include "chrome/browser/ssl/typed_navigation_upgrade_throttle.h"
#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"
#include "chrome/browser/supervised_user/supervised_user_google_auth_navigation_throttle.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "chrome/browser/ui/login/login_navigation_throttle.h"
#include "chrome/browser/ui/passwords/password_manager_navigation_throttle.h"
#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"
#include "chrome/browser/ui/web_applications/navigation_capturing_redirection_throttle.h"
#include "chrome/common/pref_names.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/fingerprinting_protection_filter/browser/throttle_manager.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/page_load_metrics/browser/metrics_navigation_throttle.h"
#include "components/policy/content/policy_blocklist_navigation_throttle.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "components/security_interstitials/content/ssl_error_navigation_throttle.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"
#include "pdf/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/android/features/dev_ui/buildflags.h"
#include "chrome/browser/download/android/intercept_oma_download_navigation_throttle.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"

#if BUILDFLAG(DFMIFY_DEV_UI)
#include "chrome/browser/dev_ui/android/dev_ui_loader_throttle.h"
#endif  // BUILDFLAG(DFMIFY_DEV_UI)

#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"
#include "chrome/browser/apps/link_capturing/web_app_link_capturing_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/chromeos_disabled_apps_throttle.h"
#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_delegate.h"
#include "chrome/browser/apps/link_capturing/chromeos_reimpl_navigation_capturing_throttle.h"
#include "chrome/browser/ash/login/signin/merge_session_navigation_throttle.h"
#include "chrome/browser/ash/login/signin/merge_session_throttling_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLATFORM_APPS)
#include "chrome/browser/apps/platform_apps/platform_app_navigation_redirector.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_APPS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "extensions/browser/extension_navigation_throttle.h"
#include "extensions/browser/extensions_browser_client.h"
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
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  return web_app::AppBrowserController::IsWebApp(browser);
#else
  return false;
#endif
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
  registry.AddThrottle(InterceptOMADownloadNavigationThrottle::Create(&handle));

#if BUILDFLAG(DFMIFY_DEV_UI)
  // If the DevUI DFM is already installed, then this is a no-op, except for the
  // side effect of ensuring that the DevUI DFM is loaded.
  registry.MaybeAddThrottle(
      dev_ui::DevUiLoaderThrottle::MaybeCreateThrottleFor(&handle));
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
        handle.GetURL().SchemeIsHTTPOrHTTPS()) {
      registry.AddThrottle(
          ash::MergeSessionNavigationThrottle::Create(&handle));
    }
  }

  registry.MaybeAddThrottle(
      apps::ChromeOsDisabledAppsThrottle::MaybeCreate(&handle));
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<apps::LinkCapturingNavigationThrottle::Delegate>
      link_capturing_delegate;

#if BUILDFLAG(IS_CHROMEOS)
  link_capturing_delegate =
      std::make_unique<apps::ChromeOsLinkCapturingDelegate>();
#else   // BUILDFLAG(IS_CHROMEOS)
  link_capturing_delegate =
      std::make_unique<web_app::WebAppLinkCapturingDelegate>();
#endif  // BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<content::NavigationThrottle> url_to_apps_throttle =
      apps::LinkCapturingNavigationThrottle::MaybeCreate(
          &handle, std::move(link_capturing_delegate));

  bool url_to_apps_throttle_created = url_to_apps_throttle != nullptr;
  if (url_to_apps_throttle_created) {
    registry.AddThrottle(std::move(url_to_apps_throttle));
  }
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/366547977): This currently does nothing and allows all
  // navigations to proceed if v2 is enabled on ChromeOS. Implement.
  std::unique_ptr<content::NavigationThrottle>
      chromeos_reimpl_navigation_throttle =
          apps::ChromeOsReimplNavigationCapturingThrottle::MaybeCreate(&handle);
  if (chromeos_reimpl_navigation_throttle) {
    // Verify the v1 throttle has not been created.
    CHECK(!url_to_apps_throttle_created);
    registry.AddThrottle(std::move(chromeos_reimpl_navigation_throttle));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  registry.MaybeAddThrottle(
      web_app::NavigationCapturingRedirectionThrottle::MaybeCreate(&handle));
#endif  // !BUILDFLAG(IS_ANDROID)

  Profile* profile =
      Profile::FromBrowserContext(handle.GetWebContents()->GetBrowserContext());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    registry.AddThrottle(
        std::make_unique<extensions::ExtensionNavigationThrottle>(&handle));

    registry.MaybeAddThrottle(extensions::ExtensionsBrowserClient::Get()
                                  ->GetUserScriptListener()
                                  ->CreateNavigationThrottle(&handle));
  }
#endif

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  registry.MaybeAddThrottle(
      extensions::WebViewGuest::MaybeCreateNavigationThrottle(&handle));
#endif

  registry.MaybeAddThrottle(
      SupervisedUserGoogleAuthNavigationThrottle::MaybeCreate(&handle));

  registry.MaybeAddThrottle(
      supervised_user::MaybeCreateClassifyUrlNavigationThrottleFor(&handle));

  if (auto* throttle_manager =
          subresource_filter::ContentSubresourceFilterThrottleManager::
              FromNavigationHandle(handle)) {
    throttle_manager->MaybeAppendNavigationThrottles(registry);
  }

  if (fingerprinting_protection_filter::features::
          IsFingerprintingProtectionEnabledForIncognitoState(
              profile ? profile->IsIncognitoProfile() : false)) {
    if (auto* throttle_manager = fingerprinting_protection_filter::
            ThrottleManager::FromNavigationHandle(handle)) {
      throttle_manager->MaybeAppendNavigationThrottles(registry);
    }
  }

  registry.MaybeAddThrottle(
      LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(&handle));

  registry.MaybeAddThrottle(
      PDFIFrameNavigationThrottle::MaybeCreateThrottleFor(&handle));

#if BUILDFLAG(ENABLE_PDF)
  registry.AddThrottle(std::make_unique<pdf::PdfNavigationThrottle>(
      &handle, std::make_unique<ChromePdfStreamDelegate>()));
#endif  // BUILDFLAG(ENABLE_PDF)

  registry.MaybeAddThrottle(TabUnderNavigationThrottle::MaybeCreate(&handle));

  registry.MaybeAddThrottle(
      WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(
          &handle));

  registry.MaybeAddThrottle(
      PasswordManagerNavigationThrottle::MaybeCreateThrottleFor(&handle));

  registry.AddThrottle(std::make_unique<PolicyBlocklistNavigationThrottle>(
      registry, handle.GetWebContents()->GetBrowserContext()));

  // Before setting up SSL error detection, configure SSLErrorHandler to invoke
  // the relevant extension API whenever an SSL interstitial is shown.
  SSLErrorHandler::SetClientCallbackOnInterstitialsShown(
      base::BindRepeating(&MaybeTriggerSecurityInterstitialShownEvent));
  registry.AddThrottle(std::make_unique<SSLErrorNavigationThrottle>(
      &handle, base::BindOnce(&HandleSSLErrorWrapper),
      base::BindOnce(&IsInHostedApp),
      base::BindOnce(
          &ShouldIgnoreSslInterstitialBecauseNavigationDefaultedToHttps)));

  registry.AddThrottle(std::make_unique<LoginNavigationThrottle>(&handle));

  if (base::FeatureList::IsEnabled(omnibox::kDefaultTypedNavigationsToHttps)) {
    registry.MaybeAddThrottle(
        TypedNavigationUpgradeThrottle::MaybeCreateThrottleFor(&handle));
  }

  // Add new throttles here.
}
