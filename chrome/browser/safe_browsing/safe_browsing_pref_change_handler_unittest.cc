// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_pref_change_handler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MockToastController : public ToastController {
 public:
  MockToastController() : ToastController(nullptr, nullptr) {}
  MOCK_METHOD(bool, MaybeShowToast, (ToastParams params), (override));
};

class SafeBrowsingPrefChangeHandlerTest : public BrowserWithTestWindowTest {
 public:
  SafeBrowsingPrefChangeHandlerTest() {
    feature_list_.InitAndEnableFeature(kEsbAsASyncedSetting);
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    handler_ = std::make_unique<SafeBrowsingPrefChangeHandler>(profile());

    ON_CALL(toast_controller_, MaybeShowToast(testing::_))
        .WillByDefault(testing::Return(true));

    handler_->SetToastControllerForTesting(&toast_controller_);
  }

  void TearDown() override {
    handler_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  std::unique_ptr<SafeBrowsingPrefChangeHandler> handler_;
  base::test::ScopedFeatureList feature_list_;

  // A mock controller to be injected into the production class.
  testing::NiceMock<MockToastController> toast_controller_;
};

TEST_F(SafeBrowsingPrefChangeHandlerTest,
       NoToastShownWhenEnhancedProtectionIsManaged) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));

  // We expect 0 calls because we do not trigger the toast controller for
  // managed users.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedProtectionSettingChangeNotification();
}

TEST_F(SafeBrowsingPrefChangeHandlerTest,
       NoToastShownWhenStandardProtectionIsManaged) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));

  // We expect 0 calls because we do not trigger the toast controller for
  // managed users.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedProtectionSettingChangeNotification();
}

TEST_F(SafeBrowsingPrefChangeHandlerTest,
       ToastShownWhenEnhancedProtectionIsSynced) {
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnhanced);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnabled);

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));

  // We expect 1 call because the pref is user set and not managed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(1);

  handler_->MaybeShowEnhancedProtectionSettingChangeNotification();
}
}  // namespace safe_browsing
