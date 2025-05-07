// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/certificate_store_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/client_certificates/cert_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/client_certificates/core/features.h"
#include "components/enterprise/client_certificates/core/leveldb_certificate_store.h"
#include "components/enterprise/client_certificates/core/prefs_certificate_store.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace client_certificates {

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
  if (!profile) {
    return nullptr;
  }

  if (features::IsManagedUserClientCertificateInPrefsEnabled()) {
    return std::make_unique<PrefsCertificateStore>(profile->GetPrefs(),
                                                   CreatePrivateKeyFactory());
  }

  if (!profile->GetDefaultStoragePartition() ||
      !profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider()) {
    return nullptr;
  }

  return LevelDbCertificateStore::Create(
      profile->GetPath(),
      profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider(),
      CreatePrivateKeyFactory());
}

}  // namespace client_certificates
