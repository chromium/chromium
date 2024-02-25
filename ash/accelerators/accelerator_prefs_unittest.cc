// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/accelerators/accelerator_prefs.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class TestShortcutPolicyObserver : public AcceleratorPrefs::Observer {
 public:
  TestShortcutPolicyObserver() = default;
  TestShortcutPolicyObserver(const TestShortcutPolicyObserver&) = delete;
  TestShortcutPolicyObserver& operator=(const TestShortcutPolicyObserver&) =
      delete;
  ~TestShortcutPolicyObserver() override = default;

  // AcceleratorPrefs::Observer:
  void OnShortcutPolicyUpdated() override { ++policy_changed_count; }
  int policy_changed_count = 0;
};

class AcceleratorPrefsTest : public AshTestBase {
 public:
  AcceleratorPrefsTest() = default;
  AcceleratorPrefsTest(const AcceleratorPrefsTest&) = delete;
  AcceleratorPrefsTest& operator=(const AcceleratorPrefsTest&) = delete;
  ~AcceleratorPrefsTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kShortcutCustomization);
    AshTestBase::SetUp();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AcceleratorPrefsTest, PrefsAreRegisitered) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kShortcutCustomizationAllowed));
}

TEST_F(AcceleratorPrefsTest, CustomizationAllowedUpdatesOnPolicyUpdated) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kShortcutCustomizationAllowed, true);
  AcceleratorPrefs* prefs_handler = Shell::Get()->accelerator_prefs();
  TestShortcutPolicyObserver observer;

  prefs_handler->AddObserver(&observer);
  EXPECT_EQ(0, observer.policy_changed_count);

  prefs->SetBoolean(prefs::kShortcutCustomizationAllowed, false);
  EXPECT_EQ(1, observer.policy_changed_count);

  prefs_handler->RemoveObserver(&observer);
}

}  // namespace ash
