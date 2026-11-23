// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/security_settings_bundle_pref_change_handler.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/browser/safe_browsing/settings/generated_safe_browsing_pref.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/test/mock_toast_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_private = extensions::settings_private;

namespace safe_browsing {

class SecuritySettingsBundlePrefChangeHandlerTest : public testing::Test {
 public:
  SecuritySettingsBundlePrefChangeHandlerTest() {
    feature_list_.InitAndEnableFeature(kBundledSecuritySettings);
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    handler_ = std::make_unique<SecuritySettingsBundlePrefChangeHandler>(
        profile_.get());

    ON_CALL(toast_controller_, MaybeShowToast(testing::_))
        .WillByDefault(testing::Return(true));

    ON_CALL(mock_browser_, GetProfile())
        .WillByDefault(testing::Return(profile()));
    EXPECT_CALL(mock_browser_, RegisterBrowserDidClose(testing::_))
        .WillOnce([this](BrowserWindowInterface::BrowserDidCloseCallback cb) {
          close_callback_ = std::move(cb);
          return base::CallbackListSubscription();
        });
    BrowserManagerServiceFactory::GetForProfile(profile())
        ->AddBrowserForTesting(&mock_browser_);
  }

  void TearDown() override {
    if (close_callback_) {
      close_callback_.Run(&mock_browser_);
      close_callback_.Reset();
    }
    handler_.reset();
    profile_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }
  PrefService* prefs() { return profile()->GetTestingPrefService(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

 protected:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_;
  BrowserWindowInterface::BrowserDidCloseCallback close_callback_;
  std::unique_ptr<SecuritySettingsBundlePrefChangeHandler> handler_;
  base::test::ScopedFeatureList feature_list_;

  testing::NiceMock<MockToastController> toast_controller_{&mock_browser_};
};

TEST_F(SecuritySettingsBundlePrefChangeHandlerTest, NoToastWithoutBrowser) {
  close_callback_.Run(&mock_browser_);
  close_callback_.Reset();
  SetSecurityBundleSetting(*prefs(), SecuritySettingsBundleSetting::ENHANCED);

  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);
  handler_->MaybeShowEnhancedBundleSettingChangeNotification();
}

TEST_F(SecuritySettingsBundlePrefChangeHandlerTest,
       BundledSettingsToastNotShownWhenStandardBundleIsSelected) {
  // Set initial state.
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnhanced);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnabled);

  // Set up an observer for generated Safe Browsing preferences.
  auto pref = std::make_unique<GeneratedSafeBrowsingPref>(profile());
  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  // Set settings bundle value.
  SetSecurityBundleSetting(*prefs(), SecuritySettingsBundleSetting::STANDARD);

  // We expect 0 call because the pref is user set and not managed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedBundleSettingChangeNotification();
}

TEST_F(SecuritySettingsBundlePrefChangeHandlerTest,
       BundledSettingsToastShownWhenEnhancedBundleIsSelected) {
  // Set initial state.
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnhanced);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnabled);

  // Set up an observer for generated Safe Browsing preferences.
  auto pref = std::make_unique<GeneratedSafeBrowsingPref>(profile());
  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  // Set settings bundle value.
  SetSecurityBundleSetting(*prefs(), SecuritySettingsBundleSetting::ENHANCED);

  // We expect 1 call because the pref is user set and not managed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(1);

  handler_->MaybeShowEnhancedBundleSettingChangeNotification();
}

TEST_F(SecuritySettingsBundlePrefChangeHandlerTest,
       BundledSettingsToastNotShownWhenEnhancedBundleIsSelectedViaPolicy) {
  // Set initial state.
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnhanced);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnabled);

  // Set up an observer for generated Safe Browsing preferences.
  auto pref = std::make_unique<GeneratedSafeBrowsingPref>(profile());
  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  // Set settings bundle value via policy.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kSecuritySettingsBundle,
      base::Value(static_cast<int>(
          safe_browsing::SecuritySettingsBundleSetting::ENHANCED)));

  // We expect 1 call because the pref is user set and not managed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedBundleSettingChangeNotification();
}

TEST_F(SecuritySettingsBundlePrefChangeHandlerTest,
       BundledSettingsToastNotShownWhenEnhancedProtectionIsEnabled) {
  // Set initial state.
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnhanced);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnabled);

  // Set up an observer for generated Safe Browsing preferences.
  auto pref = std::make_unique<GeneratedSafeBrowsingPref>(profile());
  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  // Set settings bundle value.
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);

  // We expect 0 call because the pref is user set and not managed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedBundleSettingChangeNotification();
}

TEST_F(
    SecuritySettingsBundlePrefChangeHandlerTest,
    BundledSettingsToastShownWhenStandardBundleAndEnhancedProtectionUpgradesToEnhancedBundle) {
  // Set initial state.
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnhanced);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnabled);

  // Set up an observer for generated Safe Browsing preferences.
  auto pref = std::make_unique<GeneratedSafeBrowsingPref>(profile());
  settings_private::TestGeneratedPrefObserver test_observer;
  pref->AddObserver(&test_observer);

  // Select standard bundle and enhanced protection.
  SetSecurityBundleSetting(*prefs(), SecuritySettingsBundleSetting::STANDARD);
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);

  // We expect 0 call because the pref is user set and not managed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedBundleSettingChangeNotification();

  // Upgrade to enhanced bundle.
  SetSecurityBundleSetting(*prefs(), SecuritySettingsBundleSetting::ENHANCED);

  // We expect 1 call because the pref is user set and not managed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(1);

  handler_->MaybeShowEnhancedBundleSettingChangeNotification();
}
}  // namespace safe_browsing
