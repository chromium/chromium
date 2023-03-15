// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_prefs.h"

#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::ambient::prefs {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;

class AmbientPrefsTest : public ::testing::Test {
 protected:
  AmbientPrefsTest() {
    test_pref_service_.registry()->RegisterIntegerPref(
        kAmbientTheme, static_cast<int>(kDefaultAmbientTheme));
    test_pref_service_.registry()->RegisterDictionaryPref(kAmbientUiSettings,
                                                          base::Value::Dict());
  }

  TestingPrefServiceSimple test_pref_service_;
};

TEST_F(AmbientPrefsTest, MigrateDeprecatedPrefs) {
  test_pref_service_.SetInteger(kAmbientTheme,
                                static_cast<int>(AmbientTheme::kFeelTheBreeze));
  MigrateDeprecatedPrefs(test_pref_service_);
  const base::Value::Dict& new_pref =
      test_pref_service_.GetDict(kAmbientUiSettings);
  EXPECT_THAT(new_pref.FindInt(kAmbientUiSettingsFieldTheme),
              Eq(static_cast<int>(AmbientTheme::kFeelTheBreeze)));

  MigrateDeprecatedPrefs(test_pref_service_);
  EXPECT_FALSE(test_pref_service_.HasPrefPath(ambient::prefs::kAmbientTheme));
}

TEST_F(AmbientPrefsTest, MigrateDeprecatedPrefsRejectsInvalidLegacySettings) {
  // This should not be possible by design. The video option was not introduced
  // as part of the legacy pref.
  test_pref_service_.SetInteger(ambient::prefs::kAmbientTheme,
                                static_cast<int>(AmbientTheme::kVideo));

  MigrateDeprecatedPrefs(test_pref_service_);
  // Resorts to default since legacy pref is invalid.
  EXPECT_TRUE(test_pref_service_.GetDict(kAmbientUiSettings).empty());
  EXPECT_FALSE(test_pref_service_.HasPrefPath(ambient::prefs::kAmbientTheme));
}

}  // namespace
}  // namespace ash::ambient::prefs
