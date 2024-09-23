// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/ui/platform_keys_certificate_selector_chromeos.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/cert/x509_certificate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#endif

namespace chromeos {
namespace {

// This delegate selects a certificate by showing the certificate selection
// dialog to the user.
class DefaultSelectDelegate
    : public chromeos::ExtensionPlatformKeysService::SelectDelegate {
 public:
  DefaultSelectDelegate() {}
  DefaultSelectDelegate(const DefaultSelectDelegate&) = delete;
  auto operator=(const DefaultSelectDelegate&) = delete;
  ~DefaultSelectDelegate() override {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              CertificateSelectedCallback callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    CHECK(web_contents);
    const extensions::Extension* const extension =
        extensions::ExtensionRegistry::Get(context)
            ->enabled_extensions()
            .GetByID(extension_id);
    if (!extension) {
      std::move(callback).Run(nullptr /* no certificate selected */);
      return;
    }
    ShowPlatformKeysCertificateSelector(
        web_contents, extension->short_name(), certs,
        // Don't call |callback| once this delegate is destructed, thus use a
        // WeakPtr.
        base::BindOnce(&DefaultSelectDelegate::SelectedCertificate,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SelectedCertificate(
      CertificateSelectedCallback callback,
      const scoped_refptr<net::X509Certificate>& selected_cert) {
    std::move(callback).Run(selected_cert);
  }

 private:
  base::WeakPtrFactory<DefaultSelectDelegate> weak_factory_{this};
};

}  // namespace

// static
ExtensionPlatformKeysService*
ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionPlatformKeysService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionPlatformKeysServiceFactory*
ExtensionPlatformKeysServiceFactory::GetInstance() {
  static base::NoDestructor<ExtensionPlatformKeysServiceFactory> instance;
  return instance.get();
}

ExtensionPlatformKeysServiceFactory::ExtensionPlatformKeysServiceFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionPlatformKeysService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DependsOn(crosapi::KeystoreServiceFactoryAsh::GetInstance());
#endif
}

ExtensionPlatformKeysServiceFactory::~ExtensionPlatformKeysServiceFactory() =
    default;

KeyedService* ExtensionPlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  ExtensionPlatformKeysService* const service =
      new ExtensionPlatformKeysService(context);

  service->SetSelectDelegate(std::make_unique<DefaultSelectDelegate>());
  return service;
}

}  // namespace chromeos
