// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_storage_id_salt.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

namespace {

struct MediaStorageIdSaltUserData : public base::SupportsUserData::Data {
  explicit MediaStorageIdSaltUserData(std::vector<uint8_t> salt)
      : salt(std::move(salt)) {}
  std::vector<uint8_t> salt;
};

const void* const kMediaStorageIdSaltUserDataKey =
    &kMediaStorageIdSaltUserDataKey;

std::vector<uint8_t> GetSaltFromPrefs(PrefService* pref_service) {
  // Salt is stored as hex-encoded string.
  std::string encoded_salt =
      pref_service->GetString(prefs::kMediaStorageIdSalt);
  std::vector<uint8_t> salt;
  if (encoded_salt.length() == MediaStorageIdSalt::kSaltLength * 2 &&
      base::HexStringToBytes(encoded_salt, &salt)) {
    return salt;
  }

  if (!encoded_salt.empty()) {
    DLOG(ERROR) << "Saved value for " << prefs::kMediaStorageIdSalt
                << " is not valid: " << encoded_salt;
  }

  // Generate and store a new salt.
  salt = crypto::RandBytesAsVector(MediaStorageIdSalt::kSaltLength);
  pref_service->SetString(prefs::kMediaStorageIdSalt, base::HexEncode(salt));
  return salt;
}

std::vector<uint8_t> GetSaltForOffTheRecordProfile(Profile* profile) {
  auto* user_data = static_cast<MediaStorageIdSaltUserData*>(
      profile->GetUserData(kMediaStorageIdSaltUserDataKey));
  if (user_data) {
    return user_data->salt;
  }

  std::vector<uint8_t> salt =
      crypto::RandBytesAsVector(MediaStorageIdSalt::kSaltLength);
  profile->SetUserData(kMediaStorageIdSaltUserDataKey,
                       std::make_unique<MediaStorageIdSaltUserData>(salt));
  return salt;
}

}  // namespace

std::vector<uint8_t> MediaStorageIdSalt::GetSalt(Profile* profile) {
  if (!profile) {
    return {};
  }

  // Regular profiles persist the salt in user preferences.
  if (!profile->IsOffTheRecord()) {
    return GetSaltFromPrefs(profile->GetPrefs());
  }

  // Off-the-record profiles use an in-memory ephemeral salt to prevent
  // falling through to the regular profile's persistent pref store.
  return GetSaltForOffTheRecordProfile(profile);
}

void MediaStorageIdSalt::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMediaStorageIdSalt, std::string());
}
