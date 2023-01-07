// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/https_only_mode_upgrade_url_loader.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ssl/https_only_mode_upgrade_interceptor.h"
#include "content/public/browser/browser_context.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/resource_request.h"

namespace {

// Updates a URL to HTTPS. URLs with the default port will result in the HTTPS
// URL using the default port 443. URLs with non-default ports won't have the
// port changed. For tests, the HTTPS port used can be overridden with
// HttpsOnlyModeUpgradeInterceptor::SetHttpsPortForTesting().
GURL UpgradeUrlToHttps(const GURL& url) {
  if (url.SchemeIsCryptographic())
    return url;

  // Replace scheme with HTTPS.
  GURL::Replacements upgrade_url;
  upgrade_url.SetSchemeStr(url::kHttpsScheme);

  // For tests that use the EmbeddedTestServer, the server's port needs to be
  // specified as it can't use the default ports.
  int https_port_for_testing =
      HttpsOnlyModeUpgradeInterceptor::GetHttpsPortForTesting();
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

}  // namespace

HttpsOnlyModeUpgradeURLLoader::HttpsOnlyModeUpgradeURLLoader(
    const network::ResourceRequest& tentative_resource_request,
    HandleRequest callback)
    : modified_resource_request_(tentative_resource_request),
      callback_(std::move(callback)) {}

HttpsOnlyModeUpgradeURLLoader::~HttpsOnlyModeUpgradeURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HttpsOnlyModeUpgradeURLLoader::StartRedirectToHttps(
    int frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GURL https_url = UpgradeUrlToHttps(modified_resource_request_.url);

  CreateRedirectInformation(https_url);
  std::move(callback_).Run(base::BindOnce(
      &HttpsOnlyModeUpgradeURLLoader::StartHandlingRedirectToModifiedRequest,
      weak_ptr_factory_.GetWeakPtr()));
}

void HttpsOnlyModeUpgradeURLLoader::StartRedirectToOriginalURL(
    const GURL& original_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateRedirectInformation(original_url);
  std::move(callback_).Run(base::BindOnce(
      &HttpsOnlyModeUpgradeURLLoader::StartHandlingRedirectToModifiedRequest,
      weak_ptr_factory_.GetWeakPtr()));
}

void HttpsOnlyModeUpgradeURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Content should not hang onto old URLLoaders for redirects.
  NOTREACHED();
}

void HttpsOnlyModeUpgradeURLLoader::SetPriority(net::RequestPriority priority,
                                                int32_t intra_priority_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do nothing.
}

void HttpsOnlyModeUpgradeURLLoader::PauseReadingBodyFromNet() {
  // Do nothing.
}
void HttpsOnlyModeUpgradeURLLoader::ResumeReadingBodyFromNet() {
  // Do nothing.
}

void HttpsOnlyModeUpgradeURLLoader::StartHandlingRedirectToModifiedRequest(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  response_head_->request_start = base::TimeTicks::Now();
  response_head_->response_start = response_head_->request_start;

  std::string header_string = base::StringPrintf(
      "HTTP/1.1 %i Temporary Redirect\n"
      "Location: %s\n",
      net::HTTP_TEMPORARY_REDIRECT,
      modified_resource_request_.url.spec().c_str());

  response_head_->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_string));
  response_head_->encoded_data_length = 0;

  StartHandlingRedirect(resource_request, std::move(receiver),
                        std::move(client));
}

void HttpsOnlyModeUpgradeURLLoader::StartHandlingRedirect(
    const network::ResourceRequest& /* resource_request */,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!receiver_.is_bound());

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&HttpsOnlyModeUpgradeURLLoader::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));
  client_->OnReceiveRedirect(redirect_info_, response_head_->Clone());
}

void HttpsOnlyModeUpgradeURLLoader::CreateRedirectInformation(
    const GURL& redirect_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  redirect_info_ = net::RedirectInfo::ComputeRedirectInfo(
      modified_resource_request_.method, modified_resource_request_.url,
      modified_resource_request_.site_for_cookies,
      net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT,
      modified_resource_request_.referrer_policy,
      modified_resource_request_.referrer.spec(), net::HTTP_TEMPORARY_REDIRECT,
      redirect_url,
      /*referrer_policy_header=*/absl::nullopt,
      /*insecure_scheme_was_upgraded=*/false);

  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      modified_resource_request_.url, modified_resource_request_.method,
      redirect_info_, /*removed_headers=*/absl::nullopt,
      /*modified_headers=*/absl::nullopt, &modified_resource_request_.headers,
      &should_clear_upload);

  DCHECK(!should_clear_upload);

  modified_resource_request_.url = redirect_info_.new_url;
  modified_resource_request_.method = redirect_info_.new_method;
  modified_resource_request_.site_for_cookies =
      redirect_info_.new_site_for_cookies;
  modified_resource_request_.referrer = GURL(redirect_info_.new_referrer);
  modified_resource_request_.referrer_policy =
      redirect_info_.new_referrer_policy;

  if (modified_resource_request_.trusted_params.has_value()) {
    auto params = modified_resource_request_.trusted_params.value();
    params.isolation_info =
        modified_resource_request_.trusted_params->isolation_info
            .CreateForRedirect(url::Origin::Create(redirect_url));
    modified_resource_request_.trusted_params = params;
  }
}

void HttpsOnlyModeUpgradeURLLoader::OnConnectionClosed() {
  // This happens when content cancels the navigation. Reset the network request
  // and client handle and destroy `this`.
  receiver_.reset();
  client_.reset();
  delete this;
}
