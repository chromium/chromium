// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_prefs_merge_conflict_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {

namespace {

// Batch 1 prefs.
static constexpr const char* kBatch1_LockablePref =
    prefs::kAccessibilityHighContrastEnabled;
static constexpr const char* kBatch1_NonLockablePref =
    prefs::kAccessibilityColorCorrectionEnabled;

// Batch 3 prefs.
static constexpr const char* kBatch3_LockablePref =
    prefs::kAccessibilitySelectToSpeakEnabled;
static constexpr const char* kBatch3_LockablePref_NoDialog =
    prefs::kDockedMagnifierScale;

using PrefConflict = AccessibilityPrefsMergeConflictController::PrefConflict;

}  // namespace

class AccessibilityPrefsMergeConflictControllerTest : public AshTestBase {
 public:
  AccessibilityPrefsMergeConflictControllerTest() = default;
};

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       BuildConflictsDetectsDifferences) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kOsSyncAccessibilitySettingsBatch3);

  base::DictValue locked;
  base::DictValue pending;

  locked.Set(kBatch3_LockablePref, base::Value(true));
  pending.Set(kBatch3_LockablePref, base::Value(false));

  auto conflicts =
      AccessibilityPrefsMergeConflictController::BuildConflictsForTest(
          std::move(locked), std::move(pending));

  ASSERT_EQ(conflicts.size(), 1u);
  EXPECT_EQ(conflicts[0].pref_name, kBatch3_LockablePref);
  EXPECT_TRUE(conflicts[0].local_value.GetBool());
  EXPECT_FALSE(conflicts[0].pending_value.GetBool());
}

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       BuildConflictsNoConflictWhenValuesAreEqual) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kOsSyncAccessibilitySettingsBatch3);

  base::DictValue locked;
  base::DictValue pending;

  locked.Set(kBatch3_LockablePref, true);
  pending.Set(kBatch3_LockablePref, true);

  auto conflicts =
      AccessibilityPrefsMergeConflictController::BuildConflictsForTest(
          std::move(locked), std::move(pending));

  EXPECT_TRUE(conflicts.empty());
}

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       BuildConflictsIgnoresNonDialogPrefs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kOsSyncAccessibilitySettingsBatch1);

  base::DictValue locked;
  base::DictValue pending;

  locked.Set(kBatch1_NonLockablePref, true);
  pending.Set(kBatch1_NonLockablePref, false);

  auto conflicts =
      AccessibilityPrefsMergeConflictController::BuildConflictsForTest(
          std::move(locked), std::move(pending));

  EXPECT_TRUE(conflicts.empty());
}

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       BuildConflictsWithEmptyInputsReturnsEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kOsSyncAccessibilitySettingsBatch1);

  base::DictValue locked;
  base::DictValue pending;

  auto conflicts =
      AccessibilityPrefsMergeConflictController::BuildConflictsForTest(
          std::move(locked), std::move(pending));

  EXPECT_TRUE(conflicts.empty());
}

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       BuildConflictsUsesDefaultFalseWhenPrefMissing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kOsSyncAccessibilitySettingsBatch3);

  base::DictValue locked;
  base::DictValue pending;

  // Only pending has the pref.
  pending.Set(kBatch3_LockablePref, true);

  auto conflicts =
      AccessibilityPrefsMergeConflictController::BuildConflictsForTest(
          std::move(locked), std::move(pending));

  ASSERT_EQ(conflicts.size(), 1u);
  EXPECT_FALSE(conflicts[0].local_value.GetBool());  // default
  EXPECT_TRUE(conflicts[0].pending_value.GetBool());
}

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       UpdateConflictsUpdatesPrefAndState) {
  // Feature enabled not needed.
  std::vector<PrefConflict> conflicts;
  conflicts.emplace_back(kBatch1_LockablePref, base::Value(true),
                         base::Value(false));
  conflicts.emplace_back(kBatch3_LockablePref, base::Value(false),
                         base::Value(true));
  conflicts.emplace_back(kBatch3_LockablePref_NoDialog, base::Value(0.),
                         base::Value(1.));

  auto controller = AccessibilityPrefsMergeConflictController::CreateForTest(
      std::move(conflicts));
  controller->UpdateConflict(kBatch1_LockablePref, base::Value(false));
  controller->UpdateConflict(kBatch3_LockablePref, base::Value(true));
  controller->UpdateConflict(kBatch3_LockablePref_NoDialog, base::Value(2.));

  auto* prefs = Shell::Get()->accessibility_controller()->GetActiveUserPrefs();
  EXPECT_FALSE(prefs->GetBoolean(kBatch1_LockablePref));
  EXPECT_TRUE(prefs->GetBoolean(kBatch3_LockablePref));
  EXPECT_EQ(prefs->GetValue(kBatch3_LockablePref_NoDialog), base::Value(2.));
}

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       UpdateConflictUnknownPrefDies) {
  // Feature enabled not needed.
  std::vector<PrefConflict> conflicts = {};

  auto controller = AccessibilityPrefsMergeConflictController::CreateForTest(
      std::move(conflicts));

  EXPECT_DEATH(
      controller->UpdateConflict("nonexistent_pref", base::Value(true)), "");
}

TEST_F(AccessibilityPrefsMergeConflictControllerTest,
       ConstructorReportsPrefChanged) {
  // Feature enabled not needed.
  auto* prefs = Shell::Get()->accessibility_controller()->GetActiveUserPrefs();

  bool notified = false;

  PrefChangeRegistrar registrar;
  registrar.Init(prefs);
  registrar.Add(kBatch1_LockablePref,
                base::BindLambdaForTesting([&]() { notified = true; }));

  std::vector<PrefConflict> conflicts;
  conflicts.emplace_back(kBatch1_LockablePref, base::Value(true),
                         base::Value(false));

  auto controller = AccessibilityPrefsMergeConflictController::CreateForTest(
      std::move(conflicts));

  EXPECT_TRUE(notified);
}

}  // namespace ash
