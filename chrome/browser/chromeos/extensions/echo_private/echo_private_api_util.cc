// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api_util.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos::echo_offer {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kEchoCheckedOffers);
}

void RemoveEmptyValueDicts(base::Value::Dict& dict) {
  auto it = dict.begin();
  while (it != dict.end()) {
    base::Value& value = it->second;
    if (value.is_dict()) {
      base::Value::Dict& sub_dict = value.GetDict();
      RemoveEmptyValueDicts(sub_dict);
      if (sub_dict.empty()) {
        it = dict.erase(it);
        continue;
      }
    }
    it++;
  }
}

}  // namespace chromeos::echo_offer
