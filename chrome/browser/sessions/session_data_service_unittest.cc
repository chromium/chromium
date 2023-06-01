// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_data_service.h"

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/sessions/sessions_features.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::StrictMock;

namespace {
// A test version of SessionDataDeleter that doesn't actually do any deletion.
class TestSessionDataDeleter : public SessionDataDeleter {
 public:
  TestSessionDataDeleter() : SessionDataDeleter(nullptr) {}
  MOCK_METHOD2(DeleteSessionOnlyData, void(bool, base::OnceClosure));
};

// Helper to run the callback received by DeleteSessionOnlyData.
void RunCallback(bool skip_session_cookies, base::OnceClosure callback) {
  std::move(callback).Run();
}
}  // namespace

class SessionDataServiceTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    auto cookie_settings = CookieSettingsFactory::GetForProfile(profile());
    cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
    profile()->SetExtensionSpecialStoragePolicy(
        base::MakeRefCounted<ExtensionSpecialStoragePolicy>(
            cookie_settings.get()));
    RestartService(CreateDeleter());
  }

  void TearDown() override {
    session_data_service_.reset();
    browser_shutdown::SetTryingToQuit(false);
    BrowserWithTestWindowTest::TearDown();
  }

  std::unique_ptr<StrictMock<TestSessionDataDeleter>> CreateDeleter() {
    return std::make_unique<StrictMock<TestSessionDataDeleter>>();
  }

  // Simulates Chrome being restarted from the SessionDataService's perspective.
  void RestartService(
      std::unique_ptr<StrictMock<TestSessionDataDeleter>> deleter) {
    session_data_deleter_ = deleter.get();
    session_data_service_ =
        std::make_unique<SessionDataService>(profile(), std::move(deleter));
  }

  StrictMock<TestSessionDataDeleter>* deleter() {
    return session_data_deleter_;
  }
  SessionDataService* service() { return session_data_service_.get(); }

 private:
  raw_ptr<StrictMock<TestSessionDataDeleter>, DanglingUntriaged>
      session_data_deleter_;
  std::unique_ptr<SessionDataService> session_data_service_;
};

TEST_F(SessionDataServiceTest, StartCleanup) {
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(false, _));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, CleanupOnWindowClosed) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());

  auto new_window = CreateBrowserWindow();
  auto new_browser =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, false, new_window.get());
  EXPECT_EQ(2U, browser_list->size());

  new_browser.reset();
  EXPECT_EQ(1U, browser_list->size());
  Mock::VerifyAndClearExpectations(deleter());

  bool skip_session_cookies = browser_defaults::kBrowserAliveWithNoWindows;
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(skip_session_cookies, _));
  set_browser(nullptr);
  EXPECT_EQ(0U, browser_list->size());
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, CleanupOnWindowClosedWithOtherProfileOpen) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());

  auto* new_profile = profile_manager()->CreateTestingProfile("second_profile");
  auto new_window = CreateBrowserWindow();
  auto new_browser =
      CreateBrowser(new_profile, Browser::TYPE_NORMAL, false, new_window.get());
  EXPECT_EQ(2U, browser_list->size());

  bool skip_session_cookies = browser_defaults::kBrowserAliveWithNoWindows;
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(skip_session_cookies, _));
  set_browser(nullptr);
  EXPECT_EQ(1U, browser_list->size());
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, RepeatCleanupAfterNewWindowOpened) {
  // Close browser and expect cleanup.
  bool skip_session_cookies = browser_defaults::kBrowserAliveWithNoWindows;
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(skip_session_cookies, _));
  set_browser(nullptr);
  Mock::VerifyAndClearExpectations(deleter());

  // Additional requests for cleanup will be ignored.
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // Unless a new browser is opened.
  auto new_window = CreateBrowserWindow();
  auto new_browser =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, false, new_window.get());
  Mock::VerifyAndClearExpectations(deleter());

  // And another cleanup is started.
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(false, _));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, SkipOnShutdown) {
  browser_shutdown::SetTryingToQuit(true);
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // No deletion during shutdown but the deletion will continue on startup.
  browser_shutdown::SetTryingToQuit(false);
  auto new_deleter = CreateDeleter();
  EXPECT_CALL(*new_deleter, DeleteSessionOnlyData(true, _));
  RestartService(std::move(new_deleter));
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, NoContinuedDeletionWithoutSettings) {
  browser_shutdown::SetTryingToQuit(true);
  CookieSettingsFactory::GetForProfile(profile())->SetDefaultCookieSetting(
      CONTENT_SETTING_ALLOW);

  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // No deletion during shutdown and no deletion on startup without SESSION_ONLY
  // setting.
  browser_shutdown::SetTryingToQuit(false);
  RestartService(CreateDeleter());
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, ContinueUnfinishedDeletions) {
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(false, _));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // Deletion is not marked as finished, so it will continue on restart.
  auto new_deleter = CreateDeleter();
  EXPECT_CALL(*new_deleter, DeleteSessionOnlyData(true, _))
      .WillOnce(Invoke(&RunCallback));
  RestartService(std::move(new_deleter));
  Mock::VerifyAndClearExpectations(deleter());

  // At shutdown, another deletion is started. This time it finishes.
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(false, _))
      .WillOnce(Invoke(&RunCallback));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // A finished deletion does not continue after restart.
  RestartService(CreateDeleter());
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, ContinueUnfinishedDeletionsFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kDeleteSessionOnlyDataOnStartup);

  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(false, _));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // Deletion is not marked as finished, but it will not continue on startup
  // because the feature is disabled.
  auto new_deleter = CreateDeleter();
  RestartService(std::move(new_deleter));
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, SkipOnForceSessionState) {
  // No deletion when state should be kept.
  service()->SetForceKeepSessionState();
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // Also deletion on restart after state was kept.
  RestartService(CreateDeleter());
  Mock::VerifyAndClearExpectations(deleter());
}
