// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_data_service.h"

#include <algorithm>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection_platform_delegate.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::Return;
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

class SessionDataServiceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    profile_ = profile_manager_->CreateTestingProfile("test_profile");

    auto cookie_settings = CookieSettingsFactory::GetForProfile(profile_);
    cookie_settings->SetDefaultCookieSetting(CONTENT_SETTING_SESSION_ONLY);
    profile_->SetExtensionSpecialStoragePolicy(
        base::MakeRefCounted<ExtensionSpecialStoragePolicy>(
            cookie_settings.get()));

    // Simulate an initial browser window open, matching the initial state
    // previously provided by BrowserWithTestWindowTest.
    initial_browser_ = CreateMockBrowser(profile_);
    BrowserCollectionObserver* observer =
        GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
    observer->OnBrowserCreated(initial_browser_.get());

    RestartService(CreateDeleter());
  }

  void TearDown() override {
    session_data_service_.reset();
    browser_shutdown::SetTryingToQuit(false);

    // Ensure the initial browser is safely closed and removed from the global
    // collection if the individual test case didn't already close it.
    auto browsers = GetAllBrowserWindowInterfaces();
    if (std::ranges::find(browsers, initial_browser_.get()) != browsers.end()) {
      BrowserCollectionObserver* observer =
          GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
      observer->OnBrowserClosed(initial_browser_.get());
    }
    initial_browser_.reset();

    profile_ = nullptr;
    profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  std::unique_ptr<StrictMock<MockBrowserWindowInterface>> CreateMockBrowser(
      Profile* profile) {
    auto browser = std::make_unique<StrictMock<MockBrowserWindowInterface>>();
    EXPECT_CALL(*browser, GetProfile()).WillRepeatedly(Return(profile));
    EXPECT_CALL(*browser, GetType())
        .WillRepeatedly(Return(BrowserWindowInterface::TYPE_NORMAL));
    return browser;
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
  TestingProfile* profile() { return profile_; }
  TestingProfileManager* profile_manager() { return profile_manager_; }
  MockBrowserWindowInterface* initial_browser() {
    return initial_browser_.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfileManager> profile_manager_ = nullptr;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<StrictMock<MockBrowserWindowInterface>> initial_browser_;

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
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());

  auto new_browser = CreateMockBrowser(profile());
  BrowserCollectionObserver* observer =
      GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
  observer->OnBrowserCreated(new_browser.get());
  EXPECT_EQ(2U, GlobalBrowserCollection::GetInstance()->GetSize());

  observer->OnBrowserClosed(new_browser.get());
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
  Mock::VerifyAndClearExpectations(deleter());

  bool skip_session_cookies = browser_defaults::kBrowserAliveWithNoWindows;
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(skip_session_cookies, _));
  observer->OnBrowserClosed(initial_browser());
  EXPECT_EQ(0U, GlobalBrowserCollection::GetInstance()->GetSize());
  Mock::VerifyAndClearExpectations(deleter());
}

TEST_F(SessionDataServiceTest, CleanupOnWindowClosedWithOtherProfileOpen) {
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());

  auto* new_profile = profile_manager()->CreateTestingProfile("second_profile");
  auto new_browser = CreateMockBrowser(new_profile);
  BrowserCollectionObserver* observer =
      GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();
  observer->OnBrowserCreated(new_browser.get());
  EXPECT_EQ(2U, GlobalBrowserCollection::GetInstance()->GetSize());

  bool skip_session_cookies = browser_defaults::kBrowserAliveWithNoWindows;
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(skip_session_cookies, _));
  observer->OnBrowserClosed(initial_browser());
  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
  Mock::VerifyAndClearExpectations(deleter());

  observer->OnBrowserClosed(new_browser.get());
}

TEST_F(SessionDataServiceTest, RepeatCleanupAfterNewWindowOpened) {
  BrowserCollectionObserver* observer =
      GlobalBrowserCollection::GetInstance()->GetPlatformDelegate();

  // Close browser and expect cleanup.
  bool skip_session_cookies = browser_defaults::kBrowserAliveWithNoWindows;
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(skip_session_cookies, _));
  observer->OnBrowserClosed(initial_browser());
  Mock::VerifyAndClearExpectations(deleter());

  // Additional requests for cleanup will be ignored.
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // Unless a new browser is opened.
  auto new_browser = CreateMockBrowser(profile());
  observer->OnBrowserCreated(new_browser.get());
  Mock::VerifyAndClearExpectations(deleter());

  // And another cleanup is started.
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(false, _));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  observer->OnBrowserClosed(new_browser.get());
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
      .WillOnce(&RunCallback);
  RestartService(std::move(new_deleter));
  Mock::VerifyAndClearExpectations(deleter());

  // At shutdown, another deletion is started. This time it finishes.
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(false, _))
      .WillOnce(&RunCallback);
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(deleter());

  // A finished deletion does not continue after restart.
  RestartService(CreateDeleter());
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
