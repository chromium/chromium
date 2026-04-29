// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_pref_change_handler.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MockTailoredSecurityService : public TailoredSecurityService {
 public:
  MockTailoredSecurityService()
      : TailoredSecurityService(nullptr, nullptr, nullptr) {}
  ~MockTailoredSecurityService() override = default;

  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
};

class MockToastController : public ToastController {
 public:
  MockToastController() : ToastController(nullptr, nullptr) {}
  MOCK_METHOD(bool, MaybeShowToast, (ToastParams params), (override));
};

class SafeBrowsingPrefChangeHandlerTest : public testing::Test {
 public:
  SafeBrowsingPrefChangeHandlerTest() = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          auto test_sync_service = std::make_unique<syncer::TestSyncService>();
          test_sync_service->SetSignedOut();
          return test_sync_service;
        }));

    builder.AddTestingFactory(
        TailoredSecurityServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockTailoredSecurityService>>();
        }));
    profile_ = builder.Build();

    handler_ = std::make_unique<SafeBrowsingPrefChangeHandler>(profile_.get());

    ON_CALL(toast_controller_, MaybeShowToast(testing::_))
        .WillByDefault(testing::Return(true));

    handler_->SetToastControllerForTesting(&toast_controller_);

    // Setup mock browser
    EXPECT_CALL(mock_browser_, RegisterDidBecomeActive(testing::_))
        .WillOnce(testing::Return(base::CallbackListSubscription()));
    EXPECT_CALL(mock_browser_, RegisterDidBecomeInactive(testing::_))
        .WillOnce(testing::Return(base::CallbackListSubscription()));
    EXPECT_CALL(mock_browser_, RegisterBrowserDidClose(testing::_))
        .WillOnce([this](BrowserWindowInterface::BrowserDidCloseCallback cb) {
          close_callback_ = std::move(cb);
          return base::CallbackListSubscription();
        });
    EXPECT_CALL(mock_browser_, IsDeleteScheduled())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(mock_browser_, GetType())
        .WillRepeatedly(testing::Return(BrowserWindowInterface::TYPE_NORMAL));
    EXPECT_CALL(mock_browser_, GetTabStripModel())
        .WillRepeatedly(testing::Return(&tab_strip_model_));
    EXPECT_CALL(mock_browser_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));

    // Register mock browser
    BrowserManagerServiceFactory::GetForProfile(profile())
        ->AddBrowserForTesting(&mock_browser_);
  }

  void TearDown() override {
    if (close_callback_) {
      close_callback_.Run(&mock_browser_);
    }
    handler_.reset();
    profile_.reset();
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }

  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  MockTailoredSecurityService* tailored_security_service() {
    return static_cast<MockTailoredSecurityService*>(
        TailoredSecurityServiceFactory::GetForProfile(profile()));
  }

  void SetSignedIn() {
    signin::ConsentLevel consent_level =
        syncer::IsReplaceSyncPromosWithSignInPromosEnabled()
            ? signin::ConsentLevel::kSignin
            : signin::ConsentLevel::kSync;
    sync_service()->SetSignedIn(consent_level);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  TabStripModel tab_strip_model_{&tab_strip_model_delegate_, nullptr};

  BrowserWindowInterface::BrowserDidCloseCallback close_callback_;

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

TEST_F(SafeBrowsingPrefChangeHandlerTest,
       BundledSettingsToastNotShownWhenEnhancedProtectionIsEnabled) {
  // Set initial state.
  feature_list_.InitAndEnableFeature(kBundledSecuritySettings);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnhanced);
  profile()->GetTestingPrefService()->ClearPref(prefs::kSafeBrowsingEnabled);

  // Enable enhanced safe browsing.
  SetSafeBrowsingState(profile()->GetTestingPrefService(),
                       SafeBrowsingState::ENHANCED_PROTECTION);

  // We expect 0 calls because the enhanced bundle was not selected.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedProtectionSettingChangeNotification();
}

// This test simulates a scenario where a user enables ESB on a different
// device, and then signs into a new device for the first time. In this case,
// we don't want to show the toast because the Tailored Security flow hasn't
// run locally yet.
TEST_F(SafeBrowsingPrefChangeHandlerTest,
       ToastSuppressedForStalePrefOnInitialSync) {
  SetSignedIn();
  profile()->GetTestingPrefService()->SetTime(
      prefs::kAccountTailoredSecurityUpdateTimestamp, base::Time());

  profile()->GetTestingPrefService()->ClearPref(
      prefs::kAccountTailoredSecurityUpdateTimestamp);
  ASSERT_TRUE(profile()
                  ->GetPrefs()
                  ->GetTime(prefs::kAccountTailoredSecurityUpdateTimestamp)
                  .is_null());

  // Mock a stale pref value for kSafeBrowsingEnhanced.
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(false));

  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedProtectionSettingChangeNotification();
}

TEST_F(SafeBrowsingPrefChangeHandlerTest,
       ToastSuppressedForTailoredSecurityChange) {
  // This test simulates a scenario where a user enables ESB through the
  // Tailored Security flow. In this case, we don't want to show the toast
  // because the user has already seen the Tailored Security modal.
  SetSignedIn();
  profile()->GetTestingPrefService()->SetTime(
      prefs::kAccountTailoredSecurityUpdateTimestamp, base::Time::Now());

  // Simulate enabling ESB through a tailored security service update.
  TailoredSecurityService::ScopedSyncNotificationGuard guard(
      *tailored_security_service());
  ASSERT_TRUE(tailored_security_service()->is_handling_sync_notification());
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));

  // We do not show the toast when it's enabled through tailored security
  // service.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(0);

  handler_->MaybeShowEnhancedProtectionSettingChangeNotification();
}

TEST_F(SafeBrowsingPrefChangeHandlerTest,
       ToastShownForNonTailoredSecurityChange) {
  // Verify the toast is shown when the pref change is NOT from an active
  // Tailored Security update.
  SetSignedIn();
  profile()->GetTestingPrefService()->SetTime(
      prefs::kAccountTailoredSecurityUpdateTimestamp, base::Time::Now());

  ASSERT_FALSE(tailored_security_service()->is_handling_sync_notification());

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));

  // We expect 1 call because we are NOT suppressed.
  EXPECT_CALL(toast_controller_, MaybeShowToast(testing::_)).Times(1);

  handler_->MaybeShowEnhancedProtectionSettingChangeNotification();
}
}  // namespace safe_browsing
