// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/linux_key_network_delegate.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace enterprise_connectors {
namespace {

constexpr int kMaxRetryCount = 10;

}  // namespace

LinuxKeyNetworkDelegate::LinuxKeyNetworkDelegate(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_remote_url_loader_factory)
    : remote_url_loader_factory_(std::move(pending_remote_url_loader_factory)) {
  DCHECK(remote_url_loader_factory_);
}

LinuxKeyNetworkDelegate::~LinuxKeyNetworkDelegate() = default;

KeyNetworkDelegate::HttpResponseCode
LinuxKeyNetworkDelegate::SendPublicKeyToDmServerSync(
    const GURL& url,
    const std::string& dm_token,
    const std::string& body) {
  base::RunLoop run_loop;
  auto callback = base::BindOnce(&LinuxKeyNetworkDelegate::SetResponseCode,
                                 weak_factory_.GetWeakPtr())
                      .Then(run_loop.QuitClosure());
  StartRequest(std::move(callback), url, dm_token, body);
  run_loop.Run();

  // Parallel requests are not supported.
  url_loader_.reset();
  return response_code_;
}

void LinuxKeyNetworkDelegate::StartRequest(
    base::OnceCallback<void(int)> callback,
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
  std::string header_auth = "GoogleDMToken token=" + dm_token;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      header_auth);
  resource_request->method = "POST";

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(body, "application/octet-stream");
  url_loader_->SetRetryOptions(
      kMaxRetryCount, network::SimpleURLLoader::RetryMode::RETRY_ON_5XX);
  url_loader_->DownloadHeadersOnly(
      remote_url_loader_factory_.get(),
      base::BindOnce(&LinuxKeyNetworkDelegate::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LinuxKeyNetworkDelegate::OnURLLoaderComplete(
    base::OnceCallback<void(int)> callback,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  int response_code = headers ? headers->response_code() : 0;
  std::move(callback).Run(response_code);
}

void LinuxKeyNetworkDelegate::SetResponseCode(int response_code) {
  response_code_ = response_code;
}

}  // namespace enterprise_connectors
