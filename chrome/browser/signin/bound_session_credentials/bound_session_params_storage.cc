// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"

#include "base/base64.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const char kBoundSessionParamsPref[] =
    "bound_session_credentials_bound_session_params";

class BoundSessionParamsPrefsStorage : public BoundSessionParamsStorage {
 public:
  explicit BoundSessionParamsPrefsStorage(PrefService& pref_service);
  ~BoundSessionParamsPrefsStorage() override;

  [[nodiscard]] bool SaveParams(
      const bound_session_credentials::BoundSessionParams& params) override;

  absl::optional<bound_session_credentials::BoundSessionParams> ReadParams()
      const override;

  void ClearParams() override;

 private:
  const raw_ref<PrefService> pref_service_;
};

BoundSessionParamsPrefsStorage::BoundSessionParamsPrefsStorage(
    PrefService& pref_service)
    : pref_service_(pref_service) {}

BoundSessionParamsPrefsStorage::~BoundSessionParamsPrefsStorage() = default;

bool BoundSessionParamsPrefsStorage::SaveParams(
    const bound_session_credentials::BoundSessionParams& params) {
  if (!AreParamsValid(params)) {
    return false;
  }

  std::string serialized_params = params.SerializeAsString();
  if (serialized_params.empty()) {
    return false;
  }

  std::string encoded_serialized_params;
  base::Base64Encode(serialized_params, &encoded_serialized_params);
  pref_service_->SetString(kBoundSessionParamsPref, encoded_serialized_params);
  return true;
}

absl::optional<bound_session_credentials::BoundSessionParams>
BoundSessionParamsPrefsStorage::ReadParams() const {
  std::string encoded_params_str =
      pref_service_->GetString(kBoundSessionParamsPref);
  if (encoded_params_str.empty()) {
    return absl::nullopt;
  }

  std::string params_str;
  if (!base::Base64Decode(encoded_params_str, &params_str)) {
    return absl::nullopt;
  }

  bound_session_credentials::BoundSessionParams params;
  if (params.ParseFromString(params_str) && AreParamsValid(params)) {
    return params;
  }
  return absl::nullopt;
}

void BoundSessionParamsPrefsStorage::ClearParams() {
  pref_service_->ClearPref(kBoundSessionParamsPref);
}

class BoundSessionParamsInMemoryStorage : public BoundSessionParamsStorage {
 public:
  BoundSessionParamsInMemoryStorage();
  ~BoundSessionParamsInMemoryStorage() override;

  [[nodiscard]] bool SaveParams(
      const bound_session_credentials::BoundSessionParams& params) override;

  absl::optional<bound_session_credentials::BoundSessionParams> ReadParams()
      const override;

  void ClearParams() override;

 private:
  absl::optional<bound_session_credentials::BoundSessionParams>
      in_memory_params_;
};

BoundSessionParamsInMemoryStorage::BoundSessionParamsInMemoryStorage() =
    default;
BoundSessionParamsInMemoryStorage::~BoundSessionParamsInMemoryStorage() =
    default;

bool BoundSessionParamsInMemoryStorage::SaveParams(
    const bound_session_credentials::BoundSessionParams& params) {
  if (!AreParamsValid(params)) {
    return false;
  }

  in_memory_params_ = params;
  return true;
}

absl::optional<bound_session_credentials::BoundSessionParams>
BoundSessionParamsInMemoryStorage::ReadParams() const {
  return in_memory_params_;
}

void BoundSessionParamsInMemoryStorage::ClearParams() {
  in_memory_params_ = absl::nullopt;
}

}  // namespace

// static
std::unique_ptr<BoundSessionParamsStorage>
BoundSessionParamsStorage::CreateForProfile(Profile& profile) {
  if (profile.IsOffTheRecord()) {
    return std::make_unique<BoundSessionParamsInMemoryStorage>();
  }
  return std::make_unique<BoundSessionParamsPrefsStorage>(*profile.GetPrefs());
}

// static
std::unique_ptr<BoundSessionParamsStorage>
BoundSessionParamsStorage::CreatePrefsStorageForTesting(
    PrefService& pref_service) {
  return std::make_unique<BoundSessionParamsPrefsStorage>(pref_service);
}

// static
void BoundSessionParamsStorage::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kBoundSessionParamsPref, std::string());
}

// static
bool BoundSessionParamsStorage::AreParamsValid(
    const bound_session_credentials::BoundSessionParams& bound_session_params) {
  // TODO(crbug.com/1441168): Check for validity of other fields once they are
  // available.
  return bound_session_params.has_session_id() &&
         bound_session_params.has_wrapped_key();
}
