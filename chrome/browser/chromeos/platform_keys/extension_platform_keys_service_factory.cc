// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/ui/platform_keys_certificate_selector_chromeos.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace {

// This delegate selects a certificate by showing the certificate selection
// dialog to the user.
class DefaultSelectDelegate
    : public chromeos::ExtensionPlatformKeysService::SelectDelegate {
 public:
  DefaultSelectDelegate() {}
  ~DefaultSelectDelegate() override {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              CertificateSelectedCallback callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    CHECK(web_contents);
    const extensions::Extension* const extension =
        extensions::ExtensionRegistry::Get(context)->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::ENABLED);
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

  DISALLOW_COPY_AND_ASSIGN(DefaultSelectDelegate);
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
  return base::Singleton<ExtensionPlatformKeysServiceFactory>::get();
}

ExtensionPlatformKeysServiceFactory::ExtensionPlatformKeysServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionPlatformKeysService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(chromeos::platform_keys::PlatformKeysServiceFactory::GetInstance());
  DependsOn(
      chromeos::platform_keys::KeyPermissionsServiceFactory::GetInstance());
}

ExtensionPlatformKeysServiceFactory::~ExtensionPlatformKeysServiceFactory() {}

content::BrowserContext*
ExtensionPlatformKeysServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* ExtensionPlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  ExtensionPlatformKeysService* const service =
      new ExtensionPlatformKeysService(context);

  service->SetSelectDelegate(std::make_unique<DefaultSelectDelegate>());
  return service;
}

}  // namespace chromeos
