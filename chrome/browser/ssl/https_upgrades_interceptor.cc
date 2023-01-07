// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_upgrades_interceptor.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_url_loader.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "url/url_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_base.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

// Used to handle upgrading/fallback for tests using EmbeddedTestServer which
// uses random ports.
int g_https_port_for_testing = 0;
int g_http_port_for_testing = 0;

// Only serve upgrade redirects for main frame, GET requests to HTTP URLs. This
// excludes "localhost" (and loopback addresses) as they do not expose traffic
// over the network.
// TODO(crbug.com/1394910): Extend the exemption list for HTTPS-Upgrades
// beyond just localhost.
bool ShouldCreateLoader(const network::ResourceRequest& resource_request,
                        HttpsOnlyModeTabHelper* tab_helper) {
  if (resource_request.is_outermost_main_frame &&
      resource_request.method == "GET" &&
      !net::IsLocalhost(resource_request.url) &&
      resource_request.url.SchemeIs(url::kHttpScheme) &&
      !tab_helper->is_navigation_fallback()) {
    return true;
  }
  return false;
}

}  // namespace

HttpsUpgradesInterceptor::HttpsUpgradesInterceptor(int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

HttpsUpgradesInterceptor::~HttpsUpgradesInterceptor() = default;

void HttpsUpgradesInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there isn't a BrowserContext/Profile for this, then just allow it.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile ||
      !g_browser_process->profile_manager()->IsValidProfile(profile)) {
    std::move(callback).Run({});
    return;
  }

  // TODO(crbug.com/1394910): Check for HttpsUpgrades and HttpsAllowlist
  // enterprise policies as well. It might be best to consolidate these checks
  // into the HttpsUpgradesNavigationThrottle which sees the navigation first.
  auto* prefs = profile->GetPrefs();
  bool https_first_mode_enabled =
      base::FeatureList::IsEnabled(features::kHttpsFirstModeV2) && prefs &&
      prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled);
  bool https_upgrades_enabled =
      base::FeatureList::IsEnabled(features::kHttpsUpgrades) ||
      https_first_mode_enabled;
  if (!https_upgrades_enabled) {
    // Don't upgrade the request and let the default loader continue.
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

  // Don't upgrade navigation if it is allowlisted.
  // TODO(crbug.com/1394910): Distinguish HTTPS-First Mode and HTTPS-Upgrades
  // allowlist entries, and ensure that HTTPS-Upgrades allowlist entries don't
  // downgrade Page Info.
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

    std::move(callback).Run({});
    return;
  }

  if (!ShouldCreateLoader(tentative_resource_request, tab_helper)) {
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
  auto query_complete_callback = base::BindOnce(
      &HttpsUpgradesInterceptor::MaybeCreateLoaderOnHstsQueryCompleted,
      weak_factory_.GetWeakPtr(), tentative_resource_request,
      std::move(callback), tab_helper);
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
    HttpsOnlyModeTabHelper* tab_helper,
    bool is_hsts_active_for_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't upgrade this request if HSTS is active for this host.
  if (is_hsts_active_for_host) {
    std::move(callback).Run({});
    return;
  }

  // Mark navigation as upgraded.
  tab_helper->set_is_navigation_upgraded(true);
  tab_helper->set_fallback_url(tentative_resource_request.url);
  CreateHttpsRedirectLoader(tentative_resource_request, std::move(callback));
  // `redirect_url_loader_` can be null after this call.
  redirect_url_loader_->StartRedirectToHttps(frame_tree_node_id_);
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

// Creates a redirect URL loader that immediately serves a redirect to the
// upgraded HTTPS version of the URL.
void HttpsUpgradesInterceptor::CreateHttpsRedirectLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  redirect_url_loader_ = std::make_unique<HttpsOnlyModeUpgradeURLLoader>(
      tentative_resource_request,
      base::BindOnce(&HttpsUpgradesInterceptor::HandleRedirectLoader,
                     base::Unretained(this), std::move(callback)));
}

// Runs `callback` with `handler`.
void HttpsUpgradesInterceptor::HandleRedirectLoader(
    content::URLLoaderRequestInterceptor::LoaderCallback callback,
    RequestHandler handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Handle any failure by using default loader.
  if (handler.is_null()) {
    redirect_url_loader_.reset();
    // PROCEED.
    std::move(callback).Run({});
    return;
  }

  // `redirect_url_loader_` now manages its own lifetime via a mojo channel.
  // `handler` is guaranteed to be called. It will complete by serving the
  // artificial redirect.
  redirect_url_loader_.release();
  std::move(callback).Run(std::move(handler));
}
