// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_util.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/kcer/key_permissions.pb.h"
#include "base/base64.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::platform_keys::internal {

namespace {
// The profile pref prefs::kPlatformKeys stores a dictionary mapping from
// public key (base64 encoding of an DER-encoded SPKI) to key properties. The
// currently only key property is the key usage, which can either be undefined
// or "corporate". If a key is not present in the pref, the default for the key
// usage is undefined, which in particular means "not for corporate usage".
// E.g. the entry in the profile pref might look like:
// "platform_keys" : {
//   "ABCDEF123" : {
//     "keyUsage" : "corporate"
//   },
//   "abcdef567" : {
//     "keyUsage" : "corporate"
//   }
// }
const char kPrefKeyUsage[] = "keyUsage";
const char kPrefKeyUsageCorporate[] = "corporate";

const base::Value* GetPrefsEntry(const std::string& public_key_spki_der_b64,
                                 const PrefService* const profile_prefs) {
  if (!profile_prefs) {
    return nullptr;
  }

  const base::Value::Dict& platform_keys =
      profile_prefs->GetDict(prefs::kPlatformKeys);

  return platform_keys.Find(public_key_spki_der_b64);
}

}  // namespace

bool IsUserKeyMarkedCorporateInPref(
    const std::vector<uint8_t>& public_key_spki_der,
    PrefService* profile_prefs) {
  const base::Value* prefs_entry =
      GetPrefsEntry(base::Base64Encode(public_key_spki_der), profile_prefs);
  if (prefs_entry) {
    const std::string* key_usage =
        prefs_entry->GetDict().FindString(kPrefKeyUsage);
    if (!key_usage) {
      return false;
    }

    return *key_usage == kPrefKeyUsageCorporate;
  }
  return false;
}

void MarkUserKeyCorporateInPref(const std::vector<uint8_t>& public_key_spki_der,
                                PrefService* profile_prefs) {
  ScopedDictPrefUpdate update(profile_prefs, prefs::kPlatformKeys);

  base::Value::Dict new_pref_entry;
  new_pref_entry.Set(kPrefKeyUsage, kPrefKeyUsageCorporate);

  update->Set(base::Base64Encode(public_key_spki_der),
              std::move(new_pref_entry));
}

// Serializes the KeyPermissions `message` as bytes.
std::vector<uint8_t> KeyPermissionsProtoToBytes(
    const chaps::KeyPermissions& message) {
  std::vector<uint8_t> result;
  result.resize(message.ByteSizeLong());
  message.SerializeToArray(result.data(), result.size());
  return result;
}

// Deserializes the KeyPermissions `message` from `bytes`. Returns true on
// success, false on failure.
[[nodiscard]] bool KeyPermissionsProtoFromBytes(
    const std::vector<uint8_t>& bytes,
    chaps::KeyPermissions& message) {
  return message.ParseFromArray(bytes.data(), bytes.size());
}

}  // namespace ash::platform_keys::internal
