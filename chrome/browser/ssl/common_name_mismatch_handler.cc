// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/common_name_mismatch_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/ssl_errors/error_classification.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

CommonNameMismatchHandler::CommonNameMismatchHandler(
    const GURL& request_url,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : request_url_(request_url),
      url_loader_factory_(std::move(url_loader_factory)) {}

CommonNameMismatchHandler::~CommonNameMismatchHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
CommonNameMismatchHandler::TestingState
    CommonNameMismatchHandler::testing_state_ = NOT_TESTING;

void CommonNameMismatchHandler::CheckSuggestedUrl(
    const GURL& url,
    const CheckUrlCallback& callback) {
  // Should be used only in tests.
  if (testing_state_ == IGNORE_REQUESTS_FOR_TESTING)
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsCheckingSuggestedUrl());
  DCHECK(check_url_callback_.is_null());

  check_url_ = url;
  check_url_callback_ = callback;

  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ssl_name_mismatch_lookup", R"(
        semantics {
          sender: "SSL Name Mismatch Handler"
          description:
            "If Chromium cannot make a secure connection to a site, this can "
            "be because the site is misconfigured. The site may be serving a "
            "security certificate intended for another site. If the SSL Common "
            "Name Mismatch Handling feature is enabled, Chromium will try to "
            "detect if one of the domains listed in the site's certificate is "
            "available by issuing requests to those domains. If the response "
            "indicates that an alternative site for which the certificate is "
            "valid is available, Chromium will automatically redirect the user "
            "to the alternative site."
          trigger: "Resource load."
          data: "An HTTP HEAD request to the alternative site."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can disable this feature by command line flag "
            "'--disable-feature=SSLCommonNameMismatchHandling'."
          policy_exception_justification:
            "Not implemented."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  // Can't safely use net::LOAD_DISABLE_CERT_NETWORK_FETCHES here,
  // since then the connection may be reused without checking the cert.
  resource_request->url = check_url_;
  resource_request->method = "HEAD";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Don't follow redirects to prevent leaking URL data to HTTP sites.
  simple_url_loader_->SetOnRedirectCallback(
      base::BindRepeating(&CommonNameMismatchHandler::OnSimpleLoaderRedirect,
                          base::Unretained(this)));
  simple_url_loader_->SetOnResponseStartedCallback(
      base::BindOnce(&CommonNameMismatchHandler::OnSimpleLoaderResponseStarted,
                     base::Unretained(this)));
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&CommonNameMismatchHandler::OnSimpleLoaderComplete,
                     base::Unretained(this)),
      1 /*max_body_size*/);
}

// static
bool CommonNameMismatchHandler::GetSuggestedUrl(
    const GURL& request_url,
    const std::vector<std::string>& dns_names,
    GURL* suggested_url) {
  std::string www_mismatch_hostname;
  if (!ssl_errors::GetWWWSubDomainMatch(request_url, dns_names,
                                        &www_mismatch_hostname)) {
    return false;
  }
  // The full URL should be pinged, not just the new hostname. So, get the
  // |suggested_url| with the |request_url|'s hostname replaced with
  // new hostname. Keep resource path, query params the same.
  GURL::Replacements replacements;
  replacements.SetHostStr(www_mismatch_hostname);
  *suggested_url = request_url.ReplaceComponents(replacements);
  return true;
}

void CommonNameMismatchHandler::Cancel() {
  simple_url_loader_.reset();
  check_url_callback_.Reset();
}

void CommonNameMismatchHandler::OnSimpleLoaderHandler(
    const GURL& final_url,
    const network::mojom::URLResponseHead* head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsCheckingSuggestedUrl());
  DCHECK(!check_url_callback_.is_null());

  SuggestedUrlCheckResult result = SUGGESTED_URL_NOT_AVAILABLE;

  // Make sure the URL is a HTTPS page and returns a proper response code.
  int response_code = -1;
  // head may be null here, if called from OnSimpleLoaderComplete.
  if (head && head->headers) {
    response_code = head->headers->response_code();
  }
  if (response_code == 200 && final_url.SchemeIsCryptographic() &&
      final_url.host() != request_url_.host()) {
    DCHECK_EQ(final_url.host(), final_url.host());
    result = SUGGESTED_URL_AVAILABLE;
  }
  simple_url_loader_.reset();
  std::move(check_url_callback_).Run(result, check_url_);
}

void CommonNameMismatchHandler::OnSimpleLoaderRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  OnSimpleLoaderHandler(redirect_info.new_url, &response_head);
}

void CommonNameMismatchHandler::OnSimpleLoaderResponseStarted(
    const GURL& final_url,
    const network::mojom::URLResponseHead& response_head) {
  OnSimpleLoaderHandler(final_url, &response_head);
}

void CommonNameMismatchHandler::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  OnSimpleLoaderHandler(simple_url_loader_->GetFinalURL(),
                        simple_url_loader_->ResponseInfo());
}

bool CommonNameMismatchHandler::IsCheckingSuggestedUrl() const {
  return !!simple_url_loader_;
}
