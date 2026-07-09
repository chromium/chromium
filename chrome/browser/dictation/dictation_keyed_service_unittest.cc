// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dictation {

class DictationKeyedServiceTest : public testing::Test {
 public:
  DictationKeyedServiceTest()
      : scoped_feature_list_(CreateEnablingFeatureList()),
        service_(std::make_unique<MockDictationKeyedService>(&profile_)) {
    profile_.GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
  }
  ~DictationKeyedServiceTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  tabs::MockTabInterface tab_;
  std::unique_ptr<MockDictationKeyedService> service_;
};

// Ending a non-existent session should not crash.
TEST_F(DictationKeyedServiceTest, EndSessionDoesNotCrash) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->EndSession();
}

TEST_F(DictationKeyedServiceTest, StartSessionWithNullTarget) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->StartSession(tab_, EmptyTargetId());
  EXPECT_NE(service_->session_controller(), nullptr);
}

TEST_F(DictationKeyedServiceTest, EndSessionRemovesController) {
  service_->StartSession(tab_, EmptyTargetId());
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->EndSession();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

}  // namespace dictation
