// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/profile_force_foreground_priority_list_helper.h"

#include <map>
#include <memory>

#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::user_tuning {

namespace {

class MockDelegate : public ProfileForceForegroundPriorityListHelper::Delegate {
 public:
  MOCK_METHOD(void,
              SetPatterns,
              (const std::string&, const base::ListValue&),
              (override));
  MOCK_METHOD(void, ClearPatterns, (const std::string&), (override));
};

class ProfileForceForegroundPriorityListHelperTest : public ::testing::Test {
 public:
  ProfileForceForegroundPriorityListHelperTest() {
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    helper_ = std::make_unique<ProfileForceForegroundPriorityListHelper>(
        std::move(delegate));
  }

  ~ProfileForceForegroundPriorityListHelperTest() override = default;

  void RegisterPrefs(TestingPrefServiceSimple* prefs) {
    prefs->registry()->RegisterListPref(
        performance_manager::user_tuning::prefs::
            kForceForegroundPriorityForUrls);
  }

 protected:
  std::unique_ptr<ProfileForceForegroundPriorityListHelper> helper_;
  raw_ptr<MockDelegate> delegate_;
};

}  // namespace

TEST_F(ProfileForceForegroundPriorityListHelperTest, OriginsPref) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(&prefs);

  // Initially empty list should trigger a SetPatterns with empty list.
  EXPECT_CALL(*delegate_,
              SetPatterns("", ::testing::Property(&base::ListValue::size, 0)));
  helper_->OnProfileAddedImpl("", &prefs);

  // Add origin.
  base::ListValue patterns;
  patterns.Append("google.com");
  EXPECT_CALL(*delegate_,
              SetPatterns("", ::testing::Property(&base::ListValue::size, 1)));
  prefs.SetList(performance_manager::user_tuning::prefs::
                    kForceForegroundPriorityForUrls,
                patterns.Clone());

  // Removing profile should clear patterns.
  EXPECT_CALL(*delegate_, ClearPatterns(""));
  helper_->OnProfileWillBeRemovedImpl("");
}

}  // namespace performance_manager::user_tuning
