// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_upgrade_interceptor.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/https_only_mode_tab_storage.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_url_loader.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace {

// Used to handle upgrading/fallback for tests using EmbeddedTestServer which
// uses random ports.
int g_https_port_for_testing = 0;
int g_http_port_for_testing = 0;

// Only serve upgrade redirects for main frame, GET requests to HTTP URLs.
// TODO(crbug.com/1218526): Consider excluding IP addresses and non-unique
// hostnames (as these are likely intranet or unable to have publicly trusted
// certificates).
bool ShouldCreateLoader(const network::ResourceRequest& resource_request) {
  if (resource_request.resource_type !=
          static_cast<int>(blink::mojom::ResourceType::kMainFrame) ||
      !resource_request.url.SchemeIs(url::kHttpScheme) ||
      resource_request.method != "GET" || !resource_request.is_main_frame) {
    return false;
  }

  return true;
}

}  // namespace

HttpsOnlyModeUpgradeInterceptor::HttpsOnlyModeUpgradeInterceptor(
    int frame_tree_node_id)
    : frame_tree_node_id_(frame_tree_node_id) {}

HttpsOnlyModeUpgradeInterceptor::~HttpsOnlyModeUpgradeInterceptor() = default;

// content::URLLoaderRequestInterceptor:
void HttpsOnlyModeUpgradeInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there isn't a BrowserContext/Profile for this, then just allow it.
  if (!browser_context) {
    std::move(callback).Run({});
    return;
  }

  // Don't upgrade if the HTTPS-Only Mode setting isn't enabled.
  auto* prefs = Profile::FromBrowserContext(browser_context)->GetPrefs();
  if (!prefs || !prefs->GetBoolean(prefs::kHttpsOnlyModeEnabled)) {
    std::move(callback).Run({});
    return;
  }

  // Do not upgrade if the hostname is allowlisted.
  auto* tab_storage = HttpsOnlyModeTabStorage::GetOrCreate(
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_));
  if (tab_storage->IsHostAllowlisted(tentative_resource_request.url.host())) {
    std::move(callback).Run({});
    return;
  }

  if (ShouldCreateLoader(tentative_resource_request)) {
    // Mark the navigation as upgraded.
    auto* web_contents =
        content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
    auto* tab_storage = HttpsOnlyModeTabStorage::GetOrCreate(web_contents);
    tab_storage->set_is_navigation_upgraded(true);

    CreateHttpsRedirectLoader(tentative_resource_request, std::move(callback));
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

  // `redirect_url_loader_` can be null after this call.
  redirect_url_loader_->StartRedirectToHttps(frame_tree_node_id_);
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
