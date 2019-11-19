// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/constants/chromeos_switches.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {
// Values for the attestation server switch.
const char kAttestationServerDefault[] = "default";
const char kAttestationServerTest[] = "test";

// Endpoints for the default Google Privacy CA operations.
const char kDefaultEnrollRequestURL[] =
    "https://chromeos-ca.gstatic.com/enroll";
const char kDefaultCertificateRequestURL[] =
    "https://chromeos-ca.gstatic.com/sign";

// Endpoints for the test Google Privacy CA operations.
const char kTestEnrollRequestURL[] =
    "https://asbestos-qa.corp.google.com/enroll";
const char kTestCertificateRequestURL[] =
    "https://asbestos-qa.corp.google.com/sign";

const char kMimeContentType[] = "application/octet-stream";

}  // namespace

namespace chromeos {
namespace attestation {

static PrivacyCAType GetAttestationServerType() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          chromeos::switches::kAttestationServer);
  if (value.empty() || value == kAttestationServerDefault) {
    return DEFAULT_PCA;
  }
  if (value == kAttestationServerTest) {
    return TEST_PCA;
  }
  LOG(WARNING) << "Invalid attestation server value: " << value
               << ". Using default.";
  return DEFAULT_PCA;
}

AttestationCAClient::AttestationCAClient() {
  pca_type_ = GetAttestationServerType();
}

AttestationCAClient::~AttestationCAClient() {}

void AttestationCAClient::SendEnrollRequest(const std::string& request,
                                            const DataCallback& on_response) {
  FetchURL(
      GetType() == TEST_PCA ? kTestEnrollRequestURL : kDefaultEnrollRequestURL,
      request, on_response);
}

void AttestationCAClient::SendCertificateRequest(
    const std::string& request,
    const DataCallback& on_response) {
  FetchURL(GetType() == TEST_PCA ? kTestCertificateRequestURL
                                 : kDefaultCertificateRequestURL,
           request, on_response);
}

void AttestationCAClient::OnURLLoadComplete(
    std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    const DataCallback& callback,
    std::unique_ptr<std::string> response_body) {
  // Move the loader out of the active loaders list.
  std::unique_ptr<network::SimpleURLLoader> url_loader = std::move(*it);
  url_loaders_.erase(it);

  DCHECK(url_loader);

  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    int response_code = url_loader->ResponseInfo()->headers->response_code();

    if (response_code < 200 || response_code > 299) {
      LOG(ERROR) << "Attestation CA sent an HTTP error response: "
                 << response_code;
      callback.Run(false, "");
      return;
    }
  }

  if (!response_body) {
    int net_error = url_loader->NetError();
    LOG(ERROR) << "Attestation CA request failed, error: "
               << net::ErrorToString(net_error);
    callback.Run(false, "");
    return;
  }

  // Run the callback last because it may delete |this|.
  callback.Run(true, *response_body);
}

void AttestationCAClient::FetchURL(const std::string& url,
                                   const std::string& request,
                                   const DataCallback& on_response) {
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("attestation_ca_client", R"(
        semantics {
          sender: "Attestation CA client"
          description:
            "Sends requests to the Attestation CA as part of the remote "
            "attestation feature, such as enrolling for remote attestation or "
            "to obtain an attestation certificate."
          trigger:
            "Device enrollment, content protection or get an attestation "
            "certificate for a hardware-protected key."
          data:
            "The data from AttestationCertificateRequest or from "
            "AttestationEnrollmentRequest message from "
            "cryptohome/attestation.proto. Some of the important data being "
            "encrypted endorsement certificate, attestation identity public "
            "key, PCR0 and PCR1 TPM values."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "The device setting DeviceAttestationEnabled can disable the "
            "attestation requests and AttestationForContentProtectionEnabled "
            "can disable the attestation for content protection. But they "
            "cannot be controlled by a policy or through settings."
          policy_exception_justification: "Not implemented."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(url);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  url_loader->AttachStringForUpload(request, kMimeContentType);

  auto* raw_url_loader = url_loader.get();
  url_loaders_.push_back(std::move(url_loader));

  raw_url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      g_browser_process->shared_url_loader_factory().get(),
      base::BindOnce(&AttestationCAClient::OnURLLoadComplete,
                     base::Unretained(this), std::move(--url_loaders_.end()),
                     on_response));
}

PrivacyCAType AttestationCAClient::GetType() {
  return pca_type_;
}

}  // namespace attestation
}  // namespace chromeos
