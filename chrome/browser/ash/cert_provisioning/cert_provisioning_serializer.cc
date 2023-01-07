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
bool DeserializeEnumValue(const base::Value& parent_value,
                          const char* value_name,
                          T* dst) {
  const base::Value* serialized_enum =
      parent_value.FindKeyOfType(value_name, base::Value::Type::INTEGER);
  if (!serialized_enum) {
    return false;
  }
  return ConvertToEnum<T>(serialized_enum->GetInt(), dst);
}

bool DeserializeStringValue(const base::Value& parent_value,
                            const char* value_name,
                            std::string* dst) {
  const base::Value* serialized_string =
      parent_value.FindKeyOfType(value_name, base::Value::Type::STRING);
  if (!serialized_string) {
    return false;
  }
  *dst = serialized_string->GetString();
  return true;
}

bool DeserializeBoolValue(const base::Value& parent_value,
                          const char* value_name,
                          bool* dst) {
  const base::Value* serialized_bool =
      parent_value.FindKeyOfType(value_name, base::Value::Type::BOOLEAN);
  if (!serialized_bool) {
    return false;
  }
  *dst = serialized_bool->GetBool();
  return true;
}

bool DeserializeRenewalPeriod(const base::Value& parent_value,
                              const char* value_name,
                              base::TimeDelta* dst) {
  absl::optional<int> serialized_time = parent_value.FindIntKey(value_name);
  *dst = base::Seconds(serialized_time.value_or(0));
  return true;
}

base::Value SerializeCertProfile(const CertProfile& profile) {
  static_assert(CertProfile::kVersion == 5, "This function should be updated");

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey(kKeyNameCertProfileId, profile.profile_id);
  result.SetStringKey(kKeyNameCertProfileName, profile.name);
  result.SetStringKey(kKeyNameCertProfileVersion, profile.policy_version);
  result.SetBoolKey(kKeyNameCertProfileVaEnabled, profile.is_va_enabled);

  if (!profile.renewal_period.is_zero()) {
    result.SetIntKey(kKeyNameCertProfileRenewalPeriod,
                     profile.renewal_period.InSeconds());
  }

  return result;
}

bool DeserializeCertProfile(const base::Value& parent_value,
                            const char* value_name,
                            CertProfile* dst) {
  static_assert(CertProfile::kVersion == 5, "This function should be updated");

  const base::Value* serialized_profile =
      parent_value.FindKeyOfType(value_name, base::Value::Type::DICTIONARY);

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

base::Value SerializePublicKey(const std::string& public_key) {
  std::string public_key_str;
  base::Base64Encode(public_key, &public_key_str);
  return base::Value(std::move(public_key_str));
}

bool DeserializePublicKey(const base::Value& parent_value,
                          const char* value_name,
                          std::string* dst) {
  const base::Value* serialized_public_key =
      parent_value.FindKeyOfType(value_name, base::Value::Type::STRING);

  if (!serialized_public_key) {
    return false;
  }

  return base::Base64Decode(serialized_public_key->GetString(), dst);
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
base::Value CertProvisioningSerializer::SerializeWorker(
    const CertProvisioningWorkerImpl& worker) {
  static_assert(CertProvisioningWorkerImpl::kVersion == 1,
                "This function should be updated");

  base::Value result(base::Value::Type::DICTIONARY);

  result.SetKey(kKeyNameCertProfile,
                SerializeCertProfile(worker.cert_profile_));
  result.SetIntKey(kKeyNameCertScope, static_cast<int>(worker.cert_scope_));
  result.SetIntKey(kKeyNameState, static_cast<int>(worker.state_));
  result.SetKey(kKeyNamePublicKey, SerializePublicKey(worker.public_key_));
  result.SetStringKey(kKeyNameInvalidationTopic, worker.invalidation_topic_);
  return result;
}

bool CertProvisioningSerializer::DeserializeWorker(
    const base::Value& saved_worker,
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
