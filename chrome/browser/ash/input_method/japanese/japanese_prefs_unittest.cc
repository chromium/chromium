// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/japanese/japanese_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

using ::base::Value;

TEST(JapanesePrefsTest, SetJpOptionsSourceAsPrefService) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(
      ::prefs::kLanguageInputMethodSpecificSettings);

  SetJpOptionsSourceAsPrefService(prefs);

  EXPECT_EQ(*prefs.GetUserPref(::prefs::kLanguageInputMethodSpecificSettings),
            Value::Dict().Set(
                "nacl_mozc_jp",
                Value::Dict().Set("Metadata-OptionsSource", "PrefService")));
}

TEST(JapanesePrefsTest, SetJpOptionsSourceAsLegacyConfig) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterDictionaryPref(
      ::prefs::kLanguageInputMethodSpecificSettings);

  SetJpOptionsSourceAsLegacyConfig(prefs);

  EXPECT_EQ(*prefs.GetUserPref(::prefs::kLanguageInputMethodSpecificSettings),
            Value::Dict().Set("nacl_mozc_jp",
                              Value::Dict().Set("Metadata-OptionsSource",
                                                "LegacyConfig1Db")));
}

}  // namespace
}  // namespace ash::input_method
