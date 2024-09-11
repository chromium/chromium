// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_serializer.h"

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {
namespace cert_provisioning {

namespace {

constexpr char kKeyNameProcessId[] = "process_id";
constexpr char kKeyNameCertScope[] = "cert_scope";
constexpr char kKeyNameCertProfile[] = "cert_profile";
constexpr char kKeyNameState[] = "state";
constexpr char kKeyNamePublicKey[] = "public_key";
constexpr char kKeyNameInvalidationTopic[] = "invalidation_topic";
constexpr char kKeyNameKeyLocation[] = "key_location";
constexpr char kKeyNameAttemptedVaChallenge[] = "attempted_va_challenge";
constexpr char kKeyNameAttemptedProofOfPossession[] =
    "attempted_proof_of_possession";
constexpr char kKeyNameProofOfPossessionSignature[] =
    "proof_of_possession_signature";

constexpr char kKeyNameCertProfileId[] = "profile_id";
constexpr char kKeyNameCertProfileName[] = "name";
constexpr char kKeyNameCertProfileVersion[] = "policy_version";
constexpr char kKeyNameCertProfileProtocolVersion[] = "protocol_version";
constexpr char kKeyNameCertProfileVaEnabled[] = "va_enabled";
constexpr char kKeyNameCertProfileRenewalPeriod[] = "renewal_period";
constexpr char kKeyNameCertProfileKeyType[] = "key_type";

template <typename T>
bool ConvertToEnum(int value, T* dst) {
  if ((value < 0) || (value > static_cast<int>(T::kMaxValue))) {
    return false;
  }
  *dst = static_cast<T>(value);
  return true;
}

template <typename T>
bool DeserializeEnumValue(const base::Value::Dict& parent_dict,
                          const char* value_name,
                          T* dst) {
  std::optional<int> serialized_enum = parent_dict.FindInt(value_name);
  if (!serialized_enum.has_value()) {
    return false;
  }
  return ConvertToEnum<T>(*serialized_enum, dst);
}

bool DeserializeStringValue(const base::Value::Dict& parent_dict,
                            const char* value_name,
                            std::string* dst) {
  const std::string* serialized_string = parent_dict.FindString(value_name);
  if (!serialized_string) {
    return false;
  }
  *dst = *serialized_string;
  return true;
}

bool DeserializeBoolValue(const base::Value::Dict& parent_dict,
                          const char* value_name,
                          bool* dst) {
  std::optional<bool> serialized_bool = parent_dict.FindBool(value_name);
  if (!serialized_bool.has_value()) {
    return false;
  }
  *dst = *serialized_bool;
  return true;
}

bool DeserializeRenewalPeriod(const base::Value::Dict& parent_dict,
                              const char* value_name,
                              base::TimeDelta* dst) {
  std::optional<int> serialized_time = parent_dict.FindInt(value_name);
  *dst = base::Seconds(serialized_time.value_or(0));
  return true;
}

bool DeserializeProtocolVersion(const base::Value::Dict& parent_value,
                                const char* value_name,
                                ProtocolVersion* dst) {
  std::optional<int> protocol_version_value = parent_value.FindInt(value_name);
  std::optional<ProtocolVersion> protocol_version =
      ParseProtocolVersion(protocol_version_value);
  if (!protocol_version.has_value()) {
    return false;
  }
  *dst = *protocol_version;
  return true;
}

base::Value::Dict SerializeCertProfile(const CertProfile& profile) {
  static_assert(CertProfile::kVersion == 7, "This function should be updated");

  base::Value::Dict result;
  result.Set(kKeyNameCertProfileId, profile.profile_id);
  result.Set(kKeyNameCertProfileName, profile.name);
  result.Set(kKeyNameCertProfileVersion, profile.policy_version);
  result.Set(kKeyNameCertProfileVaEnabled, profile.is_va_enabled);
  if (profile.protocol_version != ProtocolVersion::kStatic) {
    // Only set the protocol_version and key type if it's not kStatic to avoid
    // changing how "static flow" workers are serialized.
    result.Set(kKeyNameCertProfileProtocolVersion,
               static_cast<int>(profile.protocol_version));
    result.Set(kKeyNameCertProfileKeyType, static_cast<int>(profile.key_type));
  }

  if (!profile.renewal_period.is_zero()) {
    result.Set(kKeyNameCertProfileRenewalPeriod,
               base::saturated_cast<int>(profile.renewal_period.InSeconds()));
  }

  return result;
}

bool DeserializeCertProfile(const base::Value::Dict& parent_dict,
                            const char* value_name,
                            CertProfile* dst) {
  static_assert(CertProfile::kVersion == 7, "This function should be updated");

  const base::Value::Dict* serialized_profile =
      parent_dict.FindDict(value_name);

  if (!serialized_profile) {
    return false;
  }

  bool is_ok = true;
  is_ok = is_ok &&
          DeserializeStringValue(*serialized_profile, kKeyNameCertProfileId,
                                 &(dst->profile_id));
  is_ok =
      is_ok && DeserializeStringValue(*serialized_profile,
                                      kKeyNameCertProfileName, &(dst->name));
  is_ok = is_ok && DeserializeStringValue(*serialized_profile,
                                          kKeyNameCertProfileVersion,
                                          &(dst->policy_version));
  is_ok = is_ok && DeserializeBoolValue(*serialized_profile,
                                        kKeyNameCertProfileVaEnabled,
                                        &(dst->is_va_enabled));
  is_ok = is_ok && DeserializeRenewalPeriod(*serialized_profile,
                                            kKeyNameCertProfileRenewalPeriod,
                                            &(dst->renewal_period));
  is_ok = is_ok && DeserializeProtocolVersion(
                       *serialized_profile, kKeyNameCertProfileProtocolVersion,
                       &(dst->protocol_version));

  // The static protocol does not support key types other than RSA, and should
  // not serialize the key type, so we hardcode it here instead.
  if (is_ok && dst->protocol_version == ProtocolVersion::kStatic) {
    dst->key_type = KeyType::kRsa;
  } else {
    is_ok = is_ok &&
            DeserializeEnumValue(*serialized_profile,
                                 kKeyNameCertProfileKeyType, &(dst->key_type));
  }

  return is_ok;
}

std::string SerializeBase64Encoded(const std::vector<uint8_t>& public_key) {
  return base::Base64Encode(public_key);
}

bool DeserializeBase64Encoded(const base::Value::Dict& parent_dict,
                              const char* value_name,
                              std::vector<uint8_t>* dst) {
  const std::string* serialized_public_key = parent_dict.FindString(value_name);

  if (!serialized_public_key) {
    return false;
  }

  std::optional<std::vector<uint8_t>> public_key =
      base::Base64Decode(*serialized_public_key);
  if (!public_key) {
    return false;
  }
  *dst = std::move(*public_key);

  return true;
}

}  // namespace

void CertProvisioningSerializer::SerializeWorkerToPrefs(
    PrefService* pref_service,
    const CertProvisioningWorkerStatic& worker) {
  ScopedDictPrefUpdate scoped_dict_updater(
      pref_service, GetPrefNameForSerialization(worker.cert_scope_));
  base::Value::Dict& saved_workers = scoped_dict_updater.Get();
  saved_workers.Set(worker.cert_profile_.profile_id, SerializeWorker(worker));
}

void CertProvisioningSerializer::SerializeWorkerToPrefs(
    PrefService* pref_service,
    const CertProvisioningWorkerDynamic& worker) {
  ScopedDictPrefUpdate scoped_dict_updater(
      pref_service, GetPrefNameForSerialization(worker.cert_scope_));
  base::Value::Dict& saved_workers = scoped_dict_updater.Get();
  saved_workers.Set(worker.cert_profile_.profile_id, SerializeWorker(worker));
}

void CertProvisioningSerializer::DeleteWorkerFromPrefs(
    PrefService* pref_service,
    const CertProvisioningWorkerStatic& worker) {
  ScopedDictPrefUpdate scoped_dict_updater(
      pref_service, GetPrefNameForSerialization(worker.cert_scope_));

  base::Value::Dict& saved_workers = scoped_dict_updater.Get();

  saved_workers.Remove(worker.cert_profile_.profile_id);
}

void CertProvisioningSerializer::DeleteWorkerFromPrefs(
    PrefService* pref_service,
    const CertProvisioningWorkerDynamic& worker) {
  ScopedDictPrefUpdate scoped_dict_updater(
      pref_service, GetPrefNameForSerialization(worker.cert_scope_));

  base::Value::Dict& saved_workers = scoped_dict_updater.Get();

  saved_workers.Remove(worker.cert_profile_.profile_id);
}

// Serialization scheme:
// {
//   "cert_scope": <number>,
//   "cert_profile": <CertProfile>,
//   "state": <number>,
//   "public_key": <string>,
//   "invalidation_topic": <string>,
// }
base::Value::Dict CertProvisioningSerializer::SerializeWorker(
    const CertProvisioningWorkerStatic& worker) {
  static_assert(CertProvisioningWorkerStatic::kVersion == 2,
                "This function should be updated");

  base::Value::Dict result;

  result.Set(kKeyNameProcessId, worker.process_id_);
  result.Set(kKeyNameCertProfile, SerializeCertProfile(worker.cert_profile_));
  result.Set(kKeyNameCertScope, static_cast<int>(worker.cert_scope_));
  result.Set(kKeyNameState, static_cast<int>(worker.state_));
  result.Set(kKeyNamePublicKey, SerializeBase64Encoded(worker.public_key_));
  result.Set(kKeyNameInvalidationTopic, worker.invalidation_topic_);
  return result;
}

// Serialization scheme:
// {
//   "cert_scope": <number>,
//   "cert_profile": <CertProfile>,
//   "state": <number>,
//   "public_key": <string>,
//   "invalidation_topic": <string>,
//   "key_location": <number>,
//   "attempted_va_challenge": <bool>,
//   "proof_of_possession_count": <number>,
// }
base::Value::Dict CertProvisioningSerializer::SerializeWorker(
    const CertProvisioningWorkerDynamic& worker) {
  static_assert(CertProvisioningWorkerDynamic::kVersion == 3,
                "This function should be updated");

  base::Value::Dict result;

  result.Set(kKeyNameProcessId, worker.process_id_);
  result.Set(kKeyNameCertProfile, SerializeCertProfile(worker.cert_profile_));
  result.Set(kKeyNameCertScope, static_cast<int>(worker.cert_scope_));
  result.Set(kKeyNameState, static_cast<int>(worker.state_));
  result.Set(kKeyNamePublicKey, SerializeBase64Encoded(worker.public_key_));
  result.Set(kKeyNameInvalidationTopic, worker.invalidation_topic_);
  result.Set(kKeyNameKeyLocation, static_cast<int>(worker.key_location_));
  result.Set(kKeyNameAttemptedVaChallenge, worker.attempted_va_challenge_);
  result.Set(kKeyNameAttemptedProofOfPossession,
             worker.attempted_proof_of_possession_);
  result.Set(kKeyNameProofOfPossessionSignature,
             SerializeBase64Encoded(worker.signature_));
  return result;
}

bool CertProvisioningSerializer::DeserializeWorker(
    const base::Value::Dict& saved_worker,
    CertProvisioningWorkerStatic* worker) {
  static_assert(CertProvisioningWorkerStatic::kVersion == 2,
                "This function should be updated");

  // This will show to the scheduler that the worker is not doing anything yet
  // and that it should be continued manually.
  worker->is_waiting_ = true;

  bool is_ok = true;
  int error_code = 0;

  // Try to only add new deserialize statements at the end so error_code values
  // are stable.
  is_ok = is_ok && ++error_code &&
          DeserializeEnumValue<CertScope>(saved_worker, kKeyNameCertScope,
                                          &(worker->cert_scope_));

  is_ok = is_ok && ++error_code &&
          DeserializeCertProfile(saved_worker, kKeyNameCertProfile,
                                 &(worker->cert_profile_));

  is_ok = is_ok && ++error_code &&
          DeserializeEnumValue<CertProvisioningWorkerState>(
              saved_worker, kKeyNameState, &(worker->state_));

  is_ok = is_ok && ++error_code &&
          DeserializeBase64Encoded(saved_worker, kKeyNamePublicKey,
                                   &(worker->public_key_));

  is_ok = is_ok && ++error_code &&
          DeserializeStringValue(saved_worker, kKeyNameInvalidationTopic,
                                 &(worker->invalidation_topic_));

  is_ok = is_ok && ++error_code &&
          DeserializeStringValue(saved_worker, kKeyNameProcessId,
                                 &(worker->process_id_));

  if (!is_ok) {
    LOG(ERROR)
        << " Failed to deserialize cert provisioning worker, error code: "
        << error_code;
    return false;
  }

  worker->InitAfterDeserialization();

  return true;
}

bool CertProvisioningSerializer::DeserializeWorker(
    const base::Value::Dict& saved_worker,
    CertProvisioningWorkerDynamic* worker) {
  static_assert(CertProvisioningWorkerDynamic::kVersion == 3,
                "This function should be updated");

  // This will show to the scheduler that the worker is not doing anything yet
  // and that it should be continued manually.
  worker->is_waiting_ = true;

  bool is_ok = true;
  int error_code = 0;

  // Try to only add new deserialize statements at the end so error_code values
  // are stable.
  is_ok = is_ok && ++error_code &&
          DeserializeEnumValue<CertScope>(saved_worker, kKeyNameCertScope,
                                          &(worker->cert_scope_));

  is_ok = is_ok && ++error_code &&
          DeserializeCertProfile(saved_worker, kKeyNameCertProfile,
                                 &(worker->cert_profile_));

  is_ok = is_ok && ++error_code &&
          DeserializeEnumValue<CertProvisioningWorkerState>(
              saved_worker, kKeyNameState, &(worker->state_));

  is_ok = is_ok && ++error_code &&
          DeserializeBase64Encoded(saved_worker, kKeyNamePublicKey,
                                   &(worker->public_key_));

  is_ok = is_ok && ++error_code &&
          DeserializeStringValue(saved_worker, kKeyNameInvalidationTopic,
                                 &(worker->invalidation_topic_));

  is_ok = is_ok && ++error_code &&
          DeserializeEnumValue<KeyLocation>(saved_worker, kKeyNameKeyLocation,
                                            &(worker->key_location_));

  is_ok = is_ok && ++error_code &&
          DeserializeBoolValue(saved_worker, kKeyNameAttemptedVaChallenge,
                               &(worker->attempted_va_challenge_));

  is_ok = is_ok && ++error_code &&
          DeserializeBoolValue(saved_worker, kKeyNameAttemptedProofOfPossession,
                               &(worker->attempted_proof_of_possession_));

  is_ok =
      is_ok && ++error_code &&
      DeserializeBase64Encoded(saved_worker, kKeyNameProofOfPossessionSignature,
                               &(worker->signature_));

  is_ok = is_ok && ++error_code &&
          DeserializeStringValue(saved_worker, kKeyNameProcessId,
                                 &(worker->process_id_));

  if (!is_ok) {
    LOG(ERROR)
        << " Failed to deserialize cert provisioning worker, error code: "
        << error_code;
    return false;
  }

  worker->InitAfterDeserialization();

  return true;
}

std::optional<ProtocolVersion> CertProvisioningSerializer::GetProtocolVersion(
    const base::Value::Dict& saved_worker) {
  CertProfile cert_profile;
  if (!DeserializeCertProfile(saved_worker, kKeyNameCertProfile,
                              &cert_profile)) {
    return {};
  }
  return cert_profile.protocol_version;
}

}  // namespace cert_provisioning
}  // namespace ash
