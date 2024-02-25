// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/attestation_ca_client.h"

#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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

namespace ash {
namespace attestation {

namespace {

class CAProxyLookupClient : public network::mojom::ProxyLookupClient {
 public:
  // Not copyable nor movable.
  CAProxyLookupClient(const CAProxyLookupClient&) = delete;
  CAProxyLookupClient& operator=(const CAProxyLookupClient&) = delete;
  CAProxyLookupClient(CAProxyLookupClient&&) = delete;
  CAProxyLookupClient& operator=(CAProxyLookupClient&&) = delete;

  static void LookUpProxyForURL(
      network::mojom::NetworkContext* network_context,
      const GURL& url,
      AttestationCAClient::ProxyPresenceCallback callback) {
    // The created object will be deleted in `OnProxyLookupComplete()`.
    CAProxyLookupClient* client =
        new CAProxyLookupClient(network_context, url, std::move(callback));
    client->Start(network_context, url);
  }

  // network::mojom::ProxyLookupClient:
  void OnProxyLookupComplete(
      int32_t net_error,
      const std::optional<net::ProxyInfo>& proxy_info) override {
    LOG_IF(WARNING, !proxy_info.has_value())
        << " Error determining the proxy information: " << net_error;
    // Assume there is a proxy if failing to get proxy information.
    const bool has_proxy = !proxy_info.has_value() || !proxy_info->is_direct();
    receiver_.reset();
    std::move(callback_).Run(has_proxy);
    delete this;
  }

 private:
  CAProxyLookupClient(network::mojom::NetworkContext* network_context,
                      const GURL& url,
                      AttestationCAClient::ProxyPresenceCallback callback)
      : callback_(std::move(callback)) {
    CHECK(network_context);
  }
  void Start(network::mojom::NetworkContext* network_context, const GURL& url) {
    const net::NetworkAnonymizationKey network_anonymization_key =
        net::NetworkAnonymizationKey::CreateTransient();
    mojo::PendingRemote<network::mojom::ProxyLookupClient> proxy_lookup_client =
        receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&CAProxyLookupClient::OnProxyLookupComplete,
                       base::Unretained(this), net::ERR_ABORTED, std::nullopt));

    network_context->LookUpProxyForURL(url, network_anonymization_key,
                                       std::move(proxy_lookup_client));
  }

  AttestationCAClient::ProxyPresenceCallback callback_;

  mojo::Receiver<network::mojom::ProxyLookupClient> receiver_{this};
};

}  // namespace

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
                                            DataCallback on_response) {
  FetchURL(
      GetType() == TEST_PCA ? kTestEnrollRequestURL : kDefaultEnrollRequestURL,
      request, std::move(on_response));
}

void AttestationCAClient::SendCertificateRequest(const std::string& request,
                                                 DataCallback on_response) {
  FetchURL(GetType() == TEST_PCA ? kTestCertificateRequestURL
                                 : kDefaultCertificateRequestURL,
           request, std::move(on_response));
}

void AttestationCAClient::OnURLLoadComplete(
    std::list<std::unique_ptr<network::SimpleURLLoader>>::iterator it,
    DataCallback callback,
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
      std::move(callback).Run(false, "");
      return;
    }
  }

  if (!response_body) {
    int net_error = url_loader->NetError();
    LOG(ERROR) << "Attestation CA request failed, error: "
               << net::ErrorToString(net_error);
    std::move(callback).Run(false, "");
    return;
  }

  // Run the callback last because it may delete |this|.
  std::move(callback).Run(true, *response_body);
}

void AttestationCAClient::FetchURL(const std::string& url,
                                   const std::string& request,
                                   DataCallback on_response) {
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
            "The device setting AttestationForContentProtectionEnabled "
            "can disable the attestation for content protection."
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
                     std::move(on_response)));
}

PrivacyCAType AttestationCAClient::GetType() {
  return pca_type_;
}

void AttestationCAClient::CheckIfAnyProxyPresent(
    ProxyPresenceCallback callback) {
  GURL url(GetType() == TEST_PCA ? kTestEnrollRequestURL
                                 : kDefaultEnrollRequestURL);
  DCHECK(url.is_valid());

  network::mojom::NetworkContext* network_context = nullptr;
  // Uses the injected network context if present.
  if (network_context_for_testing_ != nullptr) {
    network_context = network_context_for_testing_;
  } else if (!g_browser_process ||
             !g_browser_process->system_network_context_manager() ||
             !g_browser_process->system_network_context_manager()
                  ->GetContext()) {
    LOG(DFATAL) << "No valid system network context.";
    std::move(callback).Run(/*is_any_proxy_present=*/true);
    return;
  } else {
    network_context =
        g_browser_process->system_network_context_manager()->GetContext();
  }

  CAProxyLookupClient::LookUpProxyForURL(
      network_context, url.DeprecatedGetOriginAsURL(), std::move(callback));
}

}  // namespace attestation
}  // namespace ash
