// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/japanese/japanese_prefs.h"

#include "chrome/browser/ash/input_method/input_method_settings.h"
#include "chrome/browser/ash/input_method/japanese/japanese_prefs_constants.h"
#include "components/prefs/pref_service.h"

namespace ash::input_method {

using ::base::Value;

void SetJpOptionsSourceAsPrefService(PrefService& prefs) {
  SetLanguageInputMethodSpecificSetting(
      prefs, std::string(kJpPrefsEngineId),
      Value::Dict().Set(kJpPrefMetadataOptionsSource,
                        kJpPrefMetadataOptionsSourcePrefService));
}

void SetJpOptionsSourceAsLegacyConfig(PrefService& prefs) {
  SetLanguageInputMethodSpecificSetting(
      prefs, std::string(kJpPrefsEngineId),
      Value::Dict().Set(kJpPrefMetadataOptionsSource,
                        kJpPrefMetadataOptionsSourceLegacyConfig1Db));
}

}  // namespace ash::input_method
