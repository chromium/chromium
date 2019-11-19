// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
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
    : public chromeos::PlatformKeysService::SelectDelegate {
 public:
  DefaultSelectDelegate() {}
  ~DefaultSelectDelegate() override {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              const CertificateSelectedCallback& callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    CHECK(web_contents);
    const extensions::Extension* const extension =
        extensions::ExtensionRegistry::Get(context)->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::ENABLED);
    if (!extension) {
      callback.Run(nullptr /* no certificate selected */);
      return;
    }
    ShowPlatformKeysCertificateSelector(
        web_contents, extension->short_name(), certs,
        // Don't call |callback| once this delegate is destructed, thus use a
        // WeakPtr.
        base::Bind(&DefaultSelectDelegate::SelectedCertificate,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void SelectedCertificate(
      const CertificateSelectedCallback& callback,
      const scoped_refptr<net::X509Certificate>& selected_cert) {
    callback.Run(selected_cert);
  }

 private:
  base::WeakPtrFactory<DefaultSelectDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DefaultSelectDelegate);
};

}  // namespace

// static
PlatformKeysService* PlatformKeysServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PlatformKeysService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PlatformKeysServiceFactory* PlatformKeysServiceFactory::GetInstance() {
  return base::Singleton<PlatformKeysServiceFactory>::get();
}

PlatformKeysServiceFactory::PlatformKeysServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlatformKeysService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

PlatformKeysServiceFactory::~PlatformKeysServiceFactory() {
}

content::BrowserContext* PlatformKeysServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* PlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  extensions::StateStore* const store =
      extensions::ExtensionSystem::Get(context)->state_store();

  policy::ProfilePolicyConnector* const policy_connector =
      Profile::FromBrowserContext(context)->GetProfilePolicyConnector();

  Profile* const profile = Profile::FromBrowserContext(context);

  PlatformKeysService* const service = new PlatformKeysService(
      policy_connector->IsManaged(), profile->GetPrefs(),
      policy_connector->policy_service(), context, store);

  service->SetSelectDelegate(std::make_unique<DefaultSelectDelegate>());
  return service;
}

}  // namespace chromeos
