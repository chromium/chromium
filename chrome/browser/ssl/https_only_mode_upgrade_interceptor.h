// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_UPGRADE_INTERCEPTOR_H_
#define CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_UPGRADE_INTERCEPTOR_H_

#include <memory>

#include "base/sequence_checker.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_url_loader.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class Profile;

// A class that attempts to intercept HTTP navigation requests and redirect them
// to HTTPS. Its lifetime matches that of the content/ navigation loader code.
class HttpsOnlyModeUpgradeInterceptor
    : public content::URLLoaderRequestInterceptor {
 public:
  explicit HttpsOnlyModeUpgradeInterceptor(int frame_tree_node_id);
  ~HttpsOnlyModeUpgradeInterceptor() override;

  HttpsOnlyModeUpgradeInterceptor(const HttpsOnlyModeUpgradeInterceptor&) =
      delete;
  HttpsOnlyModeUpgradeInterceptor& operator=(
      const HttpsOnlyModeUpgradeInterceptor&) = delete;

  // content::URLLoaderRequestInterceptor:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

  // Continuation of MaybeCreateLoader() after querying the network service for
  // the HSTS status for the hostname in the request.
  void MaybeCreateLoaderOnHstsQueryCompleted(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback,
      Profile* profile,
      content::WebContents* web_contents,
      bool is_hsts_active_for_host);

  // Sets the ports used by the EmbeddedTestServer (which uses random ports)
  // to determine the correct port to upgrade/fallback to in tests.
  static void SetHttpsPortForTesting(int port);
  static void SetHttpPortForTesting(int port);
  static int GetHttpsPortForTesting();
  static int GetHttpPortForTesting();

 private:
  // Creates a URL loader that immediately serves a redirect to the HTTPS
  // version of the URL.
  void CreateHttpsRedirectLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::URLLoaderRequestInterceptor::LoaderCallback callback);

  // Runs `callback` with `handler`.
  void HandleRedirectLoader(
      content::URLLoaderRequestInterceptor::LoaderCallback callback,
      RequestHandler handler);

  // URLLoader that serves redirects.
  std::unique_ptr<HttpsOnlyModeUpgradeURLLoader> redirect_url_loader_;

  // Used to access the WebContents for the navigation.
  int frame_tree_node_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HttpsOnlyModeUpgradeInterceptor> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SSL_HTTPS_ONLY_MODE_UPGRADE_INTERCEPTOR_H_
