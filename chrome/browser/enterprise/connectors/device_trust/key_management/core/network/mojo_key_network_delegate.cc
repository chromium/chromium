// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace enterprise_connectors {
namespace {

constexpr int kMaxRetryCount = 7;

std::unique_ptr<network::SimpleURLLoader> CreateURLLoader(
    const GURL& url,
    const std::string& dm_token,
    const std::string& body) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("device_trust_key_rotation", R"(
        semantics {
          sender: "Enterprise Device Trust"
          description:
            "When the Device Trust connector is enabled via an enterprise "
            "policy, Chrome generates and sends a public attestation key "
            "to Google's Device Management server for usage when doing "
            "device  attestation. Admins can then issue key rotation "
            "requests to rotate the key-pair and get another public key "
            "uploaded."
          trigger:
            "When an enterprise policy is activated, or when enterprise "
            "administrators issue key rotation remote commands."
          data: "Public key and Google DM token."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "You can enable or disable this feature via the Google Admin "
            "Console by updating the Device Trust connector settings. "
            "The feature is disabled by default."
          chrome_policy {
            ContextAwareAccessSignalsAllowlist {
                ContextAwareAccessSignalsAllowlist {
                  entries: "*"
                }
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  static constexpr char kDMTokenHeaderFormat[] = "GoogleDMToken token=%s";
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(kDMTokenHeaderFormat, dm_token.c_str()));
  resource_request->method = "POST";

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  url_loader->AttachStringForUpload(body, "application/octet-stream");
  url_loader->SetRetryOptions(
      kMaxRetryCount, network::SimpleURLLoader::RetryMode::RETRY_ON_5XX);
  url_loader->SetTimeoutDuration(timeouts::kKeyUploadTimeout);
  return url_loader;
}

void LogNetError(int net_error) {
  static constexpr char kNetErrorHistogram[] =
      "Enterprise.DeviceTrust.PublicKeyUpload.URLLoaderNetError";
  base::UmaHistogramSparse(kNetErrorHistogram, net_error);
}

}  // namespace

MojoKeyNetworkDelegate::MojoKeyNetworkDelegate(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : shared_url_loader_factory_(std::move(shared_url_loader_factory)) {
  DCHECK(shared_url_loader_factory_);
}

MojoKeyNetworkDelegate::~MojoKeyNetworkDelegate() = default;

void MojoKeyNetworkDelegate::SendPublicKeyToDmServer(
    const GURL& url,
    const std::string& dm_token,
    const std::string& body,
    UploadKeyCompletedCallback upload_key_completed_callback) {
  auto url_loader = CreateURLLoader(url, dm_token, body);
  auto* url_loader_ptr = url_loader.get();
  url_loader_ptr->DownloadHeadersOnly(
      shared_url_loader_factory_.get(),
      base::BindOnce(&MojoKeyNetworkDelegate::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), std::move(url_loader),
                     std::move(upload_key_completed_callback)));
}

void MojoKeyNetworkDelegate::OnURLLoaderComplete(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    UploadKeyCompletedCallback upload_key_completed_callback,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  HttpResponseCode response_code = headers ? headers->response_code() : 0;

  if (response_code == 0 && url_loader) {
    LogNetError(url_loader->NetError());
  }

  std::move(upload_key_completed_callback).Run(response_code);
}

}  // namespace enterprise_connectors
