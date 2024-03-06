// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/certificate_store_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_version.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/ec_private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/unexportable_key.h"

namespace client_certificates {

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kUnexportableKeyKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING ".devicetrust";
#endif  // BUILDFLAG(IS_MAC)

std::unique_ptr<PrivateKeyFactory> CreatePrivateKeyFactory() {
  PrivateKeyFactory::PrivateKeyFactoriesMap sub_factories;
  crypto::UnexportableKeyProvider::Config config;
#if BUILDFLAG(IS_MAC)
  config.keychain_access_group = kUnexportableKeyKeychainAccessGroup;
#endif  // BUILDFLAG(IS_MAC)
  auto unexportable_key_factory =
      UnexportablePrivateKeyFactory::TryCreate(std::move(config));
  if (unexportable_key_factory) {
    sub_factories.insert_or_assign(PrivateKeySource::kUnexportableKey,
                                   std::move(unexportable_key_factory));
  }
  sub_factories.insert_or_assign(PrivateKeySource::kSoftwareKey,
                                 std::make_unique<ECPrivateKeyFactory>());

  return PrivateKeyFactory::Create(std::move(sub_factories));
}

}  // namespace

// static
CertificateStoreFactory* CertificateStoreFactory::GetInstance() {
  static base::NoDestructor<CertificateStoreFactory> instance;
  return instance.get();
}

// static
CertificateStore* CertificateStoreFactory::GetForProfile(Profile* profile) {
  return static_cast<CertificateStore*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CertificateStoreFactory::CertificateStoreFactory()
    : ProfileKeyedServiceFactory("CertificateStore",
                                 ProfileSelections::BuildForRegularProfile()) {}

CertificateStoreFactory::~CertificateStoreFactory() = default;

std::unique_ptr<KeyedService>
CertificateStoreFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  if (!profile || !profile->GetDefaultStoragePartition() ||
      !profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider()) {
    return nullptr;
  }

  return CertificateStore::Create(
      profile->GetPath(),
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider(),
      CreatePrivateKeyFactory());
}

}  // namespace client_certificates
