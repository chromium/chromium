// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/gcd_api_flow_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/printing/cloud_print/gcd_constants.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using net::DefineNetworkTrafficAnnotation;

namespace cloud_print {

namespace {

const char kCloudPrintOAuthHeaderKey[] = "Authorization";
const char kCloudPrintOAuthHeaderValueFormat[] = "Bearer %s";
constexpr size_t kMaxContentSize = 1 * 1024 * 1024;

const std::string GetOAuthHeaderValue(const std::string& token) {
  return base::StringPrintf(kCloudPrintOAuthHeaderValueFormat, token.c_str());
}

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    GCDApiFlow::Request::NetworkTrafficAnnotation type) {
  if (type == CloudPrintApiFlowRequest::TYPE_PRIVET_REGISTER) {
    return DefineNetworkTrafficAnnotation("cloud_print_privet_register", R"(
        semantics {
          sender: "Cloud Print"
          description:
            "Registers a locally discovered Privet printer with a Cloud Print "
            "Server."
          trigger:
            "Users can select Privet printers on chrome://devices/ and "
            "register them."
          data:
            "Token id for a printer retrieved from a previous request to a "
            "Cloud Print Server."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "User triggered requests cannot be disabled."
          policy_exception_justification: "Not implemented, it's good to do so."
        })");
  } else {
    DCHECK_EQ(CloudPrintApiFlowRequest::TYPE_SEARCH, type);
    return DefineNetworkTrafficAnnotation("cloud_print_search", R"(
        semantics {
          sender: "Cloud Print"
          description:
            "Queries a Cloud Print Server for the list of printers."
          trigger:
            "chrome://devices/ fetches the list when the user logs in, "
            "re-enable the Cloud Print service, or manually requests a printer "
            "list refresh."
          data: "None"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "User triggered requests cannot be disabled."
          policy_exception_justification: "Not implemented, it's good to do so."
        })");
  }
}

}  // namespace

GCDApiFlowImpl::GCDApiFlowImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

GCDApiFlowImpl::~GCDApiFlowImpl() {}

void GCDApiFlowImpl::Start(std::unique_ptr<Request> request) {
  request_ = std::move(request);
  identity::ScopeSet oauth_scopes;
  oauth_scopes.insert(request_->GetOAuthScope());
  DCHECK(identity_manager_);
  token_fetcher_ = std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
      "cloud_print", identity_manager_, oauth_scopes,
      base::BindOnce(&GCDApiFlowImpl::OnAccessTokenFetchComplete,
                     base::Unretained(this)),
      signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void GCDApiFlowImpl::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    request_->OnGCDApiFlowError(ERROR_TOKEN);
    return;
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_->GetURL();

  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  request->headers.SetHeader(kCloudPrintOAuthHeaderKey,
                             GetOAuthHeaderValue(access_token_info.token));

  std::vector<std::string> extra_headers = request_->GetExtraRequestHeaders();
  for (const std::string& header : extra_headers)
    request->headers.AddHeaderFromString(header);

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(request),
      GetNetworkTrafficAnnotation(request_->GetNetworkTrafficAnnotationType()));

  url_loader_->SetAllowHttpErrorResults(true);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&GCDApiFlowImpl::OnDownloadedToString,
                     weak_factory_.GetWeakPtr()),
      kMaxContentSize);
}

void GCDApiFlowImpl::OnDownloadedToString(
    std::unique_ptr<std::string> response_body) {
  const network::mojom::URLResponseHead* response_info =
      url_loader_->ResponseInfo();

  if (url_loader_->NetError() != net::OK || !response_info) {
    request_->OnGCDApiFlowError(ERROR_NETWORK);
    return;
  }

  if (response_info->headers &&
      response_info->headers->response_code() != net::HTTP_OK) {
    request_->OnGCDApiFlowError(ERROR_HTTP_CODE);
    return;
  }

  base::Optional<base::Value> value = base::JSONReader::Read(*response_body);
  const base::DictionaryValue* dictionary_value = NULL;

  if (!value || !value->GetAsDictionary(&dictionary_value)) {
    request_->OnGCDApiFlowError(ERROR_MALFORMED_RESPONSE);
    return;
  }

  request_->OnGCDApiFlowComplete(*dictionary_value);
}

}  // namespace cloud_print
