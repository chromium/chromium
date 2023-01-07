// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
#define CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_

#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/cert/x509_certificate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;
class Profile;

namespace ash {

namespace platform_keys {
class KeyPermissionsManager;
class PlatformKeysService;
}  // namespace platform_keys

namespace cert_provisioning {

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
  kStartCsrResponseReceived = 2,
  kVaChallengeFinished = 3,
  kKeyRegistered = 4,
  kKeypairMarked = 5,
  kSignCsrFinished = 6,
  kFinishCsrResponseReceived = 7,
  kSucceeded = 8,
  kInconsistentDataError = 9,
  kFailed = 10,
  kCanceled = 11,
  kMaxValue = kCanceled,
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
// with definitions of RequiredClientCertificateForDevice and
// RequiredClientCertificateForUser policies in policy_templates.json file.
const char kCertProfileIdKey[] = "cert_profile_id";
const char kCertProfileNameKey[] = "name";
const char kCertProfileRenewalPeroidSec[] = "renewal_period_seconds";
const char kCertProfilePolicyVersionKey[] = "policy_version";
const char kCertProfileIsVaEnabledKey[] = "enable_remote_attestation_check";

struct CertProfile {
  static absl::optional<CertProfile> MakeFromValue(const base::Value& value);

  CertProfile();
  // For tests.
  CertProfile(CertProfileId profile_id,
              std::string name,
              std::string policy_version,
              bool is_va_enabled,
              base::TimeDelta renewal_period);
  CertProfile(const CertProfile& other);
  ~CertProfile();

  CertProfileId profile_id;
  // Human-readable name (UTF-8).
  std::string name;
  std::string policy_version;
  bool is_va_enabled = true;
  // Default renewal period 0 means that a certificate will be renewed only
  // after the previous one has expired (0 seconds before it is expires).
  base::TimeDelta renewal_period = base::Seconds(0);

  // IMPORTANT:
  // Increment this when you add/change any member in CertProfile (and update
  // all functions that fail to compile because of it).
  static constexpr int kVersion = 5;

  bool operator==(const CertProfile& other) const;
  bool operator!=(const CertProfile& other) const;
};

struct CertProfileComparator {
  bool operator()(const CertProfile& a, const CertProfile& b) const;
};

void RegisterProfilePrefs(PrefRegistrySimple* registry);
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
const char* GetPrefNameForCertProfiles(CertScope scope);
const char* GetPrefNameForSerialization(CertScope scope);

// Returns the nickname (CKA_LABEL) for keys created for the |profile_id|.
std::string GetKeyName(CertProfileId profile_id);
// Returns the key type for VA API calls for |scope|.
attestation::AttestationKeyType GetVaKeyType(CertScope scope);
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

}  // namespace cert_provisioning
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CERT_PROVISIONING_CERT_PROVISIONING_COMMON_H_
