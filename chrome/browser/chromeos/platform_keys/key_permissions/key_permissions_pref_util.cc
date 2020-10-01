// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

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

const base::DictionaryValue* GetPrefsEntry(
    const std::string& public_key_spki_der_b64,
    const PrefService* const profile_prefs) {
  if (!profile_prefs)
    return nullptr;

  const base::DictionaryValue* platform_keys =
      profile_prefs->GetDictionary(prefs::kPlatformKeys);
  if (!platform_keys)
    return nullptr;

  const base::Value* key_entry_value =
      platform_keys->FindKey(public_key_spki_der_b64);
  if (!key_entry_value)
    return nullptr;

  const base::DictionaryValue* key_entry = nullptr;
  key_entry_value->GetAsDictionary(&key_entry);
  return key_entry;
}

}  // namespace

namespace chromeos {
namespace platform_keys {
namespace internal {

bool IsUserKeyMarkedCorporateInPref(const std::string& public_key_spki_der,
                                    PrefService* profile_prefs) {
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  const base::DictionaryValue* prefs_entry =
      GetPrefsEntry(public_key_spki_der_b64, profile_prefs);
  if (prefs_entry) {
    const base::Value* key_usage = prefs_entry->FindKey(kPrefKeyUsage);
    if (!key_usage || !key_usage->is_string())
      return false;
    return key_usage->GetString() == kPrefKeyUsageCorporate;
  }
  return false;
}

void MarkUserKeyCorporateInPref(const std::string& public_key_spki_der,
                                PrefService* profile_prefs) {
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  DictionaryPrefUpdate update(profile_prefs, prefs::kPlatformKeys);

  auto new_pref_entry = std::make_unique<base::DictionaryValue>();
  new_pref_entry->SetKey(kPrefKeyUsage, base::Value(kPrefKeyUsageCorporate));

  update->SetWithoutPathExpansion(public_key_spki_der_b64,
                                  std::move(new_pref_entry));
}

}  // namespace internal
}  // namespace platform_keys
}  // namespace chromeos
