// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_interceptor.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using security_interstitials::https_only_mode::RecordHttpsFirstModeNavigation;
using security_interstitials::https_only_mode::
    RecordNavigationRequestSecurityLevel;

namespace {

// Used to handle upgrading/fallback for tests using EmbeddedTestServer which
// uses random ports.
int g_https_port_for_testing = 0;
int g_http_port_for_testing = 0;

// Updates a URL to HTTPS. URLs with the default port will result in the HTTPS
// URL using the default port 443. URLs with non-default ports won't have the
// port changed. For tests, the HTTPS port used can be overridden with
// HttpsUpgradesInterceptor::SetHttpsPortForTesting().
GURL UpgradeUrlToHttps(const GURL& url) {
  DCHECK(!url.SchemeIsCryptographic());

  // Replace scheme with HTTPS.
  GURL::Replacements upgrade_url;
  upgrade_url.SetSchemeStr(url::kHttpsScheme);

  // For tests that use the EmbeddedTestServer, the server's port needs to be
  // specified as it can't use the default ports.
  int https_port_for_testing =
      HttpsUpgradesInterceptor::GetHttpsPortForTesting();
  // `port_str` must be in scope for the call to ReplaceComponents() below.
  const std::string port_str = base::NumberToString(https_port_for_testing);
  if (https_port_for_testing) {
    // Only reached in testing, where the original URL will always have a
    // non-default port.
    DCHECK(!url.port().empty());
    upgrade_url.SetPortStr(port_str);
  }

  return url.ReplaceComponents(upgrade_url);
}

// Helper to configure an artificial redirect to `new_url`. This configures
// `response_head` and returns a computed RedirectInfo so both can be passed to
// URLLoaderClient::OnReceiveRedirect() to trigger the redirect.
net::RedirectInfo SetupRedirect(
    const network::ResourceRequest& request,
    const GURL& new_url,
    network::mojom::URLResponseHead* response_head) {
  response_head->encoded_data_length = 0;
  response_head->request_start = base::TimeTicks::Now();
  response_head->response_start = response_head->request_start;
  std::string header_string = base::StringPrintf(
      "HTTP/1.1 %i Temporary Redirect\n"
      "Location: %s\n"
      "Non-Authoritative-Reason: HttpsUpgrades\n",
      net::HTTP_TEMPORARY_REDIRECT, new_url.spec().c_str());
  response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_string));
  net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
      request.method, request.url, request.site_for_cookies,
      request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      request.referrer_policy, request.referrer.spec(),
      net::HTTP_TEMPORARY_REDIRECT, new_url,
      /*referrer_policy_header=*/absl::nullopt,
      /*insecure_scheme_was_upgraded=*/false);
  return redirect_info;
}

// Check whether the HTTP or HTTPS versions of the URL has "Insecure
// Content" allowed in content settings. A user can manually specify hosts
// or hostname patterns (e.g., [*.]example.com) in site settings.
bool DoesInsecureContentSettingDisableUpgrading(const GURL& url,
                                                Profile* profile) {
  // Mixed content isn't an overridable content setting on Android.
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(profile);

  if (!content_settings) {
    return false;
  }

  if (content_settings->GetContentSetting(url, GURL(),
                                          ContentSettingsType::MIXEDSCRIPT) ==
      CONTENT_SETTING_ALLOW) {
    return true;
  }

  // Also check for the HTTPS version of the URL -- if an upgraded page is
  // broken and the user goes through Page Info -> Site Settings and sets
  // "Insecure Content" to be allowed, this will store a site setting only for
  // the HTTPS version of the site.
  GURL https_url = url.SchemeIsCryptographic() ? url : UpgradeUrlToHttps(url);
  if (content_settings->GetContentSetting(https_url, GURL(),
                                          ContentSettingsType::MIXEDSCRIPT) ==
      CONTENT_SETTING_ALLOW) {
    return true;
  }
  return false;
#endif
}

// Check for net errors that should not result in an HTTPS-First Mode
// interstitial. These cover cases where the error is most likely related to the
// local network conditions or a transient error, where it is more useful to
// show the user a network error page than the HTTPS-First Mode interstitial.
// (Or, in the case of HTTPS Upgrades, we don't want to fallback and allowlist
// the hostname yet -- instead we want to show the network error page and then
// retry the HTTPS upgrade again later.)
bool IsHttpsFirstModeExemptedError(int error) {
  return net::IsHostnameResolutionError(error) ||
         error == net::ERR_NETWORK_CHANGED ||
         error == net::ERR_INTERNET_DISCONNECTED ||
         error == net::ERR_ADDRESS_UNREACHABLE;
}

}  // namespace

using RequestHandler = HttpsUpgradesInterceptor::RequestHandler;
using security_interstitials::https_only_mode::Event;
using security_interstitials::https_only_mode::NavigationRequestSecurityLevel;

// static
std::unique_ptr<HttpsUpgradesInterceptor>
HttpsUpgradesInterceptor::MaybeCreateInterceptor(
    int frame_tree_node_id,
    content::NavigationUIData* navigation_ui_data) {
  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  // Could be null if the FrameTreeNode's RenderFrameHost is shutting down.
  if (!web_contents) {
    return nullptr;
  }
  // If there isn't a BrowserContext/Profile for this, then just allow it.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile ||
      !g_browser_process->profile_manager()->IsValidProfile(profile)) {
    return nullptr;
  }
  PrefService* prefs = profile->GetPrefs();
  bool https_first_mode_enabled =
      base::FeatureList::IsEnabled(features::kHttpsFirstModeV2) && prefs &&
      prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  return std::make_unique<HttpsUpgradesInterceptor>(
      frame_tree_node_id, https_first_mode_enabled, navigation_ui_data);
}

HttpsUpgradesInterceptor::HttpsUpgradesInterceptor(
    int frame_tree_node_id,
    bool http_interstitial_enabled_by_pref,
    content::NavigationUIData* navigation_ui_data)
    : frame_tree_node_id_(frame_tree_node_id),
      http_interstitial_enabled_by_pref_(http_interstitial_enabled_by_pref),
      navigation_ui_data_(navigation_ui_data) {}

HttpsUpgradesInterceptor::~HttpsUpgradesInterceptor() = default;

void HttpsUpgradesInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: Redirects cause a restarted request with a new call to
  // MaybeCreateLoader().

  // If there isn't a BrowserContext/Profile for this, then just allow it.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile ||
      !g_browser_process->profile_manager()->IsValidProfile(profile)) {
    std::move(callback).Run({});
    return;
  }

  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  // Could be null if the FrameTreeNode's RenderFrameHost is shutting down.
  if (!web_contents) {
    std::move(callback).Run({});
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If this is a GuestView (e.g., Chrome Apps <webview>) then HTTPS-First Mode
  // should not apply. See crbug.com/1233889 for more details.
  if (guest_view::GuestViewBase::IsGuest(web_contents)) {
    std::move(callback).Run({});
    return;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    HttpsOnlyModeTabHelper::CreateForWebContents(web_contents);
    tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents);
  }

  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());
  auto* storage_partition =
      web_contents->GetPrimaryMainFrame()->GetStoragePartition();

  // Set up the interstitial state before checking any exclusions to upgrades,
  // as some may depend on this being configured.
  interstitial_state_ = std::make_unique<
      security_interstitials::https_only_mode::HttpInterstitialState>();
  interstitial_state_->enabled_by_pref = http_interstitial_enabled_by_pref_;
  // StatefulSSLHostStateDelegate can be null during tests.
  if (state && state->IsHttpsEnforcedForHost(
                   tentative_resource_request.url.host(), storage_partition)) {
    interstitial_state_->enabled_by_engagement_heuristic = true;
  }

  // Exclude HTTPS URLs.
  if (tentative_resource_request.url.SchemeIs(url::kHttpsScheme)) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kSecure);
    std::move(callback).Run({});
    return;
  }

  // Exclude all other schemes other than HTTP.
  if (!tentative_resource_request.url.SchemeIs(url::kHttpScheme)) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kOtherScheme);
    std::move(callback).Run({});
    return;
  }

  // Exclude "localhost" (and loopback addresses) as they do not expose traffic
  // over the network.
  if (net::IsLocalhost(tentative_resource_request.url)) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kLocalhost);
    std::move(callback).Run({});
    return;
  }

  // Don't exclude local-network requests (for now) but record metrics for them.
  if (net::IsHostnameNonUnique(tentative_resource_request.url.host())) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kNonUniqueHostname);

    // For HTTPS-Upgrades, skip attempting to upgrade non-unique hostnames
    // as they can't get publicly-trusted certificates.
    //
    // HTTPS-First Mode does not exempt these hosts in order to ensure that
    // Chrome shows the HTTP interstitial before navigation to them.
    // Potentially, these could fast-fail instead and skip directly to the
    // interstitial.
    if (base::FeatureList::IsEnabled(features::kHttpsUpgrades) &&
        !IsInterstitialEnabled(*interstitial_state_)) {
      std::move(callback).Run({});
      return;
    }
  }

  // If the URL was typed with an explicit http:// URL, it is opted-out from
  // upgrades.
  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data_);
  if (chrome_navigation_ui_data &&
      chrome_navigation_ui_data->url_is_typed_with_http_scheme() &&
      !IsInterstitialEnabled(*interstitial_state_)) {
    if (state) {
      state->AllowHttpForHost(tentative_resource_request.url.host(),
                              storage_partition);
    }
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kExplicitHttpScheme);
    std::move(callback).Run({});
    return;
  }

  // Check whether this host would be upgraded to HTTPS by HSTS. This requires a
  // Mojo call to the network service, so set up a callback to continue the rest
  // of the MaybeCreateLoader() logic (passing along the necessary state). The
  // HSTS status will be passed as a boolean to
  // MaybeCreateLoaderOnHstsQueryCompleted(). If the Mojo call fails, this will
  // default to passing `false` and continuing as though the host does not have
  // HSTS (i.e., it will proceed with the HTTPS-First Mode logic).
  // TODO(crbug.com/1394910): Consider caching this result, at least within the
  // same navigation.
  auto query_complete_callback = base::BindOnce(
      &HttpsUpgradesInterceptor::MaybeCreateLoaderOnHstsQueryCompleted,
      weak_factory_.GetWeakPtr(), tentative_resource_request,
      std::move(callback), profile, web_contents, tab_helper);
  network::mojom::NetworkContext* network_context =
      profile->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->IsHSTSActiveForHost(
      tentative_resource_request.url.host(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(query_complete_callback),
          /*is_hsts_active_for_host=*/false));
}

void HttpsUpgradesInterceptor::MaybeCreateLoaderOnHstsQueryCompleted(
    const network::ResourceRequest& tentative_resource_request,
    content::URLLoaderRequestInterceptor::LoaderCallback callback,
    Profile* profile,
    content::WebContents* web_contents,
    HttpsOnlyModeTabHelper* tab_helper,
    bool is_hsts_active_for_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't upgrade this request if HSTS is active for this host.
  if (is_hsts_active_for_host) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kHstsUpgraded);
    std::move(callback).Run({});
    return;
  }

  // Only serve upgrade redirects for main frame, GET requests.
  if (!tentative_resource_request.is_outermost_main_frame ||
      tentative_resource_request.method != "GET") {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kInsecure);
    std::move(callback).Run({});
    return;
  }

  // Don't upgrade navigation if it is allowlisted.
  // First, check the enterprise policy HTTP allowlist.
  PrefService* prefs = profile->GetPrefs();
  if (IsHostnameInHttpAllowlist(tentative_resource_request.url,
                                profile->GetPrefs())) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kAllowlisted);
    std::move(callback).Run({});
    return;
  }

  // Next check whether the HTTP or HTTPS versions of the URL has "Insecure
  // Content" allowed in content settings. We treat this as a sign to not do
  // silent HTTPS Upgrades for the site overall and not show an HTTPS-First Mode
  // interstitial for Engaged Sites. The main HTTPS-First Mode ignores this
  // setting.
  if (!interstitial_state_->enabled_by_pref &&
      DoesInsecureContentSettingDisableUpgrading(tentative_resource_request.url,
                                                 profile)) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kAllowlisted);
    std::move(callback).Run({});
    return;
  }

  // Then check whether the host has been allowlisted by the user (or by a
  // previous upgrade attempt failing).
  // TODO(crbug.com/1394910): Distinguish HTTPS-First Mode and HTTPS-Upgrades
  // allowlist entries.
  // TODO(crbug.com/1394910): Move this to a helper function `IsAllowlisted()`,
  // especially once this gets more complicated for HFM vs. Upgrades.
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());
  // StatefulSSLHostStateDelegate can be null during tests.
  auto* storage_partition =
      web_contents->GetPrimaryMainFrame()->GetStoragePartition();
  if (state && state->IsHttpAllowedForHost(
                   tentative_resource_request.url.host(), storage_partition)) {
    // Renew the allowlist expiration for this host as the user is still
    // actively using it. This means that the allowlist entry will stay
    // valid until the user stops visiting this host for the entire
    // expiration period (one week).
    state->AllowHttpForHost(tentative_resource_request.url.host(),
                            storage_partition);

    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kInsecure);
    std::move(callback).Run({});
    return;
  }

  // If this is a back/forward navigation to a failed upgrade, then don't
  // intercept to upgrade the navigation. Other forms of re-visiting a URL
  // that previously failed to be upgraded to HTTPS *should* be intercepted so
  // the upgrade can be attempted again (e.g., the user reloading the tab, the
  // user navigating around and ending back on this URL in the same tab, etc.).
  //
  // This effectively "caches" the HTTPS-First Mode interstitial for the
  // history entry of a failed upgrade for the lifetime of the tab. This means
  // that it is possible for a user to come back much later (say, a week later),
  // after a site has fixed its HTTPS configuration, and still see the
  // interstitial for that URL.
  //
  // Without this check, resetting the HTTPS-Upgrades flags in
  // HttpsOnlyModeTabHelper::DidStartNavigation() means the Interceptor would
  // fire on back/forward navigation to the interstitial, which causes an
  // "extra" interstitial entry to be added to the history list and lose other
  // entries.
  auto* entry = web_contents->GetController().GetPendingEntry();
  if (entry && entry->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK &&
      tab_helper->has_failed_upgrade(tentative_resource_request.url)) {
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kInsecure);
    std::move(callback).Run({});
    return;
  }

  // The `HttpsUpgradesEnabled` enterprise policy can be set to `false` to
  // disable HTTPS-Upgrades entirely. Abort if HFM is disabled and the
  // enterprise policy is set.
  if (!prefs->GetBoolean(prefs::kHttpsUpgradesEnabled) &&
      !IsInterstitialEnabled(*interstitial_state_)) {
    RecordHttpsFirstModeNavigation(Event::kUpgradeNotAttempted,
                                   *interstitial_state_);
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kAllowlisted);
    std::move(callback).Run({});
    return;
  }

  // Both HTTPS-First Mode and HTTPS-Upgrades are forms of upgrading all HTTP
  // navigations to HTTPS, with HTTPS-First Mode additionally enabling the
  // HTTP interstitial on fallback.
  if (!base::FeatureList::IsEnabled(features::kHttpsUpgrades) &&
      !IsInterstitialEnabled(*interstitial_state_)) {
    // Don't upgrade the request and let the default loader continue, but record
    // that the request *would have* upgraded, had upgrading been enabled.
    RecordHttpsFirstModeNavigation(Event::kUpgradeNotAttempted,
                                   *interstitial_state_);
    RecordNavigationRequestSecurityLevel(
        NavigationRequestSecurityLevel::kInsecure);
    std::move(callback).Run({});
    return;
  }

  // If the request URL is in the set of URLs that HttpsUpgradesInterceptor has
  // already processed, skip upgrading and trigger fallback to HTTP to avoid a
  // redirect loop.
  if (base::Contains(urls_seen_, tentative_resource_request.url)) {
    // Record failure type metrics for upgraded navigations.
    RecordHttpsFirstModeNavigation(Event::kUpgradeFailed, *interstitial_state_);
    RecordHttpsFirstModeNavigation(Event::kUpgradeRedirectLoop,
                                   *interstitial_state_);

    // If HTTPS-First Mode is not enabled (so no interstitial will be shown),
    // add the fallback hostname to the allowlist now before triggering
    // fallback. HTTPS-First Mode handles this on the user proceeding through
    // the interstitial only.
    // TODO(crbug.com/1446193): Distinguish HTTPS-First Mode and HTTPS-Upgrades
    // allowlist entries, and ensure that HTTPS-Upgrades allowlist entries don't
    // downgrade Page Info.
    // TODO(crbug.com/1394910): Move this to a helper function
    // `AddUrlToAllowlist()`, especially once this gets more complicated for
    // HFM vs. Upgrades.
    if (!IsInterstitialEnabled(*interstitial_state_)) {
      // StatefulSSLHostStateDelegate can be null during tests.
      if (state) {
        state->AllowHttpForHost(
            tab_helper->fallback_url().host(),
            web_contents->GetPrimaryMainFrame()->GetStoragePartition());
      }
    }

    tab_helper->set_is_navigation_upgraded(false);
    tab_helper->set_is_navigation_fallback(true);
    tab_helper->add_failed_upgrade(tab_helper->fallback_url());

    // Note: If `fallback_url` is the same as the request URL, this
    // could skip doing an additional redirect, but then the NavigationThrottle
    // doesn't have the ability to act on this navigation request and apply
    // the HTTPS-First Mode interstitial. If we add a better way to "fast fail"
    // navigations directly to the interstitial, then we could probably use that
    // here as well as an optimization.
    std::move(callback).Run(CreateRedirectHandler(tab_helper->fallback_url()));
    return;
  }
  // Not a redirect loop. Add the current request URL to the set of URLs seen.
  urls_seen_.insert(tentative_resource_request.url);

  RecordNavigationRequestSecurityLevel(
      NavigationRequestSecurityLevel::kUpgraded);

  // Mark navigation as upgraded.
  tab_helper->set_is_navigation_upgraded(true);
  tab_helper->set_fallback_url(tentative_resource_request.url);

  GURL https_url = UpgradeUrlToHttps(tentative_resource_request.url);
  std::move(callback).Run(CreateRedirectHandler(https_url));
}

bool HttpsUpgradesInterceptor::MaybeCreateLoaderForResponse(
    const network::URLLoaderCompletionStatus& status,
    const network::ResourceRequest& request,
    network::mojom::URLResponseHeadPtr* response_head,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    mojo::PendingRemote<network::mojom::URLLoader>* loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
    blink::ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors,
    bool* will_return_unsafe_redirect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // When an upgraded navigation fails, this method creates a loader to trigger
  // the fallback to HTTP.
  //
  // Note: MaybeCreateLoaderForResponse() is called for all navigation
  // responses and failures, but not for things like a NavigationThrottle
  // cancelling or blocking the navigation.

  // Only intercept if the navigation failed.
  if (status.error_code == net::OK) {
    return false;
  }

  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents) {
    // `web_contents` can be null if the tab is being closed. Skip handling
    // failure in that case since the page is going away anyway.
    return false;
  }

  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents);
  if (!tab_helper || !tab_helper->is_navigation_upgraded()) {
    return false;
  }

  // interstitial_state_ is only created after the initial checks in
  // MaybeCreateLoader(). If it wasn't created, the load wasn't eligible for
  // upgrades, so ignore the load here as well. Also explicitly ignore non-main
  // frame loads because we don't want to trigger a fallback navigation in
  // non-main frames.
  // This is a fix for crbug.com/1441276.
  if (!interstitial_state_ || !request.is_outermost_main_frame) {
    return false;
  }

  // If the navigation resulted in an exempted transient network error, don't
  // intercept the failure and allow the normal net error page to trigger. Set
  // the exempt-error flag so if the net error page is reloaded we can try
  // continuing with the upgrade (and fallback to the original URL if the
  // transient network error goes away and the navigation fails).
  // Currently this is only done if the HTTPS-First Mode interstitial is
  // enabled, because otherwise these would cause the interstitial to be shown
  // which is confusing. HTTPS-Upgrades will silently fall back to HTTP for
  // these errors.
  //
  // However, if this is a request to a non-unique hostname, don't prefer
  // net error as it is likely non-recoverable -- we want to fallback to HTTP
  // and the HTTPS-First Mode interstitial in this case. (In particular, this
  // avoids breaking corporate single-label hostnames.)
  if (IsInterstitialEnabled(*interstitial_state_) &&
      IsHttpsFirstModeExemptedError(status.error_code) &&
      !net::IsHostnameNonUnique(request.url.host())) {
    tab_helper->set_is_exempt_error(true);
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  StatefulSSLHostStateDelegate* state =
      static_cast<StatefulSSLHostStateDelegate*>(
          profile->GetSSLHostStateDelegate());

  // Record failure type metrics for upgraded navigations.
  RecordHttpsFirstModeNavigation(Event::kUpgradeFailed, *interstitial_state_);
  if (net::IsCertificateError(status.error_code)) {
    RecordHttpsFirstModeNavigation(Event::kUpgradeCertError,
                                   *interstitial_state_);
  } else if (status.error_code == net::ERR_TIMED_OUT) {
    RecordHttpsFirstModeNavigation(Event::kUpgradeTimedOut,
                                   *interstitial_state_);
  } else {
    RecordHttpsFirstModeNavigation(Event::kUpgradeNetError,
                                   *interstitial_state_);
  }

  // If HTTPS-First Mode is not enabled (so no interstitial will be shown),
  // add the fallback hostname to the allowlist now before triggering fallback.
  // HTTPS-First Mode handles this on the user proceeding through the
  // interstitial only.
  // TODO(crbug.com/1394910): Distinguish HTTPS-First Mode and HTTPS-Upgrades
  // allowlist entries, and ensure that HTTPS-Upgrades allowlist entries don't
  // downgrade Page Info.
  // TODO(crbug.com/1394910): Move this to a helper function
  // `AddUrlToAllowlist()`, especially once this gets more complicated for
  // HFM vs. Upgrades.
  if (!IsInterstitialEnabled(*interstitial_state_)) {
    // StatefulSSLHostStateDelegate can be null during tests.
    if (state) {
      state->AllowHttpForHost(
          tab_helper->fallback_url().host(),
          web_contents->GetPrimaryMainFrame()->GetStoragePartition());
    }
  }

  tab_helper->set_is_navigation_upgraded(false);
  tab_helper->set_is_navigation_fallback(true);
  tab_helper->add_failed_upgrade(tab_helper->fallback_url());

  // `client_` may have been previously bound from handling the initial upgrade
  // in MaybeCreateLoader(), so reset it before re-binding it to handle this
  // response.
  client_.reset();
  *client_receiver = client_.BindNewPipeAndPassReceiver();

  // Create an artificial redirect back to the fallback URL.
  auto new_response_head = network::mojom::URLResponseHead::New();
  net::RedirectInfo redirect_info = SetupRedirect(
      request, tab_helper->fallback_url(), new_response_head.get());

  client_->OnReceiveRedirect(redirect_info, std::move(new_response_head));
  return true;
}

// static
void HttpsUpgradesInterceptor::SetHttpsPortForTesting(int port) {
  g_https_port_for_testing = port;
}

// static
void HttpsUpgradesInterceptor::SetHttpPortForTesting(int port) {
  g_http_port_for_testing = port;
}

// static
int HttpsUpgradesInterceptor::GetHttpsPortForTesting() {
  return g_https_port_for_testing;
}

// static
int HttpsUpgradesInterceptor::GetHttpPortForTesting() {
  return g_http_port_for_testing;
}

RequestHandler HttpsUpgradesInterceptor::CreateRedirectHandler(
    const GURL& new_url) {
  return base::BindOnce(&HttpsUpgradesInterceptor::RedirectHandler,
                        weak_factory_.GetWeakPtr(), new_url);
}

void HttpsUpgradesInterceptor::RedirectHandler(
    const GURL& new_url,
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set up Mojo connection and initiate the redirect. `client_` and `receiver_`
  // may have been previously bound from handling a previous upgrade earlier in
  // the same navigation, so reset them before re-binding them to handle a new
  // redirect.
  receiver_.reset();
  client_.reset();
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&HttpsUpgradesInterceptor::OnConnectionClosed,
                     weak_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));

  // Create redirect.
  auto response_head = network::mojom::URLResponseHead::New();
  net::RedirectInfo redirect_info =
      SetupRedirect(request, new_url, response_head.get());

  client_->OnReceiveRedirect(redirect_info, std::move(response_head));
}

void HttpsUpgradesInterceptor::OnConnectionClosed() {
  // This happens when content cancels the navigation. Reset the receiver and
  // client handle.
  receiver_.reset();
  client_.reset();
}
