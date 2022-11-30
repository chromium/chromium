// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace cert_provisioning {

namespace {
absl::optional<AccountId> GetAccountId(CertScope scope, Profile* profile) {
  switch (scope) {
    case CertScope::kDevice: {
      return EmptyAccountId();
    }
    case CertScope::kUser: {
      user_manager::User* user =
          ProfileHelper::Get()->GetUserByProfile(profile);
      if (!user) {
        return absl::nullopt;
      }

      return user->GetAccountId();
    }
  }

  NOTREACHED();
}

// This function implements `DeleteVaKey()` and `DeleteVaKeysByPrefix()`, both
// of which call this function with a proper match behavior.
void DeleteVaKeysWithMatchBehavior(
    CertScope scope,
    Profile* profile,
    ::attestation::DeleteKeysRequest::MatchBehavior match_behavior,
    const std::string& label_match,
    DeleteVaKeyCallback callback) {
  auto account_id = GetAccountId(scope, profile);
  if (!account_id.has_value()) {
    std::move(callback).Run(false);
    return;
  }
  ::attestation::DeleteKeysRequest request;
  request.set_username(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id.value())
          .account_id());
  request.set_key_label_match(label_match);
  request.set_match_behavior(match_behavior);

  auto wrapped_callback = [](DeleteVaKeyCallback cb,
                             const ::attestation::DeleteKeysReply& reply) {
    std::move(cb).Run(reply.status() == ::attestation::STATUS_SUCCESS);
  };
  AttestationClient::Get()->DeleteKeys(
      request, base::BindOnce(wrapped_callback, std::move(callback)));
}

}  // namespace

std::string CertificateProvisioningWorkerStateToString(
    CertProvisioningWorkerState state) {
  switch (state) {
    case CertProvisioningWorkerState::kInitState:
      return "InitState";
    case CertProvisioningWorkerState::kKeypairGenerated:
      return "KeypairGenerated";
    case CertProvisioningWorkerState::kStartCsrResponseReceived:
      return "StartCsrResponseReceived";
    case CertProvisioningWorkerState::kVaChallengeFinished:
      return "VaChallengeFinished";
    case CertProvisioningWorkerState::kKeyRegistered:
      return "KeyRegistered";
    case CertProvisioningWorkerState::kKeypairMarked:
      return "KeypairMarked";
    case CertProvisioningWorkerState::kSignCsrFinished:
      return "SignCsrFinished";
    case CertProvisioningWorkerState::kFinishCsrResponseReceived:
      return "FinishCsrResponseReceived";
    case CertProvisioningWorkerState::kSucceeded:
      return "Succeeded";
    case CertProvisioningWorkerState::kInconsistentDataError:
      return "InconsistentDataError";
    case CertProvisioningWorkerState::kFailed:
      return "Failed";
    case CertProvisioningWorkerState::kCanceled:
      return "Canceled";
  }
}

bool IsFinalState(CertProvisioningWorkerState state) {
  switch (state) {
    case CertProvisioningWorkerState::kSucceeded:
    case CertProvisioningWorkerState::kInconsistentDataError:
    case CertProvisioningWorkerState::kFailed:
    case CertProvisioningWorkerState::kCanceled:
      return true;
    default:
      return false;
  }
}

//===================== CertProfile ============================================

CertProfile::CertProfile(CertProfileId profile_id,
                         std::string name,
                         std::string policy_version,
                         bool is_va_enabled,
                         base::TimeDelta renewal_period)
    : profile_id(profile_id),
      name(std::move(name)),
      policy_version(std::move(policy_version)),
      is_va_enabled(is_va_enabled),
      renewal_period(renewal_period) {}

CertProfile::CertProfile(const CertProfile& other) = default;

CertProfile::CertProfile() = default;
CertProfile::~CertProfile() = default;

absl::optional<CertProfile> CertProfile::MakeFromValue(
    const base::Value& value) {
  static_assert(kVersion == 5, "This function should be updated");

  const std::string* id = value.FindStringKey(kCertProfileIdKey);
  const std::string* name = value.FindStringKey(kCertProfileNameKey);
  const std::string* policy_version =
      value.FindStringKey(kCertProfilePolicyVersionKey);
  absl::optional<bool> is_va_enabled =
      value.FindBoolKey(kCertProfileIsVaEnabledKey);
  absl::optional<int> renewal_period_sec =
      value.FindIntKey(kCertProfileRenewalPeroidSec);

  if (!id || !policy_version) {
    return absl::nullopt;
  }

  CertProfile result;
  result.profile_id = *id;
  result.name = name ? *name : std::string();
  result.policy_version = *policy_version;
  result.is_va_enabled = is_va_enabled.value_or(true);
  result.renewal_period = base::Seconds(renewal_period_sec.value_or(0));

  return result;
}

bool CertProfile::operator==(const CertProfile& other) const {
  static_assert(kVersion == 5, "This function should be updated");
  return ((profile_id == other.profile_id) && (name == other.name) &&
          (policy_version == other.policy_version) &&
          (is_va_enabled == other.is_va_enabled) &&
          (renewal_period == other.renewal_period));
}

bool CertProfile::operator!=(const CertProfile& other) const {
  return !(*this == other);
}

bool CertProfileComparator::operator()(const CertProfile& a,
                                       const CertProfile& b) const {
  static_assert(CertProfile::kVersion == 5, "This function should be updated");
  return ((a.profile_id < b.profile_id) || (a.name < b.name) ||
          (a.policy_version < b.policy_version) ||
          (a.is_va_enabled < b.is_va_enabled) ||
          (a.renewal_period < b.renewal_period));
}

//==============================================================================

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRequiredClientCertificateForUser);
  registry->RegisterDictionaryPref(prefs::kCertificateProvisioningStateForUser);
}

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kRequiredClientCertificateForDevice);
  registry->RegisterDictionaryPref(
      prefs::kCertificateProvisioningStateForDevice);
}

const char* GetPrefNameForCertProfiles(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return prefs::kRequiredClientCertificateForUser;
    case CertScope::kDevice:
      return prefs::kRequiredClientCertificateForDevice;
  }
}

const char* GetPrefNameForSerialization(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return prefs::kCertificateProvisioningStateForUser;
    case CertScope::kDevice:
      return prefs::kCertificateProvisioningStateForDevice;
  }
}

std::string GetKeyName(CertProfileId profile_id) {
  return kKeyNamePrefix + profile_id;
}

attestation::AttestationKeyType GetVaKeyType(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return attestation::AttestationKeyType::KEY_USER;
    case CertScope::kDevice:
      return attestation::AttestationKeyType::KEY_DEVICE;
  }
}

chromeos::platform_keys::TokenId GetPlatformKeysTokenId(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return chromeos::platform_keys::TokenId::kUser;
    case CertScope::kDevice:
      return chromeos::platform_keys::TokenId::kSystem;
  }
}

void DeleteVaKey(CertScope scope,
                 Profile* profile,
                 const std::string& key_name,
                 DeleteVaKeyCallback callback) {
  DeleteVaKeysWithMatchBehavior(
      scope, profile, ::attestation::DeleteKeysRequest::MATCH_BEHAVIOR_EXACT,
      key_name, std::move(callback));
}

void DeleteVaKeysByPrefix(CertScope scope,
                          Profile* profile,
                          const std::string& key_prefix,
                          DeleteVaKeyCallback callback) {
  DeleteVaKeysWithMatchBehavior(
      scope, profile, ::attestation::DeleteKeysRequest::MATCH_BEHAVIOR_PREFIX,
      key_prefix, std::move(callback));
}

scoped_refptr<net::X509Certificate> CreateSingleCertificateFromBytes(
    const char* data,
    size_t length) {
  net::CertificateList cert_list =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_bytes(base::make_span(data, length)),
          net::X509Certificate::FORMAT_AUTO);

  if (cert_list.size() != 1) {
    return {};
  }

  return cert_list[0];
}

platform_keys::PlatformKeysService* GetPlatformKeysService(CertScope scope,
                                                           Profile* profile) {
  switch (scope) {
    case CertScope::kUser:
      return platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          profile);
    case CertScope::kDevice:
      return platform_keys::PlatformKeysServiceFactory::GetInstance()
          ->GetDeviceWideService();
  }
}

platform_keys::KeyPermissionsManager* GetKeyPermissionsManager(
    CertScope scope,
    Profile* profile) {
  switch (scope) {
    case CertScope::kUser:
      return platform_keys::KeyPermissionsManagerImpl::
          GetUserPrivateTokenKeyPermissionsManager(profile);
    case CertScope::kDevice:
      return platform_keys::KeyPermissionsManagerImpl::
          GetSystemTokenKeyPermissionsManager();
  }
}

}  // namespace cert_provisioning
}  // namespace ash
