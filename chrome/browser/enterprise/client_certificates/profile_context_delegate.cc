// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/client_certificates/profile_context_delegate.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "components/policy/core/common/policy_logger.h"
#include "content/public/browser/storage_partition.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace client_certificates {

namespace {

constexpr char kLoggingContext[] = "Profile";

std::string_view UserPermissionToString(
    device_signals::UserPermission permission) {
  switch (permission) {
    case device_signals::UserPermission::kUnaffiliated:
      return "Unaffiliated";
    case device_signals::UserPermission::kMissingConsent:
      return "MissingConsent";
    case device_signals::UserPermission::kConsumerUser:
      return "ConsumerUser";
    case device_signals::UserPermission::kUnknownUser:
      return "UnknownUser";
    case device_signals::UserPermission::kMissingUser:
      return "MissingUser";
    case device_signals::UserPermission::kGranted:
      return "Granted";
    case device_signals::UserPermission::kUnsupported:
      return "Unsupported";
  }
}

}  // namespace

ProfileContextDelegate::ProfileContextDelegate(
    Profile* profile,
    enterprise::ProfileIdService* profile_id_service,
    device_signals::UserPermissionService* user_permission_service)
    : profile_(profile),
      profile_id_service_(profile_id_service),
      user_permission_service_(user_permission_service) {
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
  const PrefService* prefs = profile_->GetPrefs();
  if (IsComputerNameConfigured(prefs) && CanUseComputerName()) {
    if (std::optional<std::string> computer_name =
            GetConfiguredComputerName(prefs)) {
      return *computer_name;
    }
  }
  return profile_id_service_->GetProfileId().value_or(std::string());
}

bool ProfileContextDelegate::CanUseComputerName() const {
  if (!user_permission_service_) {
    if (!std::exchange(has_recorded_computer_name_permission_, true)) {
      VLOG_POLICY(1, DEVICE_TRUST)
          << "[" << kLoggingContext
          << "] Cannot use computer_name for certificate Common Name "
             "(UserPermissionService is null); falling back to profile ID.";
    }
    return false;
  }

  const device_signals::UserPermission permission =
      user_permission_service_->CanCollectUserScopedSignals();
  if (!std::exchange(has_recorded_computer_name_permission_, true)) {
    base::UmaHistogramEnumeration(
        "Enterprise.ClientCertificate.Profile.ComputerNamePermission",
        permission);
    if (permission != device_signals::UserPermission::kGranted) {
      LOG_POLICY(INFO, DEVICE_TRUST)
          << "[" << kLoggingContext
          << "] Cannot use computer_name for certificate Common Name "
             "(UserPermission="
          << UserPermissionToString(permission)
          << "); falling back to profile ID.";
    }
  }
  return permission == device_signals::UserPermission::kGranted;
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
