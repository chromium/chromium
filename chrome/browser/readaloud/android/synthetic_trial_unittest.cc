// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/android/synthetic_trial.h"
#include <memory>
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_metrics_service_for_synthetic_trials.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/active_field_trials.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome::android::kReadAloud;

namespace readaloud {
namespace {

// Base trial name and group.
inline constexpr char kBaseTrial[] = "TestTrial";
inline constexpr char kBaseGroup[] = "TestGroup";

inline constexpr char kSuffix[] = "_Suffix";

// Synthetic trial name is the same as the base trial name plus a suffix.
inline constexpr char kSyntheticTrialName[] = "TestTrial_Suffix";

inline constexpr char kPrefKey[] = "ReadAloud|||_Suffix";

}  // namespace

class SyntheticTrialTest : public ::testing::Test {
 public:
  SyntheticTrialTest() = default;

  SyntheticTrialTest(const SyntheticTrialTest&) = delete;
  SyntheticTrialTest& operator=(const SyntheticTrialTest&) = delete;

  void EnableReadAloudWithTrial(const std::string& base_trial,
                                const std::string& base_group) {
    auto feature_list = std::make_unique<base::FeatureList>();
    base::FieldTrial* trial =
        base::FieldTrialList::CreateFieldTrial(base_trial, base_group);
    feature_list->RegisterFieldTrialOverride(
        kReadAloud.name,
        base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE, trial);
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_{
      TestingBrowserProcess::GetGlobal()};
  ScopedMetricsServiceForSyntheticTrials metrics_service_provider_{
      TestingBrowserProcess::GetGlobal()};
};

TEST_F(SyntheticTrialTest, TestNoBaseStudy) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));

  // Try init with no field trial set on kReadAloud.
  auto synth = SyntheticTrial::Create(kReadAloud.name, kSuffix);
  EXPECT_EQ(nullptr, synth.get());

  // Synthetic trial wasn't activated on init.
  EXPECT_FALSE(variations::HasSyntheticTrial(kSyntheticTrialName));
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));
}

TEST_F(SyntheticTrialTest, TestActivate) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));

  EnableReadAloudWithTrial(kBaseTrial, kBaseGroup);
  auto synth = SyntheticTrial::Create(kReadAloud.name, kSuffix);
  EXPECT_FALSE(variations::HasSyntheticTrial(kSyntheticTrialName));
  EXPECT_EQ(nullptr, local_state()
                         ->GetDict(prefs::kReadAloudSyntheticTrials)
                         .FindString(kSyntheticTrialName));

  synth->Activate();
  // Synthetic trial is activated.
  EXPECT_TRUE(variations::HasSyntheticTrial(kSyntheticTrialName));
  EXPECT_TRUE(
      variations::IsInSyntheticTrialGroup(kSyntheticTrialName, kBaseGroup));
  // Synthetic trial and group should be saved for reactivation.
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));
  EXPECT_EQ(kBaseTrial, *local_state()
                             ->GetDict(prefs::kReadAloudSyntheticTrials)
                             .FindString(kPrefKey));
}

TEST_F(SyntheticTrialTest, TestReactivatePreviouslyActive) {
  // Set up the reactivation pref.
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));
  {
    ScopedDictPrefUpdate(local_state(), prefs::kReadAloudSyntheticTrials)
        ->Set(kPrefKey, kBaseTrial);
  }
  EXPECT_FALSE(variations::HasSyntheticTrial(kSyntheticTrialName));

  // Base trial and group agree with the reactivation pref, so creating
  // SyntheticTrial should reactivate.
  EnableReadAloudWithTrial(kBaseTrial, kBaseGroup);
  auto synth = SyntheticTrial::Create(kReadAloud.name, kSuffix);
  EXPECT_TRUE(variations::HasSyntheticTrial(kSyntheticTrialName));
  EXPECT_TRUE(
      variations::IsInSyntheticTrialGroup(kSyntheticTrialName, kBaseGroup));
}

TEST_F(SyntheticTrialTest, TestNotActivatedIfTrialNameChanged) {
  // Set up the reactivation pref.
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));
  {
    ScopedDictPrefUpdate(local_state(), prefs::kReadAloudSyntheticTrials)
        ->Set(kPrefKey, kBaseTrial);
  }
  EXPECT_FALSE(variations::HasSyntheticTrial(kSyntheticTrialName));

  // A different trial is controlling the flag so the synthetic trial should not
  // reactivate.
  EnableReadAloudWithTrial("SomeOtherTrial", kBaseGroup);
  auto synth = SyntheticTrial::Create(kReadAloud.name, kSuffix);
  EXPECT_FALSE(variations::HasSyntheticTrial(kSyntheticTrialName));

  // Subsequent Activate() should activate the synthetic trial with the new
  // name.
  synth->Activate();
  EXPECT_TRUE(variations::HasSyntheticTrial("SomeOtherTrial_Suffix"));
  EXPECT_TRUE(
      variations::IsInSyntheticTrialGroup("SomeOtherTrial_Suffix", kBaseGroup));
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));
  EXPECT_EQ("SomeOtherTrial", *local_state()
                                   ->GetDict(prefs::kReadAloudSyntheticTrials)
                                   .FindString(kPrefKey));
}

TEST_F(SyntheticTrialTest, TestTwoSyntheticTrials) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));

  EnableReadAloudWithTrial(kBaseTrial, kBaseGroup);
  auto synth = SyntheticTrial::Create(kReadAloud.name, kSuffix);
  auto synth2 = SyntheticTrial::Create(kReadAloud.name, "_SomeOtherSuffix");

  synth->Activate();
  synth2->Activate();

  // Both trials should be active.
  EXPECT_TRUE(variations::HasSyntheticTrial(kSyntheticTrialName));
  EXPECT_TRUE(
      variations::IsInSyntheticTrialGroup(kSyntheticTrialName, kBaseGroup));
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));
  EXPECT_EQ(kBaseTrial, *local_state()
                             ->GetDict(prefs::kReadAloudSyntheticTrials)
                             .FindString(kPrefKey));

  EXPECT_TRUE(variations::HasSyntheticTrial("TestTrial_SomeOtherSuffix"));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup("TestTrial_SomeOtherSuffix",
                                                  kBaseGroup));
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));
  EXPECT_EQ(kBaseTrial, *local_state()
                             ->GetDict(prefs::kReadAloudSyntheticTrials)
                             .FindString("ReadAloud|||_SomeOtherSuffix"));
}

TEST_F(SyntheticTrialTest, TestClearStalePrefs) {
  // Set a few reactivation signals.
  {
    ScopedDictPrefUpdate update(local_state(),
                                prefs::kReadAloudSyntheticTrials);
    update->Set("aaa|||_suffix1", "aaatrial");
    update->Set("bbb|||_suffix2", "bbbtrial");
    update->Set("ccc|||_suffix3", "ccctrial");
  }
  auto feature_list = std::make_unique<base::FeatureList>();

  // Feature "aaa" remains associated with trial "aaatrial".
  feature_list->RegisterFieldTrialOverride(
      "aaa", base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
      base::FieldTrialList::CreateFieldTrial("aaatrial", "group"));

  // Feature "bbb" is now associated with a different trial.
  feature_list->RegisterFieldTrialOverride(
      "bbb", base::FeatureList::OverrideState::OVERRIDE_ENABLE_FEATURE,
      base::FieldTrialList::CreateFieldTrial("asdfasdf", "group"));

  // Feature "ccc" is not associated with any trial.

  scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

  // Clear.
  SyntheticTrial::ClearStalePrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kReadAloudSyntheticTrials));

  // "aaatrial" should remain.
  const base::Value::Dict& dict =
      local_state()->GetDict(prefs::kReadAloudSyntheticTrials);
  const std::string* aaa_trial_name = dict.FindString("aaa|||_suffix1");
  EXPECT_TRUE(aaa_trial_name != nullptr);
  EXPECT_EQ("aaatrial", *aaa_trial_name);

  // "bbbtrial" and "ccctrial" should be gone.
  EXPECT_EQ(nullptr, dict.FindString("bbb|||_suffix2"));
  EXPECT_EQ(nullptr, dict.FindString("ccc|||_suffix3"));
}

}  // namespace readaloud
