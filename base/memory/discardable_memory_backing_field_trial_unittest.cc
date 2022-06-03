// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_internal.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include "base/memory/madv_free_discardable_memory_posix.h"
#endif  // defined(OS_POSIX)

#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
namespace base {

class DiscardableMemoryBackingFieldTrialTest : public ::testing::Test {
 protected:
  DiscardableMemoryBackingFieldTrialTest() = default;
  ~DiscardableMemoryBackingFieldTrialTest() override = default;

  std::unique_ptr<test::ScopedFeatureList>
  GetScopedFeatureListForDiscardableMemoryTrialGroup(
      DiscardableMemoryTrialGroup group) {
    auto feature_list = std::make_unique<test::ScopedFeatureList>();
    feature_list->InitAndEnableFeatureWithParameters(
        base::features::kDiscardableMemoryBackingTrial,
        {{features::kDiscardableMemoryBackingParam.name,
          features::kDiscardableMemoryBackingParamOptions[group].name}});
    return feature_list;
  }
};

TEST_F(DiscardableMemoryBackingFieldTrialTest, TrialActiveOnlyIfCapable) {
  std::unique_ptr<test::ScopedFeatureList> scoped_feature =
      GetScopedFeatureListForDiscardableMemoryTrialGroup(
          DiscardableMemoryTrialGroup::kEmulatedSharedMemory);
  FieldTrial* trial =
      FeatureList::GetFieldTrial(features::kDiscardableMemoryBackingTrial);
  ASSERT_NE(trial, nullptr);

  // Ensure the trial goes from disabled to enabled after querying state, if and
  // only if we are capable of running the trial. We have force enabled the
  // trial feature in the feature list, so |trial_enabled| implies that the
  // device is capable.
  EXPECT_FALSE(FieldTrialList::IsTrialActive(trial->trial_name()));
  bool trial_enabled = DiscardableMemoryBackingFieldTrialIsEnabled();
  EXPECT_EQ(trial_enabled, FieldTrialList::IsTrialActive(trial->trial_name()));
}

TEST_F(DiscardableMemoryBackingFieldTrialTest,
       EmulatedSharedMemoryBackingMatchesTrialGroup) {
  std::unique_ptr<test::ScopedFeatureList> scoped_feature =
      GetScopedFeatureListForDiscardableMemoryTrialGroup(
          DiscardableMemoryTrialGroup::kEmulatedSharedMemory);
  if (!DiscardableMemoryBackingFieldTrialIsEnabled())
    return;
  DiscardableMemoryBacking backing = GetDiscardableMemoryBacking();
  EXPECT_EQ(backing, DiscardableMemoryBacking::kSharedMemory);
}

TEST_F(DiscardableMemoryBackingFieldTrialTest,
       MadvFreeBackingMatchesTrialGroup) {
  std::unique_ptr<test::ScopedFeatureList> scoped_feature =
      GetScopedFeatureListForDiscardableMemoryTrialGroup(
          DiscardableMemoryTrialGroup::kMadvFree);
  if (!DiscardableMemoryBackingFieldTrialIsEnabled())
    return;
  DiscardableMemoryBacking backing = GetDiscardableMemoryBacking();
  EXPECT_EQ(backing, DiscardableMemoryBacking::kMadvFree);
}

#if defined(OS_ANDROID)
TEST_F(DiscardableMemoryBackingFieldTrialTest, AshmemBackingMatchesTrialGroup) {
  std::unique_ptr<test::ScopedFeatureList> scoped_feature =
      GetScopedFeatureListForDiscardableMemoryTrialGroup(
          DiscardableMemoryTrialGroup::kAshmem);
  if (!DiscardableMemoryBackingFieldTrialIsEnabled())
    return;
  DiscardableMemoryBacking backing = GetDiscardableMemoryBacking();
  EXPECT_EQ(backing, DiscardableMemoryBacking::kSharedMemory);
}
#endif  // defined(OS_ANDROID)

}  // namespace base

#endif  // defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_CHROMEOS)
