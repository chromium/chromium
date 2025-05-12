// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/prefs/persistent_renderer_prefs_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestPersistentRendererPrefsManager : PersistentRendererPrefsManager {
 public:
  explicit TestPersistentRendererPrefsManager(PrefService& pref_service)
      : PersistentRendererPrefsManager(pref_service) {}
  void TestSetViewSourceLineWrapping(bool value) {
    SetViewSourceLineWrapping(value);
  }
};

// Observe that changes made through the persistent renderer prefs service are
// reflected in the profile backing it.
TEST(PersistentRendererPrefsTest, ObservePrefChanges) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  TestPersistentRendererPrefsManager persistent_renderer_prefs_manager(
      *profile.GetPrefs());

  EXPECT_FALSE(
      profile.GetPrefs()->GetBoolean(prefs::kViewSourceLineWrappingEnabled));

  persistent_renderer_prefs_manager.TestSetViewSourceLineWrapping(true);
  EXPECT_TRUE(
      profile.GetPrefs()->GetBoolean(prefs::kViewSourceLineWrappingEnabled));
  persistent_renderer_prefs_manager.TestSetViewSourceLineWrapping(false);
  EXPECT_FALSE(
      profile.GetPrefs()->GetBoolean(prefs::kViewSourceLineWrappingEnabled));
}
