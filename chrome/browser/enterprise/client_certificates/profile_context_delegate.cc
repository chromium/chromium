// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/profile_context_delegate.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "content/public/browser/storage_partition.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace client_certificates {

namespace {

constexpr char kLoggingContext[] = "Profile";

}  // namespace

ProfileContextDelegate::ProfileContextDelegate(
    Profile* profile,
    enterprise::ProfileIdService* profile_id_service)
    : profile_(profile), profile_id_service_(profile_id_service) {
  CHECK(profile_);
  CHECK(profile_id_service_);
}

ProfileContextDelegate::~ProfileContextDelegate() = default;

void ProfileContextDelegate::OnClientCertificateDeleted(
    scoped_refptr<net::X509Certificate> certificate) {
  profile_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* storage_partition) {
        storage_partition->GetNetworkContext()->FlushMatchingCachedClientCert(
            certificate);
      });
}

std::string ProfileContextDelegate::GetExpectedCommonName() const {
  if (std::optional<std::string> computer_name =
          GetConfiguredComputerName(profile_->GetPrefs())) {
    return *computer_name;
  }
  return profile_id_service_->GetProfileId().value_or(std::string());
}

std::string ProfileContextDelegate::GetIdentityName() {
  return kManagedProfileIdentityName;
}

std::string ProfileContextDelegate::GetTemporaryIdentityName() {
  return kTemporaryManagedProfileIdentityName;
}

std::string ProfileContextDelegate::GetPolicyPref() {
  return prefs::kProvisionManagedClientCertificateForUserPrefs;
}

std::string ProfileContextDelegate::GetConfigPolicyPref() const {
  return prefs::kManagedClientCertificateForUserConfigPrefs;
}

std::string ProfileContextDelegate::GetLoggingContext() {
  return kLoggingContext;
}

}  // namespace client_certificates
