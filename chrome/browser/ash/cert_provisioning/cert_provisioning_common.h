// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_

#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/cert/x509_certificate.h"

class PrefRegistrySimple;
class Profile;

namespace attestation {
enum VerifiedAccessFlow : int;
}  // namespace attestation

namespace ash {

namespace platform_keys {
class KeyPermissionsManager;
class PlatformKeysService;
}  // namespace platform_keys

namespace cert_provisioning {

// A feature to prevent Certificate Provisioning workers from attempting to
// continue the provisioning process on timeout (without receiving an
// invalidation). It is intended to be used for testing only to verify that new
// invalidations actually work. Also see `ShouldOnlyUseInvalidations`.
// TODO(b/336989561): Remove this after the migration to new invalidations is
// done.
BASE_DECLARE_FEATURE(kCertProvisioningUseOnlyInvalidationsForTesting);

// Used for both DeleteVaKey and DeleteVaKeysByPrefix
using DeleteVaKeyCallback = base::OnceCallback<void(bool)>;

const char kKeyNamePrefix[] = "cert-provis-";

// The type for variables containing an error from DM Server response.
using CertProvisioningResponseErrorType =
    enterprise_management::ClientCertificateProvisioningResponse::Error;
// The namespace that contains convenient aliases for error values, e.g.
// UNDEFINED, TIMED_OUT, IDENTITY_VERIFICATION_ERROR, CA_ERROR.
using CertProvisioningResponseError =
    enterprise_management::ClientCertificateProvisioningResponse;

// Numeric values are used in serialization and should not be remapped.
enum class CertScope { kUser = 0, kDevice = 1, kMaxValue = kDevice };

// These values are used in serialization and should be changed carefully. Also
// enums.xml should be updated.
enum class CertProvisioningWorkerState {
  kInitState = 0,
  kKeypairGenerated = 1,
  kStartCsrResponseReceived = 2,  // Unused in "dynamic" flow.
  kVaChallengeFinished = 3,
  kKeyRegistered = 4,
  kKeypairMarked = 5,
  kSignCsrFinished = 6,
  kFinishCsrResponseReceived = 7,  // Unused in "dynamic" flow.
  kSucceeded = 8,
  kInconsistentDataError = 9,
  kFailed = 10,
  kCanceled = 11,

  // The following states are only used in the "dynamic" flow.
  // The worker is ready for next server-provided operation.
  kReadyForNextOperation = 12,
  // The worker has received an "Authorize" instruction.
  kAuthorizeInstructionReceived = 13,
  // The worker has received a "Proof of Possession" instruction.
  kProofOfPossessionInstructionReceived = 14,
  // The worker has received an "Import Certificate" instruction.
  kImportCertificateInstructionReceived = 15,

  kMaxValue = kImportCertificateInstructionReceived,
};

// All states that are allowed in a "static" flow.
inline constexpr base::EnumSet<CertProvisioningWorkerState,
                               CertProvisioningWorkerState::kInitState,
                               CertProvisioningWorkerState::kMaxValue>
    kStaticWorkerStates = {
        CertProvisioningWorkerState::kInitState,
        CertProvisioningWorkerState::kKeypairGenerated,
        CertProvisioningWorkerState::kStartCsrResponseReceived,
        CertProvisioningWorkerState::kVaChallengeFinished,
        CertProvisioningWorkerState::kKeyRegistered,
        CertProvisioningWorkerState::kKeypairMarked,
        CertProvisioningWorkerState::kSignCsrFinished,
        CertProvisioningWorkerState::kFinishCsrResponseReceived,
        CertProvisioningWorkerState::kSucceeded,
        CertProvisioningWorkerState::kInconsistentDataError,
        CertProvisioningWorkerState::kFailed,
        CertProvisioningWorkerState::kCanceled};

// All states that are allowed in a "dynamic" flow.
inline constexpr base::EnumSet<CertProvisioningWorkerState,
                               CertProvisioningWorkerState::kInitState,
                               CertProvisioningWorkerState::kMaxValue>
    kDynamicWorkerStates = {
        CertProvisioningWorkerState::kInitState,
        CertProvisioningWorkerState::kKeypairGenerated,
        CertProvisioningWorkerState::kVaChallengeFinished,
        CertProvisioningWorkerState::kKeyRegistered,
        CertProvisioningWorkerState::kKeypairMarked,
        CertProvisioningWorkerState::kSignCsrFinished,
        CertProvisioningWorkerState::kSucceeded,
        CertProvisioningWorkerState::kInconsistentDataError,
        CertProvisioningWorkerState::kFailed,
        CertProvisioningWorkerState::kCanceled,
        CertProvisioningWorkerState::kReadyForNextOperation,
        CertProvisioningWorkerState::kAuthorizeInstructionReceived,
        CertProvisioningWorkerState::kProofOfPossessionInstructionReceived,
        CertProvisioningWorkerState::kImportCertificateInstructionReceived};

// Location where a generated key has been persisted by a "dynamic" flow worker.
// These values are used in serialization and should be changed carefully.
enum class KeyLocation {
  kNone = 0,
  kVaDatabase = 1,
  kPkcs11Token = 2,
  kMaxValue = kPkcs11Token
};

// Types of the requests sent from the certificate provisioning client to the
// device management server.
enum class DeviceManagementServerRequestType {
  kStartCsr = 0,
  kFinishCsr = 1,
  kDownloadCert = 2,
};

// Converts the worker |state| to a string. This is mainly for logging purposes.
std::string CertificateProvisioningWorkerStateToString(
    CertProvisioningWorkerState state);

// Returns true if the |state| is one of final states, i. e. worker should
// finish its task in one of them.
bool IsFinalState(CertProvisioningWorkerState state);

using CertProfileId = std::string;

// Names of CertProfile fields in a base::Value representation. Must be in sync
// with policy schema definitions in RequiredClientCertificateForDevice.yaml and
// RequiredClientCertificateForUser.yaml.
const char kCertProfileIdKey[] = "cert_profile_id";
const char kCertProfileNameKey[] = "name";
const char kCertProfileRenewalPeroidSec[] = "renewal_period_seconds";
const char kCertProfilePolicyVersionKey[] = "policy_version";
const char kCertProfileProtocolVersion[] = "protocol_version";
const char kCertProfileIsVaEnabledKey[] = "enable_remote_attestation_check";
const char kCertProfileKeyType[] = "key_algorithm";

// The version of the certificate provisioning protocol between ChromeOS client
// and device management server.
// The values must match the description in
// RequiredClientCertificateForDevice.yaml and
// RequiredClientCertificateForUser.yaml.
// They are also used in serialization so they should not be renumbered.
enum class ProtocolVersion {
  // Original "static" protocol.
  kStatic = 1,
  // "Dynamic" protocol.
  kDynamic = 2,
};

// The type of key the device should generate.
// TODO(b/364893005): After the client-side implementation is done, update the
// values in YAML files and mention those files here (same as for
// ProtocolVersion above). They are also used in serialization so they should
// not be renumbered.
enum class KeyType {
  // 2048-bit RSA keys.
  kRsa = 1,
  // Elliptic-curve keys using the P-256 curve.
  kEc = 2,
  kMaxValue = KeyType::kEc
};

struct CertProfile {
  static std::optional<CertProfile> MakeFromValue(
      const base::Value::Dict& value);

  CertProfile();
  // For tests.
  CertProfile(CertProfileId profile_id,
              std::string name,
              std::string policy_version,
              KeyType key_type,
              bool is_va_enabled,
              base::TimeDelta renewal_period,
              ProtocolVersion protocol_version);
  CertProfile(const CertProfile& other);
  CertProfile& operator=(const CertProfile&);
  CertProfile(CertProfile&& source);
  CertProfile& operator=(CertProfile&&);
  ~CertProfile();

  CertProfileId profile_id;
  // Human-readable name (UTF-8).
  std::string name;
  std::string policy_version;
  KeyType key_type;
  bool is_va_enabled = true;
  // Default renewal period 0 means that a certificate will be renewed only
  // after the previous one has expired (0 seconds before it is expires).
  base::TimeDelta renewal_period = base::Seconds(0);
  ProtocolVersion protocol_version = ProtocolVersion::kStatic;

  // IMPORTANT:
  // Increment this when you add/change any member in CertProfile (and update
  // all functions that fail to compile because of it).
  static constexpr int kVersion = 7;

  bool operator==(const CertProfile& other) const;
  bool operator!=(const CertProfile& other) const;
};

struct CertProfileComparator {
  bool operator()(const CertProfile& a, const CertProfile& b) const;
};

// Parses `protocol_version_value` as ProtocolVersion enum.
std::optional<ProtocolVersion> ParseProtocolVersion(
    std::optional<int> protocol_version_value);

void RegisterProfilePrefs(PrefRegistrySimple* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
const char* GetPrefNameForCertProfiles(CertScope scope);
const char* GetPrefNameForSerialization(CertScope scope);

// Returns the nickname (CKA_LABEL) for keys created for the |profile_id|.
std::string GetKeyName(CertProfileId profile_id);
// Returns the flow type type for VA API calls for |scope|.
::attestation::VerifiedAccessFlow GetVaFlowType(CertScope scope);
chromeos::platform_keys::TokenId GetPlatformKeysTokenId(CertScope scope);

// This functions should be used to delete keys that were created by
// TpmChallengeKey* and were not registered yet. (To delete registered keys
// PlatformKeysService should be used.)
void DeleteVaKey(CertScope scope,
                 Profile* profile,
                 const std::string& key_name,
                 DeleteVaKeyCallback callback);
void DeleteVaKeysByPrefix(CertScope scope,
                          Profile* profile,
                          const std::string& key_prefix,
                          DeleteVaKeyCallback callback);

// Parses |data| using net::X509Certificate::FORMAT_AUTO as format specifier.
// Expects exactly one certificate to be the result and returns it.  If parsing
// fails or if more than one certificates were in |data|, returns an null ptr.
scoped_refptr<net::X509Certificate> CreateSingleCertificateFromBytes(
    const char* data,
    size_t length);

// Returns the PlatformKeysService to be used.
// If |scope| is CertScope::kDevice, |profile| is ignored and the
// device-wide PlatformKeysService is returned.
// If |scope| is CertScope::kUser, returns the service for |profile|.
// The returned object is owned by the Profile (user-specific) or globally
// (device-wide) and may only be used until it notifies its observers that it is
// being shut down.
platform_keys::PlatformKeysService* GetPlatformKeysService(CertScope scope,
                                                           Profile* profile);

// Returns the KeyPermissionsManager to be used.
// If |scope| is CertScope::kDevice, |profile| is ignored and the
// system token key permissions manager is returned.
// If |scope| is CertScope::kUser, returns the user private slot key permissions
// manager for |profile|.
platform_keys::KeyPermissionsManager* GetKeyPermissionsManager(
    CertScope scope,
    Profile* profile);

// Generates a random unique identifier for a certificate provisioning process.
// It is used to receive invalidations (to wake up waiting workers) and for
// consistent logging with the server-side code (see "cppId" in the logs).
std::string GenerateCertProvisioningId();

// Creates an invalidation listener type based the cert provisioning process id
// (see `GenerateCertProvisioningId()`). The type is a string that is
// constructed both server- and client-side and is used to deliver FCM
// invalidations from the server-side.
std::string MakeInvalidationListenerType(
    const std::string& cert_prov_process_id);

// Returns true if workers should only progress when they receive an
// invalidation (not on timeout).
bool ShouldOnlyUseInvalidations();

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
