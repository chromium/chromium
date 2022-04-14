// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_upgrade_interceptor.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_helper.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_url_loader.h"
#include "chrome/browser/ssl/stateful_ssl_host_state_delegate_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/url_util.h"
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
// The loader also handles redirecting fallback navigations back to HTTP after
// proceeding through the interstitial.
bool ShouldCreateLoader(const network::ResourceRequest& resource_request,
                        HttpsOnlyModeTabHelper* tab_helper) {
  if (resource_request.is_outermost_main_frame &&
      resource_request.method == "GET" &&
      !net::IsLocalhost(resource_request.url) &&
      (resource_request.url.SchemeIs(url::kHttpScheme) ||
       tab_helper->is_navigation_fallback())) {
    return true;
  }
  return false;
}

}  // namespace

HttpsOnlyModeUpgradeInterceptor::HttpsOnlyModeUpgradeInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

HttpsOnlyModeUpgradeInterceptor::~HttpsOnlyModeUpgradeInterceptor() = default;

void HttpsOnlyModeUpgradeInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there isn't a BrowserContext/Profile for this, then just allow it.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    std::move(callback).Run({});
    return;
  }

  // Don't upgrade if the HTTPS-Only Mode setting isn't enabled.
  auto* prefs = profile->GetPrefs();
  if (!prefs || !prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
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

  // TODO(crbug.com/1228188) There might be a race condition where we
  // get here before there is a tab. If the issue was caused by web
  // contents, profile or tab helper being null, try to determine the
  // actual cause using DCHECKs.
  auto* tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    HttpsOnlyModeTabHelper::CreateForWebContents(web_contents);
    tab_helper = HttpsOnlyModeTabHelper::FromWebContents(web_contents);
  }
  if (ShouldCreateLoader(tentative_resource_request, tab_helper)) {
    // If the navigation is a fallback, redirect to the original URL.
    if (tab_helper->is_navigation_fallback()) {
      tab_helper->set_is_navigation_fallback(false);
      CreateHttpsRedirectLoader(tentative_resource_request,
                                std::move(callback));
      redirect_url_loader_->StartRedirectToOriginalURL(
          tab_helper->fallback_url());
      return;
    }

    // Don't upgrade navigation if it is allowlisted.
    StatefulSSLHostStateDelegate* state =
        static_cast<StatefulSSLHostStateDelegate*>(
            profile->GetSSLHostStateDelegate());
    // StatefulSSLHostStateDelegate can be null during tests.
    if (state && state->IsHttpAllowedForHost(
                     tentative_resource_request.url.host(), web_contents)) {
      // Renew the allowlist expiration for this host as the user is still
      // actively using it. This means that the allowlist entry will stay valid
      // until the user stops visiting this host for the entire expiration
      // period (one week).
      state->AllowHttpForHost(tentative_resource_request.url.host(),
                              web_contents);

      std::move(callback).Run({});
      return;
    }

    // Mark navigation as upgraded.
    tab_helper->set_is_navigation_upgraded(true);
    tab_helper->set_fallback_url(tentative_resource_request.url);
    CreateHttpsRedirectLoader(tentative_resource_request, std::move(callback));
    // `redirect_url_loader_` can be null after this call.
    redirect_url_loader_->StartRedirectToHttps(frame_tree_node_id_);
    return;
  }

  // Navigation doesn't meet upgrade criteria, so don't intercept it.
  std::move(callback).Run({});
}

// static
void HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting(int port) {
  g_https_port_for_testing = port;
}

// static
void HttpsOnlyModeUpgradeInterceptor::SetHttpPortForTesting(int port) {
  g_http_port_for_testing = port;
}

// static
int HttpsOnlyModeUpgradeInterceptor::GetHttpsPortForTesting() {
  return g_https_port_for_testing;
}

// static
int HttpsOnlyModeUpgradeInterceptor::GetHttpPortForTesting() {
  return g_http_port_for_testing;
}

// Creates a redirect URL loader that immediately serves a redirect to the
// upgraded HTTPS version of the URL.
void HttpsOnlyModeUpgradeInterceptor::CreateHttpsRedirectLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  redirect_url_loader_ = std::make_unique<HttpsOnlyModeUpgradeURLLoader>(
      tentative_resource_request,
      base::BindOnce(&HttpsOnlyModeUpgradeInterceptor::HandleRedirectLoader,
                     base::Unretained(this), std::move(callback)));
}

// Runs `callback` with `handler`.
void HttpsOnlyModeUpgradeInterceptor::HandleRedirectLoader(
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
