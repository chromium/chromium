// Copyright 2021 The Chromium Authors. All rights reserved.
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

  settings.Set("setting_a", "foo");
  settings.Set("setting_b", "bar");

  const auto* prefs = settings.Get();
  EXPECT_EQ(*prefs->FindStringKey("setting_a"), "foo");
  EXPECT_EQ(*prefs->FindStringKey("setting_b"), "bar");

  settings.Remove("setting_a");
  prefs = settings.Get();
  EXPECT_EQ(prefs->FindStringKey("setting_a"), nullptr);

  settings.Clear();
  prefs = settings.Get();
  EXPECT_EQ(prefs->DictSize(), static_cast<size_t>(0));
}
