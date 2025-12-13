// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/cert_utils.h"

#include <utility>

#include "chrome/browser/enterprise/client_certificates/browser_context_delegate.h"
#include "chrome/browser/enterprise/client_certificates/cert_utils.h"
#include "chrome/common/chrome_version.h"
#include "components/enterprise/client_certificates/core/browser_cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/enterprise/client_certificates/core/ec_private_key_factory.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "components/enterprise/client_certificates/core/features.h"
#include "components/enterprise/client_certificates/core/win/windows_software_private_key_factory.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "components/enterprise/client_certificates/core/android_private_key_factory.h"
#include "components/enterprise/client_certificates/core/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace client_certificates {

namespace {

#if BUILDFLAG(IS_MAC)
constexpr char kUnexportableKeyKeychainAccessGroup[] =
    MAC_TEAM_IDENTIFIER_STRING "." MAC_BUNDLE_IDENTIFIER_STRING ".devicetrust";
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

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

#if BUILDFLAG(IS_WIN)
  if (features::AreWindowsSoftwareKeysEnabled()) {
    auto windows_software_key_factory =
        WindowsSoftwarePrivateKeyFactory::TryCreate();
    if (windows_software_key_factory) {
      sub_factories.insert_or_assign(PrivateKeySource::kOsSoftwareKey,
                                     std::move(windows_software_key_factory));
    }
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
  if (features::IsClientCertificateProvisioningOnAndroidEnabled()) {
    auto android_key_factory = AndroidPrivateKeyFactory::TryCreate();
    if (android_key_factory) {
      sub_factories.insert_or_assign(PrivateKeySource::kAndroidKey,
                                     std::move(android_key_factory));
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  sub_factories.insert_or_assign(PrivateKeySource::kSoftwareKey,
                                 std::make_unique<ECPrivateKeyFactory>());

  return PrivateKeyFactory::Create(std::move(sub_factories));
}

std::unique_ptr<client_certificates::CertificateProvisioningService>
CreateBrowserCertificateProvisioningService(
    PrefService* local_state,
    client_certificates::CertificateStore* certificate_store,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!url_loader_factory || !device_management_service || !certificate_store) {
    return nullptr;
  }

  return client_certificates::CertificateProvisioningService::Create(
      local_state, certificate_store,
      std::make_unique<client_certificates::BrowserContextDelegate>(),
      client_certificates::KeyUploadClient::Create(
          std::make_unique<
              enterprise_attestation::BrowserCloudManagementDelegate>(
              enterprise_attestation::DMServerClient::Create(
                  device_management_service, std::move(url_loader_factory)))));
}

}  // namespace client_certificates
