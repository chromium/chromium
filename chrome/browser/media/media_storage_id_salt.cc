// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_storage_id_salt.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "crypto/random.h"

std::vector<uint8_t> MediaStorageIdSalt::GetSalt(PrefService* pref_service) {
  // Salt is stored as hex-encoded string.
  std::string encoded_salt =
      pref_service->GetString(prefs::kMediaStorageIdSalt);
  std::vector<uint8_t> salt;
  if (encoded_salt.length() != kSaltLength * 2 ||
      !base::HexStringToBytes(encoded_salt, &salt)) {
    // If the salt is not the proper format log an error.
    if (encoded_salt.length() > 0) {
      DLOG(ERROR) << "Saved value for " << prefs::kMediaStorageIdSalt
                  << " is not valid: " << encoded_salt;
      // Continue on to generate a new one.
    }

    // If the salt doesn't exist, generate a new one.
    salt = crypto::RandBytesAsVector(kSaltLength);
    encoded_salt = base::HexEncode(salt);
    pref_service->SetString(prefs::kMediaStorageIdSalt, encoded_salt);
  }

  return salt;
}

void MediaStorageIdSalt::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMediaStorageIdSalt, std::string());
}
