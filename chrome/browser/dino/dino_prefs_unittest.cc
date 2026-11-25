// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_prefs.h"

#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dino {

TEST(DinoPrefsTest, RegisterProfilePrefsSetsDefaults) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  RegisterProfilePrefs(pref_service.registry());

  EXPECT_EQ(0, pref_service.GetInteger(
                   prefs::kDinoBookmarkPromptMaxTimesShownCounter));
  EXPECT_LT(
      pref_service.GetInteger(prefs::kDinoBookmarkPromptMaxTimesShownCounter),
      kDinoBookmarkPromptMaxTimesShown);

  pref_service.SetInteger(prefs::kDinoBookmarkPromptMaxTimesShownCounter,
                          kDinoBookmarkPromptMaxTimesShown);
  EXPECT_EQ(
      kDinoBookmarkPromptMaxTimesShown,
      pref_service.GetInteger(prefs::kDinoBookmarkPromptMaxTimesShownCounter));
}

TEST(DinoPrefsTest, TestingProfileRegistersDinoPrefs) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  PrefService* pref_service = profile.GetPrefs();
  ASSERT_TRUE(pref_service);

  EXPECT_EQ(0, pref_service->GetInteger(
                   prefs::kDinoBookmarkPromptMaxTimesShownCounter));
}

}  // namespace dino
