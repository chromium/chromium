// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_settings.h"

#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class DevToolsSettingsTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Simple smoke test to verify that the basic CRUD operations are working.
// Only verifies through the API, doesn't check the backend state.
TEST_F(DevToolsSettingsTest, BasicApiTest) {
  DevToolsSettings settings(&profile_);

  settings.Register("setting_a", {RegisterOptions::SyncMode::kSync});
  settings.Register("setting_b", {RegisterOptions::SyncMode::kDontSync});

  settings.Set("setting_a", "foo");
  settings.Set("setting_b", "bar");

  base::Value::Dict prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting_a"), "foo");
  EXPECT_EQ(*prefs.FindString("setting_b"), "bar");

  settings.Remove("setting_a");
  prefs = settings.Get();
  EXPECT_EQ(prefs.FindString("setting_a"), nullptr);

  settings.Clear();
  prefs = settings.Get();
  // kSyncDevToolsPreferenceFrontendName is always reported.
  EXPECT_EQ(prefs.size(), static_cast<size_t>(1));
  EXPECT_EQ(prefs.FindString("setting_b"), nullptr);
}

TEST_F(DevToolsSettingsTest, CanMoveUnsyncedSettingToBeingSynced) {
  {
    DevToolsSettings settings(&profile_);
    settings.Register("setting", {RegisterOptions::SyncMode::kSync});
    settings.Set("setting", "value");
  }

  // Simulate a new session by creating a new `DevToolsSettings` instance.
  DevToolsSettings settings(&profile_);
  settings.Register("setting", {RegisterOptions::SyncMode::kDontSync});

  base::Value::Dict prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting"), "value");

  settings.Set("setting", "new_value");
  prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting"), "new_value");

  settings.Remove("setting");
  prefs = settings.Get();
  EXPECT_EQ(prefs.FindString("setting"), nullptr);
}

TEST_F(DevToolsSettingsTest, CanMoveSyncedSettingToBeingUnsynced) {
  {
    DevToolsSettings settings(&profile_);
    settings.Register("setting", {RegisterOptions::SyncMode::kDontSync});
    settings.Set("setting", "value");
  }

  // Simulate a new session by creating a new `DevToolsSettings` instance.
  DevToolsSettings settings(&profile_);
  settings.Register("setting", {RegisterOptions::SyncMode::kSync});

  base::Value::Dict prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting"), "value");

  settings.Set("setting", "new_value");
  prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting"), "new_value");

  settings.Remove("setting");
  prefs = settings.Get();
  EXPECT_EQ(prefs.FindString("setting"), nullptr);
}

TEST_F(DevToolsSettingsTest, MovingUnsycnedToSyncedDoesNotOverwrite) {
  // 1) Enable sync and register an unsynced setting.
  {
    DevToolsSettings settings(&profile_);
    settings.Register("setting", {RegisterOptions::SyncMode::kDontSync});
    settings.Set("setting", "unsynced value");
    settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName,
                 "true");
  }

  // 2) Simulate the update to synced plus setting of a new value on a
  //    different device.
  {
    ScopedDictPrefUpdate update(profile_.GetPrefs(),
                                prefs::kDevToolsSyncedPreferencesSyncEnabled);
    update->Set("setting", "overwritten synced value");
  }

  // 3) Move the setting from unsynced to synced on this device but expect the
  //    already synced value to be honored.
  DevToolsSettings settings(&profile_);
  settings.Register("setting", {RegisterOptions::SyncMode::kSync});

  base::Value::Dict prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting"), "overwritten synced value");
}

TEST_F(DevToolsSettingsTest, Set_SetsTheUnderlyingTogglePreference) {
  DevToolsSettings settings(&profile_);
  settings.Register(DevToolsSettings::kSyncDevToolsPreferencesFrontendName,
                    {RegisterOptions::SyncMode::kSync});

  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "true");
  EXPECT_TRUE(profile_.GetPrefs()->GetBoolean(prefs::kDevToolsSyncPreferences));

  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "false");
  EXPECT_FALSE(
      profile_.GetPrefs()->GetBoolean(prefs::kDevToolsSyncPreferences));
}

TEST_F(DevToolsSettingsTest, Get_GetsTheUnderlyingTogglePreference) {
  DevToolsSettings settings(&profile_);
  settings.Register(DevToolsSettings::kSyncDevToolsPreferencesFrontendName,
                    {RegisterOptions::SyncMode::kSync});

  profile_.GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences, true);
  auto prefs = settings.Get();
  EXPECT_EQ(
      *prefs.FindString(DevToolsSettings::kSyncDevToolsPreferencesFrontendName),
      "true");

  profile_.GetPrefs()->SetBoolean(prefs::kDevToolsSyncPreferences, false);
  prefs = settings.Get();
  EXPECT_EQ(
      *prefs.FindString(DevToolsSettings::kSyncDevToolsPreferencesFrontendName),
      "false");
}

TEST_F(DevToolsSettingsTest, Remove_ResetsUnderlyingTogglePreference) {
  DevToolsSettings settings(&profile_);
  settings.Register(DevToolsSettings::kSyncDevToolsPreferencesFrontendName,
                    {RegisterOptions::SyncMode::kSync});
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "true");

  settings.Remove(DevToolsSettings::kSyncDevToolsPreferencesFrontendName);

  EXPECT_EQ(profile_.GetPrefs()->GetBoolean(prefs::kDevToolsSyncPreferences),
            DevToolsSettings::kSyncDevToolsPreferencesDefault);
}

TEST_F(DevToolsSettingsTest, Remove_WorksOnBothStorages) {
  {
    ScopedDictPrefUpdate synced_update(
        profile_.GetPrefs(), prefs::kDevToolsSyncedPreferencesSyncDisabled);
    synced_update->Set("unknown setting", "value");

    DevToolsSettings settings(&profile_);
    settings.Register("synced setting", {RegisterOptions::SyncMode::kSync});
    ScopedDictPrefUpdate unsynced_update(profile_.GetPrefs(),
                                         prefs::kDevToolsPreferences);
    unsynced_update->Set("synced setting", "value");
  }

  DevToolsSettings settings(&profile_);
  base::Value::Dict prefs = settings.Get();
  EXPECT_EQ(prefs.size(), static_cast<size_t>(3));
  settings.Remove("unknown setting");
  settings.Remove("synced setting");
  prefs = settings.Get();
  EXPECT_EQ(prefs.size(), static_cast<size_t>(1));
}

TEST_F(DevToolsSettingsTest, Clear_ResetsUnderlyingTogglePreference) {
  DevToolsSettings settings(&profile_);
  settings.Register(DevToolsSettings::kSyncDevToolsPreferencesFrontendName,
                    {RegisterOptions::SyncMode::kSync});
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "true");

  settings.Clear();

  EXPECT_EQ(profile_.GetPrefs()->GetBoolean(prefs::kDevToolsSyncPreferences),
            DevToolsSettings::kSyncDevToolsPreferencesDefault);
}

TEST_F(DevToolsSettingsTest, EnableDisableSyncPreservesSettings) {
  // 1) Enable sync and register settings
  DevToolsSettings settings(&profile_);
  settings.Register("setting_unsynced", {RegisterOptions::SyncMode::kDontSync});
  settings.Register("setting_synced", {RegisterOptions::SyncMode::kSync});
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "true");

  // 2) Set initial values
  settings.Set("setting_unsynced", "unsynced value");
  settings.Set("setting_synced", "synced value");

  // 3) Disable sync
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "false");

  base::Value::Dict prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting_unsynced"), "unsynced value");
  EXPECT_EQ(*prefs.FindString("setting_synced"), "synced value");
}

TEST_F(DevToolsSettingsTest, DisableEnableSyncPreservesSettings) {
  // 1) Disable sync and register settings
  DevToolsSettings settings(&profile_);
  settings.Register("setting_unsynced", {RegisterOptions::SyncMode::kDontSync});
  settings.Register("setting_synced", {RegisterOptions::SyncMode::kSync});
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "false");

  // 2) Set initial values
  settings.Set("setting_unsynced", "unsynced value");
  settings.Set("setting_synced", "synced value");

  // 3) Enable sync
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "true");

  base::Value::Dict prefs = settings.Get();
  EXPECT_EQ(*prefs.FindString("setting_unsynced"), "unsynced value");
  EXPECT_EQ(*prefs.FindString("setting_synced"), "synced value");
}

TEST_F(DevToolsSettingsTest, GetPreference) {
  // 1) Disable sync and register settings
  DevToolsSettings settings(&profile_);
  settings.Register("setting_unsynced", {RegisterOptions::SyncMode::kDontSync});
  settings.Register("setting_synced", {RegisterOptions::SyncMode::kSync});
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "false");
  EXPECT_EQ(
      settings.Get(DevToolsSettings::kSyncDevToolsPreferencesFrontendName)
          ->GetString(),
      "false");

  // 2) Set initial values
  settings.Set("setting_unsynced", "unsynced value");
  settings.Set("setting_synced", "synced value");

  // 3) Enable sync
  settings.Set(DevToolsSettings::kSyncDevToolsPreferencesFrontendName, "true");

  EXPECT_EQ(
      settings.Get(DevToolsSettings::kSyncDevToolsPreferencesFrontendName)
          ->GetString(),
      "true");
  EXPECT_EQ(settings.Get("setting_unsynced")->GetString(), "unsynced value");
  EXPECT_EQ(settings.Get("setting_synced")->GetString(), "synced value");
  EXPECT_EQ(settings.Get("nonexistent"), std::nullopt);
}
