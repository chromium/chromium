// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/digital_asset_links/digital_asset_links_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

namespace {

// In some cases we get a network change while fetching the digital asset
// links file. See https://crbug.com/987329.
const int kNumNetworkRetries = 1;

// Location on a website where the asset links file can be found, see
// https://developers.google.com/digital-asset-links/v1/getting-started.
const char kAssetLinksAbsolutePath[] = ".well-known/assetlinks.json";

GURL GetUrlForAssetLinks(const url::Origin& origin) {
  return origin.GetURL().Resolve(kAssetLinksAbsolutePath);
}

// An example, well formed asset links file for reference:
//  [{
//    "relation": ["delegate_permission/common.handle_all_urls"],
//    "target": {
//      "namespace": "android_app",
//      "package_name": "com.peter.trustedpetersactivity",
//      "sha256_cert_fingerprints": [
//        "FA:2A:03: ... :9D"
//      ]
//    }
//  }, {
//    "relation": ["delegate_permission/common.handle_all_urls"],
//    "target": {
//      "namespace": "android_app",
//      "package_name": "com.example.firstapp",
//      "sha256_cert_fingerprints": [
//        "64:2F:D4: ... :C1"
//      ]
//    }
//  }]

bool StatementHasMatchingRelationship(const base::Value& statement,
                                      const std::string& target_relation) {
  const base::Value* relations =
      statement.FindKeyOfType("relation", base::Value::Type::LIST);

  if (!relations)
    return false;

  for (const auto& relation : relations->GetList()) {
    if (relation.is_string() && relation.GetString() == target_relation)
      return true;
  }

  return false;
}

bool StatementHasMatchingPackage(const base::Value& statement,
                                 const std::string& target_package) {
  const base::Value* package = statement.FindPathOfType(
      {"target", "package_name"}, base::Value::Type::STRING);

  return package && package->GetString() == target_package;
}

bool StatementHasMatchingFingerprint(const base::Value& statement,
                                     const std::string& target_fingerprint) {
  const base::Value* fingerprints = statement.FindPathOfType(
      {"target", "sha256_cert_fingerprints"}, base::Value::Type::LIST);

  if (!fingerprints)
    return false;

  for (const auto& fingerprint : fingerprints->GetList()) {
    if (fingerprint.is_string() &&
        fingerprint.GetString() == target_fingerprint) {
      return true;
    }
  }

  return false;
}

// Shows a warning message in the DevTools console.
void AddMessageToConsole(content::WebContents* web_contents,
                         const std::string& message) {
  if (web_contents) {
    web_contents->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning, message);
    return;
  }

  // Fallback to LOG.
  LOG(WARNING) << message;
}

}  // namespace

namespace digital_asset_links {

const char kDigitalAssetLinksCheckResponseKeyLinked[] = "linked";

DigitalAssetLinksHandler::DigitalAssetLinksHandler(
    content::WebContents* web_contents,
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : content::WebContentsObserver(web_contents),
      shared_url_loader_factory_(std::move(factory)) {}

DigitalAssetLinksHandler::~DigitalAssetLinksHandler() = default;

void DigitalAssetLinksHandler::OnURLLoadComplete(
    const std::string& package,
    const std::string& fingerprint,
    const std::string& relationship,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  if (!response_body || response_code != net::HTTP_OK) {
    int net_error = url_loader_->NetError();
    if (net_error == net::ERR_INTERNET_DISCONNECTED ||
        net_error == net::ERR_NAME_NOT_RESOLVED) {
      AddMessageToConsole(web_contents(),
                          "Digital Asset Links connection failed.");
      std::move(callback_).Run(RelationshipCheckResult::NO_CONNECTION);
      return;
    }

    AddMessageToConsole(
        web_contents(),
        base::StringPrintf(
            "Digital Asset Links endpoint responded with code %d.",
            response_code));
    std::move(callback_).Run(RelationshipCheckResult::FAILURE);
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      *response_body,
      base::BindOnce(&DigitalAssetLinksHandler::OnJSONParseResult,
                     weak_ptr_factory_.GetWeakPtr(), package, fingerprint,
                     relationship));

  url_loader_.reset(nullptr);
}

void DigitalAssetLinksHandler::OnJSONParseResult(
    const std::string& package,
    const std::string& fingerprint,
    const std::string& relationship,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.value) {
    AddMessageToConsole(
        web_contents(),
        "Digital Asset Links response parsing failed with message: " +
            *result.error);
    std::move(callback_).Run(RelationshipCheckResult::FAILURE);
    return;
  }

  auto& statement_list = *result.value;
  if (!statement_list.is_list()) {
    std::move(callback_).Run(RelationshipCheckResult::FAILURE);
    AddMessageToConsole(web_contents(), "Statement List is not a list.");
    return;
  }

  // We only output individual statement failures if none match.
  std::vector<std::string> failures;

  for (const auto& statement : statement_list.GetList()) {
    if (!statement.is_dict()) {
      failures.push_back("Statement is not a dictionary.");
      continue;
    }

    if (!StatementHasMatchingRelationship(statement, relationship)) {
      failures.push_back("Statement failure matching relationship.");
      continue;
    }

    if (!StatementHasMatchingPackage(statement, package)) {
      failures.push_back("Statement failure matching package.");
      continue;
    }

    if (!StatementHasMatchingFingerprint(statement, fingerprint)) {
      failures.push_back("Statement failure matching fingerprint.");
      continue;
    }

    std::move(callback_).Run(RelationshipCheckResult::SUCCESS);
    return;
  }

  for (const auto& failure_reason : failures)
    AddMessageToConsole(web_contents(), failure_reason);

  std::move(callback_).Run(RelationshipCheckResult::FAILURE);
}

bool DigitalAssetLinksHandler::CheckDigitalAssetLinkRelationship(
    RelationshipCheckResultCallback callback,
    const std::string& web_domain,
    const std::string& package,
    const std::string& fingerprint,
    const std::string& relationship) {
  // TODO(peconn): Propegate the use of url::Origin backwards to clients.
  GURL request_url = GetUrlForAssetLinks(url::Origin::Create(GURL(web_domain)));

  if (!request_url.is_valid())
    return false;

  // Resetting both the callback and SimpleURLLoader here to ensure
  // that any previous requests will never get a
  // OnURLLoadComplete. This effectively cancels any checks that was
  // done over this handler.
  callback_ = std::move(callback);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("digital_asset_links", R"(
        semantics {
          sender: "Digital Asset Links Handler"
          description:
            "Digital Asset Links APIs allows any caller to check pre declared"
            "relationships between two assets which can be either web domains"
            "or native applications. This requests checks for a specific "
            "relationship declared by a web site with an Android application"
          trigger:
            "When the related application makes a claim to have the queried"
            "relationship with the web domain"
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting: "Not user controlled. But the verification is a trusted API"
                   "that doesn't use user data"
          policy_exception_justification:
            "Not implemented, considered not useful as no content is being "
            "uploaded; this request merely downloads the resources on the web."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = request_url;
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetRetryOptions(
      kNumNetworkRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      shared_url_loader_factory_.get(),
      base::BindOnce(&DigitalAssetLinksHandler::OnURLLoadComplete,
                     weak_ptr_factory_.GetWeakPtr(), package, fingerprint,
                     relationship));

  return true;
}

}  // namespace digital_asset_links
