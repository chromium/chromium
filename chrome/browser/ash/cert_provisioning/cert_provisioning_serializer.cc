// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_serializer.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace cert_provisioning {

namespace {

constexpr char kKeyNameCertScope[] = "cert_scope";
constexpr char kKeyNameCertProfile[] = "cert_profile";
constexpr char kKeyNameState[] = "state";
constexpr char kKeyNamePublicKey[] = "public_key";
constexpr char kKeyNameInvalidationTopic[] = "invalidation_topic";

constexpr char kKeyNameCertProfileId[] = "profile_id";
constexpr char kKeyNameCertProfileName[] = "name";
constexpr char kKeyNameCertProfileVersion[] = "policy_version";
constexpr char kKeyNameCertProfileVaEnabled[] = "va_enabled";
constexpr char kKeyNameCertProfileRenewalPeriod[] = "renewal_period";

template <typename T>
bool ConvertToEnum(int value, T* dst) {
  if ((value < 0) || (value > static_cast<int>(T::kMaxValue))) {
    return false;
  }
  *dst = static_cast<T>(value);
  return true;
}

template <typename T>
bool DeserializeEnumValue(const base::Value::Dict& parent_value,
                          const char* value_name,
                          T* dst) {
  const absl::optional<int> serialized_enum = parent_value.FindInt(value_name);
  if (!serialized_enum) {
    return false;
  }
  return ConvertToEnum<T>(*serialized_enum, dst);
}

bool DeserializeStringValue(const base::Value::Dict& parent_value,
                            const char* value_name,
                            std::string* dst) {
  const std::string* serialized_string = parent_value.FindString(value_name);
  if (!serialized_string) {
    return false;
  }
  *dst = *serialized_string;
  return true;
}

bool DeserializeBoolValue(const base::Value::Dict& parent_value,
                          const char* value_name,
                          bool* dst) {
  const absl::optional<bool> serialized_bool =
      parent_value.FindBool(value_name);
  if (!serialized_bool) {
    return false;
  }
  *dst = *serialized_bool;
  return true;
}

bool DeserializeRenewalPeriod(const base::Value::Dict& parent_value,
                              const char* value_name,
                              base::TimeDelta* dst) {
  absl::optional<int> serialized_time = parent_value.FindInt(value_name);
  *dst = base::Seconds(serialized_time.value_or(0));
  return true;
}

base::Value::Dict SerializeCertProfile(const CertProfile& profile) {
  static_assert(CertProfile::kVersion == 5, "This function should be updated");

  base::Value::Dict result;
  result.Set(kKeyNameCertProfileId, profile.profile_id);
  result.Set(kKeyNameCertProfileName, profile.name);
  result.Set(kKeyNameCertProfileVersion, profile.policy_version);
  result.Set(kKeyNameCertProfileVaEnabled, profile.is_va_enabled);

  if (!profile.renewal_period.is_zero()) {
    result.Set(kKeyNameCertProfileRenewalPeriod,
               static_cast<int>(profile.renewal_period.InSeconds()));
  }

  return result;
}

bool DeserializeCertProfile(const base::Value::Dict& parent_value,
                            const char* value_name,
                            CertProfile* dst) {
  static_assert(CertProfile::kVersion == 5, "This function should be updated");

  const base::Value::Dict* serialized_profile =
      parent_value.FindDict(value_name);

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
  return is_ok;
}

base::Value SerializePublicKey(const std::vector<uint8_t>& public_key) {
  return base::Value(base::Base64Encode(public_key));
}

bool DeserializePublicKey(const base::Value::Dict& parent_value,
                          const char* value_name,
                          std::vector<uint8_t>* dst) {
  const std::string* serialized_public_key =
      parent_value.FindString(value_name);

  if (!serialized_public_key) {
    return false;
  }

  absl::optional<std::vector<uint8_t>> public_key =
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
    const CertProvisioningWorkerImpl& worker) {
  ScopedDictPrefUpdate scoped_dict_updater(
      pref_service, GetPrefNameForSerialization(worker.cert_scope_));
  base::Value::Dict& saved_workers = scoped_dict_updater.Get();
  saved_workers.Set(worker.cert_profile_.profile_id, SerializeWorker(worker));
}

void CertProvisioningSerializer::DeleteWorkerFromPrefs(
    PrefService* pref_service,
    const CertProvisioningWorkerImpl& worker) {
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
    const CertProvisioningWorkerImpl& worker) {
  static_assert(CertProvisioningWorkerImpl::kVersion == 1,
                "This function should be updated");

  base::Value::Dict result;

  result.Set(kKeyNameCertProfile, SerializeCertProfile(worker.cert_profile_));
  result.Set(kKeyNameCertScope, static_cast<int>(worker.cert_scope_));
  result.Set(kKeyNameState, static_cast<int>(worker.state_));
  result.Set(kKeyNamePublicKey, SerializePublicKey(worker.public_key_));
  result.Set(kKeyNameInvalidationTopic, worker.invalidation_topic_);
  return result;
}

bool CertProvisioningSerializer::DeserializeWorker(
    const base::Value::Dict& saved_worker,
    CertProvisioningWorkerImpl* worker) {
  static_assert(CertProvisioningWorkerImpl::kVersion == 1,
                "This function should be updated");

  // This will show to the scheduler that the worker is not doing anything yet
  // and that it should be continued manually.
  worker->is_waiting_ = true;

  bool is_ok = true;
  int error_code = 0;

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
          DeserializePublicKey(saved_worker, kKeyNamePublicKey,
                               &(worker->public_key_));

  is_ok = is_ok && ++error_code &&
          DeserializeStringValue(saved_worker, kKeyNameInvalidationTopic,
                                 &(worker->invalidation_topic_));

  if (!is_ok) {
    LOG(ERROR)
        << " Failed to deserialize cert provisioning worker, error code: "
        << error_code;
    return false;
  }

  worker->InitAfterDeserialization();

  return true;
}

}  // namespace cert_provisioning
}  // namespace ash
