// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/profile_discard_opt_out_list_helper.h"

#include <map>

#include "base/json/values_util.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {
namespace {

const char kFirstProfileUniqueId[] = "profile1";
const char kSecondProfileUniqueId[] = "profile2";

class FakeProfileDiscardOptOutListHelperDelegate
    : public ProfileDiscardOptOutListHelper::Delegate {
 public:
  ~FakeProfileDiscardOptOutListHelperDelegate() override = default;

  void ClearPatterns(const std::string& browser_context_id) override {
    patterns_.erase(browser_context_id);
  }

  void SetPatterns(const std::string& browser_context_id,
                   const std::vector<std::string>& patterns) override {
    patterns_[browser_context_id] = patterns;
  }

  std::map<std::string, std::vector<std::string>> patterns_;
};

}  // namespace

class ProfileDiscardOptOutListHelperTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterDictionaryPref(
        performance_manager::user_tuning::prefs::
            kTabDiscardingExceptionsWithTime);
    prefs_.registry()->RegisterListPref(
        performance_manager::user_tuning::prefs::
            kManagedTabDiscardingExceptions);

    std::unique_ptr<FakeProfileDiscardOptOutListHelperDelegate> delegate =
        std::make_unique<FakeProfileDiscardOptOutListHelperDelegate>();
    delegate_ = delegate.get();
    helper_ =
        std::make_unique<ProfileDiscardOptOutListHelper>(std::move(delegate));

    AddProfile(kFirstProfileUniqueId, &prefs_);
  }

  void TearDown() override { RemoveProfile(kFirstProfileUniqueId); }

  void AddProfile(const std::string& profile_id, PrefService* prefs) {
    helper_->OnProfileAddedImpl(profile_id, prefs);
  }

  void RemoveProfile(const std::string& profile_id) {
    helper_->OnProfileWillBeRemovedImpl(profile_id);
  }

  TestingPrefServiceSimple prefs_;
  raw_ptr<FakeProfileDiscardOptOutListHelperDelegate, DanglingUntriaged>
      delegate_;
  std::unique_ptr<ProfileDiscardOptOutListHelper> helper_;
};

TEST_F(ProfileDiscardOptOutListHelperTest, TestUserSpecifiedList) {
  base::Value::Dict user_specified_values;
  user_specified_values.Set("foo", base::TimeToValue(base::Time::Now()));
  user_specified_values.Set("bar", base::TimeToValue(base::Time::Now()));

  prefs_.SetDict(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptionsWithTime,
      std::move(user_specified_values));

  EXPECT_EQ(1UL, delegate_->patterns_.size());
  EXPECT_THAT(delegate_->patterns_[kFirstProfileUniqueId],
              testing::UnorderedElementsAre("foo", "bar"));
}

TEST_F(ProfileDiscardOptOutListHelperTest, TestPolicySpecifiedList) {
  base::Value::List policy_specified_values;
  policy_specified_values.Append("foo");
  policy_specified_values.Append("bar");

  prefs_.SetList(
      performance_manager::user_tuning::prefs::kManagedTabDiscardingExceptions,
      std::move(policy_specified_values));

  EXPECT_EQ(1UL, delegate_->patterns_.size());
  EXPECT_THAT(delegate_->patterns_[kFirstProfileUniqueId],
              testing::UnorderedElementsAre("foo", "bar"));
}

TEST_F(ProfileDiscardOptOutListHelperTest,
       TestPolicyAndUserSpecifiedListsMerged) {
  base::Value::Dict user_specified_values;
  user_specified_values.Set("foo", base::TimeToValue(base::Time::Now()));
  user_specified_values.Set("bar", base::TimeToValue(base::Time::Now()));

  prefs_.SetDict(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptionsWithTime,
      std::move(user_specified_values));

  base::Value::List policy_specified_values;
  policy_specified_values.Append("baz");
  prefs_.SetList(
      performance_manager::user_tuning::prefs::kManagedTabDiscardingExceptions,
      std::move(policy_specified_values));

  EXPECT_EQ(1UL, delegate_->patterns_.size());
  EXPECT_THAT(delegate_->patterns_[kFirstProfileUniqueId],
              testing::UnorderedElementsAre("foo", "bar", "baz"));
}

TEST_F(ProfileDiscardOptOutListHelperTest, TestListsArePerProfile) {
  base::Value::Dict user_specified_values;
  user_specified_values.Set("foo", base::TimeToValue(base::Time::Now()));
  user_specified_values.Set("bar", base::TimeToValue(base::Time::Now()));

  prefs_.SetDict(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptionsWithTime,
      std::move(user_specified_values));

  // Add some exceptions to a first profile.
  EXPECT_EQ(1UL, delegate_->patterns_.size());
  EXPECT_THAT(delegate_->patterns_[kFirstProfileUniqueId],
              testing::UnorderedElementsAre("foo", "bar"));

  // Simulate adding a second profile and adding exceptions to it.
  TestingPrefServiceSimple other_prefs;
  other_prefs.registry()->RegisterDictionaryPref(
      performance_manager::user_tuning::prefs::
          kTabDiscardingExceptionsWithTime);
  other_prefs.registry()->RegisterListPref(
      performance_manager::user_tuning::prefs::kManagedTabDiscardingExceptions);
  AddProfile(kSecondProfileUniqueId, &other_prefs);

  base::Value::Dict other_user_specified_values;
  other_user_specified_values.Set("baz", base::TimeToValue(base::Time::Now()));
  other_prefs.SetDict(
      performance_manager::user_tuning::prefs::kTabDiscardingExceptionsWithTime,
      std::move(other_user_specified_values));

  // The delegate should have been notified of different patterns for the 2
  // profiles.
  EXPECT_EQ(2UL, delegate_->patterns_.size());
  EXPECT_THAT(delegate_->patterns_[kFirstProfileUniqueId],
              testing::UnorderedElementsAre("foo", "bar"));
  EXPECT_THAT(delegate_->patterns_[kSecondProfileUniqueId],
              testing::UnorderedElementsAre("baz"));

  RemoveProfile(kSecondProfileUniqueId);

  // Removing a profile clears the exceptions associated with it.
  EXPECT_EQ(1UL, delegate_->patterns_.size());
  EXPECT_THAT(delegate_->patterns_[kFirstProfileUniqueId],
              testing::UnorderedElementsAre("foo", "bar"));
}

}  // namespace performance_manager::user_tuning
