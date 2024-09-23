// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace chromeos {

namespace {

namespace api_cp = extensions::api::certificate_provider;

class DefaultDelegate : public CertificateProviderService::Delegate,
                        public extensions::EventRouter::Observer,
                        public extensions::ExtensionRegistryObserver {
 public:
  // |event_router| may be null in tests.
  DefaultDelegate(CertificateProviderService* service,
                  extensions::ExtensionRegistry* registry,
                  extensions::EventRouter* event_router);
  DefaultDelegate(const DefaultDelegate&) = delete;
  DefaultDelegate& operator=(const DefaultDelegate&) = delete;
  ~DefaultDelegate() override;

  // CertificateProviderService::Delegate:
  std::vector<std::string> CertificateProviderExtensions() override;
  void BroadcastCertificateRequest(int request_id) override;
  bool DispatchSignRequestToExtension(
      const std::string& extension_id,
      int request_id,
      uint16_t algorithm,
      const scoped_refptr<net::X509Certificate>& certificate,
      base::span<const uint8_t> input) override;

  // extensions::EventRouter::Observer:
  void OnListenerAdded(const extensions::EventListenerInfo& details) override {}
  void OnListenerRemoved(const extensions::EventListenerInfo& details) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

 private:
  // Returns extension IDs that currently have event listeners for the given
  // event,
  base::flat_set<std::string> GetSubscribedExtensions(
      const std::string& event_name);

  const raw_ptr<CertificateProviderService> service_;
  const raw_ptr<extensions::ExtensionRegistry> registry_;
  const raw_ptr<extensions::EventRouter> event_router_;
};

// Constructs the "onCertificatesUpdateRequested" event.
std::unique_ptr<extensions::Event> BuildOnCertificatesUpdateRequestedEvent(
    int request_id) {
  api_cp::CertificatesUpdateRequest certificates_update_request;
  certificates_update_request.certificates_request_id = request_id;
  base::Value::List event_args;
  event_args.Append(certificates_update_request.ToValue());
  return std::make_unique<extensions::Event>(
      extensions::events::CERTIFICATEPROVIDER_ON_CERTIFICATES_UPDATE_REQUESTED,
      api_cp::OnCertificatesUpdateRequested::kEventName, std::move(event_args));
}

// Constructs the legacy "onCertificatesRequested" event.
std::unique_ptr<extensions::Event> BuildOnCertificatesRequestedEvent(
    int request_id) {
  base::Value::List event_args;
  event_args.Append(request_id);
  return std::make_unique<extensions::Event>(
      extensions::events::CERTIFICATEPROVIDER_ON_CERTIFICATES_REQUESTED,
      api_cp::OnCertificatesRequested::kEventName, std::move(event_args));
}

// Constructs the "onSignatureRequested" event.
std::unique_ptr<extensions::Event> BuildOnSignatureRequestedEvent(
    int request_id,
    uint16_t algorithm,
    const net::X509Certificate& certificate,
    base::span<const uint8_t> input) {
  api_cp::SignatureRequest request;
  request.sign_request_id = request_id;
  switch (algorithm) {
    case SSL_SIGN_RSA_PKCS1_SHA1:
      request.algorithm = api_cp::Algorithm::kRsassaPkcs1V1_5Sha1;
      break;
    case SSL_SIGN_RSA_PKCS1_SHA256:
      request.algorithm = api_cp::Algorithm::kRsassaPkcs1V1_5Sha256;
      break;
    case SSL_SIGN_RSA_PKCS1_SHA384:
      request.algorithm = api_cp::Algorithm::kRsassaPkcs1V1_5Sha384;
      break;
    case SSL_SIGN_RSA_PKCS1_SHA512:
      request.algorithm = api_cp::Algorithm::kRsassaPkcs1V1_5Sha512;
      break;
    case SSL_SIGN_RSA_PSS_RSAE_SHA256:
      request.algorithm = api_cp::Algorithm::kRsassaPssSha256;
      break;
    case SSL_SIGN_RSA_PSS_RSAE_SHA384:
      request.algorithm = api_cp::Algorithm::kRsassaPssSha384;
      break;
    case SSL_SIGN_RSA_PSS_RSAE_SHA512:
      request.algorithm = api_cp::Algorithm::kRsassaPssSha512;
      break;
    default:
      LOG(ERROR) << "Unknown signature algorithm";
      return nullptr;
  }
  request.input.assign(input.begin(), input.end());
  std::string_view cert_der =
      net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer());
  request.certificate.assign(cert_der.begin(), cert_der.end());

  base::Value::List event_args;
  event_args.Append(request.ToValue());

  return std::make_unique<extensions::Event>(
      extensions::events::CERTIFICATEPROVIDER_ON_SIGNATURE_REQUESTED,
      api_cp::OnSignatureRequested::kEventName, std::move(event_args));
}

// Constructs the legacy "onSignDigestRequested" event.
std::unique_ptr<extensions::Event> BuildOnSignDigestRequestedEvent(
    int request_id,
    uint16_t algorithm,
    const net::X509Certificate& certificate,
    base::span<const uint8_t> input) {
  api_cp::SignRequest request;

  request.sign_request_id = request_id;
  switch (algorithm) {
    case SSL_SIGN_RSA_PKCS1_SHA1:
      request.hash = api_cp::Hash::kSha1;
      break;
    case SSL_SIGN_RSA_PKCS1_SHA256:
      request.hash = api_cp::Hash::kSha256;
      break;
    case SSL_SIGN_RSA_PKCS1_SHA384:
      request.hash = api_cp::Hash::kSha384;
      break;
    case SSL_SIGN_RSA_PKCS1_SHA512:
      request.hash = api_cp::Hash::kSha512;
      break;
    default:
      LOG(ERROR) << "Unknown signature algorithm";
      return nullptr;
  }
  std::string_view cert_der =
      net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer());
  request.certificate.assign(cert_der.begin(), cert_der.end());

  // The extension expects the input to be hashed ahead of time.
  request.digest.resize(EVP_MAX_MD_SIZE);
  const EVP_MD* md = SSL_get_signature_algorithm_digest(algorithm);
  unsigned digest_len;
  if (!md || !EVP_Digest(input.data(), input.size(), request.digest.data(),
                         &digest_len, md, /*ENGINE *impl=*/nullptr)) {
    return nullptr;
  }
  request.digest.resize(digest_len);

  base::Value::List event_args;
  event_args.Append(request_id);
  event_args.Append(request.ToValue());

  return std::make_unique<extensions::Event>(
      extensions::events::CERTIFICATEPROVIDER_ON_SIGN_DIGEST_REQUESTED,
      api_cp::OnSignDigestRequested::kEventName, std::move(event_args));
}

DefaultDelegate::DefaultDelegate(CertificateProviderService* service,
                                 extensions::ExtensionRegistry* registry,
                                 extensions::EventRouter* event_router)
    : service_(service), registry_(registry), event_router_(event_router) {
  DCHECK(service_);
  registry_->AddObserver(this);
  event_router_->RegisterObserver(
      this, api_cp::OnCertificatesUpdateRequested::kEventName);
  event_router_->RegisterObserver(this,
                                  api_cp::OnCertificatesRequested::kEventName);
}

DefaultDelegate::~DefaultDelegate() {
  event_router_->UnregisterObserver(this);
  registry_->RemoveObserver(this);
}

std::vector<std::string> DefaultDelegate::CertificateProviderExtensions() {
  base::flat_set<std::string> ids = GetSubscribedExtensions(
      api_cp::OnCertificatesUpdateRequested::kEventName);
  const base::flat_set<std::string> legacy_ids =
      GetSubscribedExtensions(api_cp::OnCertificatesRequested::kEventName);
  ids.insert(legacy_ids.begin(), legacy_ids.end());
  return std::vector<std::string>(ids.begin(), ids.end());
}

void DefaultDelegate::BroadcastCertificateRequest(int request_id) {
  // First, broadcast the event to the extensions that use the up-to-date
  // version of the API.
  const auto up_to_date_api_extension_ids = GetSubscribedExtensions(
      api_cp::OnCertificatesUpdateRequested::kEventName);
  for (const std::string& extension_id : up_to_date_api_extension_ids) {
    event_router_->DispatchEventToExtension(
        extension_id, BuildOnCertificatesUpdateRequestedEvent(request_id));
  }
  // Second, broadcast the event to the extensions that only listen for the
  // legacy event.
  for (const std::string& extension_id :
       GetSubscribedExtensions(api_cp::OnCertificatesRequested::kEventName)) {
    if (up_to_date_api_extension_ids.contains(extension_id))
      continue;
    event_router_->DispatchEventToExtension(
        extension_id, BuildOnCertificatesRequestedEvent(request_id));
  }
}

bool DefaultDelegate::DispatchSignRequestToExtension(
    const std::string& extension_id,
    int request_id,
    uint16_t algorithm,
    const scoped_refptr<net::X509Certificate>& certificate,
    base::span<const uint8_t> input) {
  DCHECK(certificate);
  std::unique_ptr<extensions::Event> event;
  // Send the up-to-date version of the event, and fall back to the legacy event
  // if the extension is only listening for that one.
  if (event_router_->ExtensionHasEventListener(
          extension_id, api_cp::OnSignatureRequested::kEventName)) {
    event = BuildOnSignatureRequestedEvent(request_id, algorithm, *certificate,
                                           input);
  } else if (event_router_->ExtensionHasEventListener(
                 extension_id, api_cp::OnSignDigestRequested::kEventName)) {
    event = BuildOnSignDigestRequestedEvent(request_id, algorithm, *certificate,
                                            input);
  }
  if (!event)
    return false;
  event_router_->DispatchEventToExtension(extension_id, std::move(event));
  return true;
}

void DefaultDelegate::OnListenerRemoved(
    const extensions::EventListenerInfo& details) {
  if (!event_router_->ExtensionHasEventListener(
          details.extension_id,
          api_cp::OnCertificatesUpdateRequested::kEventName) &&
      !event_router_->ExtensionHasEventListener(
          details.extension_id, api_cp::OnCertificatesRequested::kEventName)) {
    service_->OnExtensionUnregistered(details.extension_id);
  }
}

void DefaultDelegate::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  service_->OnExtensionUnloaded(extension->id());
}

base::flat_set<std::string> DefaultDelegate::GetSubscribedExtensions(
    const std::string& event_name) {
  std::vector<std::string> ids;
  for (const std::unique_ptr<extensions::EventListener>& listener :
       event_router_->listeners().GetEventListenersByName(event_name)) {
    ids.push_back(listener->extension_id());
  }
  return base::flat_set<std::string>(ids.begin(), ids.end());
}

}  // namespace

// static
CertificateProviderService*
CertificateProviderServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CertificateProviderService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
CertificateProviderServiceFactory*
CertificateProviderServiceFactory::GetInstance() {
  static base::NoDestructor<CertificateProviderServiceFactory> instance;
  return instance.get();
}

CertificateProviderServiceFactory::CertificateProviderServiceFactory()
    : ProfileKeyedServiceFactory(
          "CertificateProviderService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::EventRouterFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
}

bool CertificateProviderServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* CertificateProviderServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  CertificateProviderService* const service = new CertificateProviderService();
  service->SetDelegate(std::make_unique<DefaultDelegate>(
      service,
      extensions::ExtensionRegistryFactory::GetForBrowserContext(context),
      extensions::EventRouterFactory::GetForBrowserContext(context)));
  return service;
}

}  // namespace chromeos
