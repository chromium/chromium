// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user.h"

namespace ash {
namespace cert_provisioning {

BASE_FEATURE(kCertProvisioningUseOnlyInvalidationsForTesting,
             "CertProvisioningUseOnlyInvalidationsForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {
std::optional<AccountId> GetAccountId(CertScope scope, Profile* profile) {
  switch (scope) {
    case CertScope::kDevice: {
      return EmptyAccountId();
    }
    case CertScope::kUser: {
      user_manager::User* user =
          ProfileHelper::Get()->GetUserByProfile(profile);
      if (!user) {
        return std::nullopt;
      }

      return user->GetAccountId();
    }
  }

  NOTREACHED_IN_MIGRATION();
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

bool IsValidKeyType(const std::string& key_type) {
  return key_type == "rsa" || key_type == "ec";
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
    case CertProvisioningWorkerState::kReadyForNextOperation:
      return "ReadyForNextOperation";
    case CertProvisioningWorkerState::kAuthorizeInstructionReceived:
      return "AuthorizeInstructionReceived";
    case CertProvisioningWorkerState::kProofOfPossessionInstructionReceived:
      return "ProofOfPossessionInstructionReceived";
    case CertProvisioningWorkerState::kImportCertificateInstructionReceived:
      return "ImportCertificateInstructionReceived";
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
                         KeyType key_type,
                         bool is_va_enabled,
                         base::TimeDelta renewal_period,
                         ProtocolVersion protocol_version)
    : profile_id(profile_id),
      name(std::move(name)),
      policy_version(std::move(policy_version)),
      key_type(key_type),
      is_va_enabled(is_va_enabled),
      renewal_period(renewal_period),
      protocol_version(protocol_version) {}

CertProfile::CertProfile(const CertProfile& other) = default;
CertProfile& CertProfile::operator=(const CertProfile&) = default;
CertProfile::CertProfile(CertProfile&& source) = default;
CertProfile& CertProfile::operator=(CertProfile&&) = default;
CertProfile::CertProfile() = default;
CertProfile::~CertProfile() = default;

std::optional<CertProfile> CertProfile::MakeFromValue(
    const base::Value::Dict& value) {
  static_assert(kVersion == 7, "This function should be updated");

  const std::string* id = value.FindString(kCertProfileIdKey);
  const std::string* name = value.FindString(kCertProfileNameKey);
  const std::string* policy_version =
      value.FindString(kCertProfilePolicyVersionKey);
  const std::string* key_type = value.FindString(kCertProfileKeyType);
  std::optional<bool> is_va_enabled =
      value.FindBool(kCertProfileIsVaEnabledKey);
  std::optional<int> renewal_period_sec =
      value.FindInt(kCertProfileRenewalPeroidSec);
  std::optional<int> protocol_version =
      value.FindInt(kCertProfileProtocolVersion);

  if (!id || !policy_version || !key_type) {
    return std::nullopt;
  }

  if (!IsValidKeyType(*key_type)) {
    LOG(ERROR) << "Unsupported key type received: " << *key_type;
    return std::nullopt;
  }

  CertProfile result;
  result.profile_id = *id;
  result.name = name ? *name : std::string();
  result.policy_version = *policy_version;
  result.is_va_enabled = is_va_enabled.value_or(true);
  result.renewal_period = base::Seconds(renewal_period_sec.value_or(0));

  std::optional<ProtocolVersion> parsed_protocol_version =
      ParseProtocolVersion(protocol_version);
  if (!parsed_protocol_version) {
    LOG(ERROR) << "Failed to parse ProtocolVersion "
               << (protocol_version.has_value()
                       ? base::NumberToString(*protocol_version)
                       : std::string());
    // If a protocol version is delivered which this client doesn't
    // understand, there's no point using it.
    return std::nullopt;
  }
  result.protocol_version = *parsed_protocol_version;

  if (*key_type == "rsa") {
    result.key_type = KeyType::kRsa;
  } else if (*key_type == "ec") {
    result.key_type = KeyType::kEc;
  }

  return result;
}

bool CertProfile::operator==(const CertProfile& other) const {
  static_assert(kVersion == 7, "This function should be updated");
  return ((profile_id == other.profile_id) && (name == other.name) &&
          (policy_version == other.policy_version) &&
          (is_va_enabled == other.is_va_enabled) &&
          (renewal_period == other.renewal_period) &&
          (protocol_version == other.protocol_version) &&
          (key_type == other.key_type));
}

bool CertProfile::operator!=(const CertProfile& other) const {
  return !(*this == other);
}

bool CertProfileComparator::operator()(const CertProfile& a,
                                       const CertProfile& b) const {
  static_assert(CertProfile::kVersion == 7, "This function should be updated");
  return ((a.profile_id < b.profile_id) || (a.name < b.name) ||
          (a.policy_version < b.policy_version) ||
          (a.is_va_enabled < b.is_va_enabled) ||
          (a.renewal_period < b.renewal_period) ||
          (a.protocol_version < b.protocol_version) ||
          (a.key_type < b.key_type));
}

//==============================================================================

std::optional<ProtocolVersion> ParseProtocolVersion(
    std::optional<int> protocol_version_value) {
  switch (protocol_version_value.value_or(
      base::strict_cast<int>(ProtocolVersion::kStatic))) {
    case base::strict_cast<int>(ProtocolVersion::kStatic):
      return ProtocolVersion::kStatic;
    case base::strict_cast<int>(ProtocolVersion::kDynamic):
      return ProtocolVersion::kDynamic;
    default:
      return std::nullopt;
  }
}

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

::attestation::VerifiedAccessFlow GetVaFlowType(CertScope scope) {
  switch (scope) {
    case CertScope::kUser:
      return ::attestation::ENTERPRISE_USER;
    case CertScope::kDevice:
      return ::attestation::ENTERPRISE_MACHINE;
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

std::string GenerateCertProvisioningId() {
  std::string result = base::UnguessableToken::Create().ToString();
  // Server-side stores the id and expects it to be <=32 characters long.
  CHECK_LE(result.size(), 32u);
  return result;
}

std::string MakeInvalidationListenerType(const std::string& cert_prov_id) {
  constexpr char kCertProvPrefix[] = "cert-";
  return base::StrCat({kCertProvPrefix, cert_prov_id});
}

bool ShouldOnlyUseInvalidations() {
  return base::FeatureList::IsEnabled(
      kCertProvisioningUseOnlyInvalidationsForTesting);
}

}  // namespace cert_provisioning
}  // namespace ash
