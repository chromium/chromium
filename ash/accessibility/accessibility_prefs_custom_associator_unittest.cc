// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_prefs_custom_associator.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/live_caption/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Pref that's not part of any batch or requires custom merge.
static constexpr char kNonCustomPref[] = "some.random.pref";

// Batch 1 prefs.
static constexpr const char* kBatch1_LockablePref =
    prefs::kAccessibilityHighContrastEnabled;
static constexpr const char* kBatch1_NonLockablePref =
    prefs::kAccessibilityLargeCursorDipSize;

// Batch 2 prefs - it contains no lockable prefs.
static constexpr const char* kBatch2_NonLockablePref =
    prefs::kAccessibilityReducedAnimationsEnabled;

// Batch 3 prefs.
static constexpr const char* kBatch3_LockablePref =
    prefs::kAccessibilityScreenMagnifierEnabled;
static constexpr const char* kBatch3_NonLockablePref =
    prefs::kAccessibilityScreenMagnifierScale;

struct BatchParams {
  raw_ptr<const base::Feature> feature;
  const char* lockable_pref;
  const char* non_lockable_pref;
};

// Used when the server value is irrelevant to the test.
const base::Value kIrrelevantServerValue;

}  // namespace

class AccessibilityPrefsCustomAssociatorTest : public testing::Test {
 protected:
  void SetUp() override {
    associator_ = std::make_unique<AccessibilityPrefsCustomAssociator>();
  }

  void TearDown() override { associator_.reset(); }

  std::optional<base::Value> GetMergeValue(const std::string& pref_name,
                                           const base::Value& server) {
    return associator_->GetPreferredPrefMergeValue(pref_name, server.Clone());
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<AccessibilityPrefsCustomAssociator> associator_;
};

// Verifies that non-custom prefs return std::nullopt.
TEST_F(AccessibilityPrefsCustomAssociatorTest,
       GetPreferredPrefMergeValue_NonCustomMergeBehaviorPref) {
  std::optional<base::Value> merge_value =
      GetMergeValue(kNonCustomPref, kIrrelevantServerValue);
  EXPECT_FALSE(merge_value.has_value());
}

// Verifies that locking a pref without custom merge behavior has no effect.
TEST_F(AccessibilityPrefsCustomAssociatorTest,
       TryLockPref_NonCustomMergeBehaviorPref) {
  base::Value value(true);
  associator_->TryLockPref(kNonCustomPref, value);

  EXPECT_FALSE(
      GetMergeValue(kNonCustomPref, kIrrelevantServerValue).has_value());
}

class AccessibilityPrefsCustomAssociatorParameterizedTest
    : public AccessibilityPrefsCustomAssociatorTest,
      public testing::WithParamInterface<BatchParams> {
 protected:
  AccessibilityPrefsCustomAssociatorParameterizedTest() {
    feature_list_.InitAndEnableFeature(*GetParam().feature);
  }

  const char* lockable_pref() const { return GetParam().lockable_pref; }
  const char* non_lockable_pref() const { return GetParam().non_lockable_pref; }
};

// Verifies that an unlocked lockable pref prefers the server value.
TEST_P(AccessibilityPrefsCustomAssociatorParameterizedTest,
       GetPreferredPrefMergeValue_UnlockedPref) {
  base::Value server_value(false);

  std::optional<base::Value> merge_value =
      GetMergeValue(lockable_pref(), server_value);
  EXPECT_EQ(*merge_value, server_value);
}

// Verifies that a locked pref prefers the locally locked value.
TEST_P(AccessibilityPrefsCustomAssociatorParameterizedTest,
       GetPreferredPrefMergeValue_LockedPref) {
  base::Value local_value(false);
  associator_->TryLockPref(lockable_pref(), local_value);

  std::optional<base::Value> merge_value =
      GetMergeValue(lockable_pref(), kIrrelevantServerValue);
  EXPECT_EQ(*merge_value, local_value);
}

// Verifies that locking a pref twice updates the locked value.
TEST_P(AccessibilityPrefsCustomAssociatorParameterizedTest,
       TryLockPref_UpdatesValue) {
  base::Value value1(true);
  base::Value value2(false);

  associator_->TryLockPref(lockable_pref(), value1);
  associator_->TryLockPref(lockable_pref(), value2);

  std::optional<base::Value> merge_value =
      GetMergeValue(lockable_pref(), kIrrelevantServerValue);
  EXPECT_EQ(*merge_value, value2);
}

// Verifies that non-lockable prefs cannot be locked.
TEST_P(AccessibilityPrefsCustomAssociatorParameterizedTest,
       TryLockPref_IntegerPrefCantLock) {
  base::Value value(123);
  associator_->TryLockPref(non_lockable_pref(), value);

  EXPECT_FALSE(
      GetMergeValue(non_lockable_pref(), kIrrelevantServerValue).has_value());
}

// Verifies that unlocking an already unlocked pref is a no-op.
TEST_P(AccessibilityPrefsCustomAssociatorParameterizedTest,
       UnlockPref_AlreadyUnlocked) {
  base::Value server_value(true);

  std::optional<base::Value> merge_value =
      GetMergeValue(lockable_pref(), server_value);
  EXPECT_EQ(*merge_value, server_value);

  associator_->UnlockPref(lockable_pref());

  merge_value = GetMergeValue(lockable_pref(), server_value);
  EXPECT_EQ(*merge_value, server_value);
}

// Verifies full lock / unlock lifecycle behavior.
TEST_P(AccessibilityPrefsCustomAssociatorParameterizedTest, LockUnlockCycle) {
  base::Value local_value(true);
  base::Value server_value(false);

  std::optional<base::Value> merge_value =
      GetMergeValue(lockable_pref(), server_value);
  EXPECT_EQ(*merge_value, server_value);

  associator_->TryLockPref(lockable_pref(), local_value);
  merge_value = GetMergeValue(lockable_pref(), server_value);
  EXPECT_EQ(*merge_value, local_value);

  associator_->UnlockPref(lockable_pref());
  merge_value = GetMergeValue(lockable_pref(), server_value);
  EXPECT_EQ(*merge_value, server_value);
}

INSTANTIATE_TEST_SUITE_P(
    Batch1AndBatch3,
    AccessibilityPrefsCustomAssociatorParameterizedTest,
    testing::Values(BatchParams{&features::kOsSyncAccessibilitySettingsBatch1,
                                kBatch1_LockablePref, kBatch1_NonLockablePref},
                    BatchParams{&features::kOsSyncAccessibilitySettingsBatch3,
                                kBatch3_LockablePref,
                                kBatch3_NonLockablePref}));

class AccessibilityPrefsCustomAssociatorTest_Batch2
    : public AccessibilityPrefsCustomAssociatorTest {
 protected:
  AccessibilityPrefsCustomAssociatorTest_Batch2() {
    feature_list_.InitAndEnableFeature(
        features::kOsSyncAccessibilitySettingsBatch2);
  }
};

// Verifies that Batch2 prefs cannot be locked.
TEST_F(AccessibilityPrefsCustomAssociatorTest_Batch2, TryLockPref) {
  base::Value value(true);

  associator_->TryLockPref(kBatch2_NonLockablePref, value);

  std::optional<base::Value> merge_value =
      GetMergeValue(kBatch2_NonLockablePref, kIrrelevantServerValue);
  EXPECT_FALSE(merge_value.has_value());
}

class AccessibilityPrefsCustomAssociatorTest_Batches1_2_3
    : public AccessibilityPrefsCustomAssociatorTest {
 protected:
  AccessibilityPrefsCustomAssociatorTest_Batches1_2_3() {
    feature_list_.InitWithFeatures(
        {features::kOsSyncAccessibilitySettingsBatch1,
         features::kOsSyncAccessibilitySettingsBatch2,
         features::kOsSyncAccessibilitySettingsBatch3},
        {});
  }
};

// Verifies locking behavior across multiple batches simultaneously.
TEST_F(AccessibilityPrefsCustomAssociatorTest_Batches1_2_3,
       MultiplePrefsAcrossBatches) {
  base::Value value1(true);
  base::Value value2(42);
  base::Value value3(true);

  associator_->TryLockPref(kBatch1_LockablePref, value1);
  associator_->TryLockPref(kBatch2_NonLockablePref, value2);
  associator_->TryLockPref(kBatch3_LockablePref, value3);

  std::optional<base::Value> merge_value =
      GetMergeValue(kBatch1_LockablePref, kIrrelevantServerValue);
  EXPECT_EQ(*merge_value, value1);

  merge_value = GetMergeValue(kBatch2_NonLockablePref, kIrrelevantServerValue);
  EXPECT_FALSE(merge_value.has_value());

  merge_value = GetMergeValue(kBatch3_LockablePref, kIrrelevantServerValue);
  EXPECT_EQ(*merge_value, value3);

  associator_->UnlockPref(kBatch1_LockablePref);

  merge_value = GetMergeValue(kBatch1_LockablePref, base::Value(false));
  EXPECT_EQ(*merge_value, base::Value(false));

  merge_value = GetMergeValue(kBatch3_LockablePref, kIrrelevantServerValue);
  EXPECT_EQ(*merge_value, value3);
}

class AccessibilityPrefsCustomAssociatorDisabledParameterizedTest
    : public AccessibilityPrefsCustomAssociatorTest,
      public testing::WithParamInterface<BatchParams> {
 protected:
  AccessibilityPrefsCustomAssociatorDisabledParameterizedTest() {
    feature_list_.InitAndDisableFeature(*GetParam().feature);
  }

  const char* lockable_pref() const { return GetParam().lockable_pref; }
  const char* non_lockable_pref() const { return GetParam().non_lockable_pref; }
};

// Verifies that no prefs can be locked when the feature is disabled.
TEST_P(AccessibilityPrefsCustomAssociatorDisabledParameterizedTest,
       TryLockPref) {
  base::Value value(true);

  {
    associator_->TryLockPref(non_lockable_pref(), value);

    std::optional<base::Value> merge_value =
        GetMergeValue(non_lockable_pref(), kIrrelevantServerValue);
    EXPECT_FALSE(merge_value.has_value());
  }

  {
    associator_->TryLockPref(lockable_pref(), value);

    std::optional<base::Value> merge_value =
        GetMergeValue(lockable_pref(), kIrrelevantServerValue);
    EXPECT_FALSE(merge_value.has_value());
  }
}

INSTANTIATE_TEST_SUITE_P(
    Batch1AndBatch3Disabled,
    AccessibilityPrefsCustomAssociatorDisabledParameterizedTest,
    testing::Values(BatchParams{&features::kOsSyncAccessibilitySettingsBatch1,
                                kBatch1_LockablePref, kBatch1_NonLockablePref},
                    BatchParams{&features::kOsSyncAccessibilitySettingsBatch3,
                                kBatch3_LockablePref,
                                kBatch3_NonLockablePref}));

}  // namespace ash
