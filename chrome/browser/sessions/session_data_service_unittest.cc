// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_data_service.h"

#include <memory>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/sessions/session_data_deleter.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::StrictMock;

namespace {
// A test version of SessionDataDeleter that doesn't actually do any deletion.
class TestSessionDataDeleter : public SessionDataDeleter {
 public:
  TestSessionDataDeleter() : SessionDataDeleter(nullptr) {}
  MOCK_METHOD1(DeleteSessionOnlyData, void(base::OnceClosure));
};
}  // namespace

class SessionDataServiceTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    RecreateService();
  }

  void TearDown() override {
    session_data_service_.reset();
    browser_shutdown::SetTryingToQuit(false);
    BrowserWithTestWindowTest::TearDown();
  }

  void RecreateService() {
    auto deleter = std::make_unique<StrictMock<TestSessionDataDeleter>>();
    session_data_deleter_ = deleter.get();
    session_data_service_ =
        std::make_unique<SessionDataService>(profile(), std::move(deleter));
  }

  SessionDataService* service() { return session_data_service_.get(); }
  TestSessionDataDeleter* deleter() { return session_data_deleter_; }

 private:
  TestSessionDataDeleter* session_data_deleter_;
  std::unique_ptr<SessionDataService> session_data_service_;
};

TEST_F(SessionDataServiceTest, StartCleanup) {
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(_));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(service());
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
  Mock::VerifyAndClearExpectations(service());

  if (!browser_defaults::kBrowserAliveWithNoWindows)
    EXPECT_CALL(*deleter(), DeleteSessionOnlyData(_));
  set_browser(nullptr);
  EXPECT_EQ(0U, browser_list->size());
  Mock::VerifyAndClearExpectations(service());
}

TEST_F(SessionDataServiceTest, CleanupOnWindowClosedWithOtherProfileOpen) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());

  auto* new_profile = profile_manager()->CreateTestingProfile("second_profile");
  auto new_window = CreateBrowserWindow();
  auto new_browser =
      CreateBrowser(new_profile, Browser::TYPE_NORMAL, false, new_window.get());
  EXPECT_EQ(2U, browser_list->size());

  if (!browser_defaults::kBrowserAliveWithNoWindows)
    EXPECT_CALL(*deleter(), DeleteSessionOnlyData(_));
  set_browser(nullptr);
  EXPECT_EQ(1U, browser_list->size());
  Mock::VerifyAndClearExpectations(service());
}

TEST_F(SessionDataServiceTest, RepeatCleanupAfterNewWindowOpened) {
  // Close browser and expect cleanup.
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(_));
  set_browser(nullptr);
  Mock::VerifyAndClearExpectations(service());

  // Additional requests for cleanup will be ignored.
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(service());

  // Unless a new browser is opened.
  auto new_window = CreateBrowserWindow();
  auto new_browser =
      CreateBrowser(profile(), Browser::TYPE_NORMAL, false, new_window.get());
  Mock::VerifyAndClearExpectations(service());

  // And another cleanup is started.
  EXPECT_CALL(*deleter(), DeleteSessionOnlyData(_));
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(service());
}

TEST_F(SessionDataServiceTest, SkipOnShutdown) {
  browser_shutdown::SetTryingToQuit(true);
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(service());
}

TEST_F(SessionDataServiceTest, SkipOnForceSessionState) {
  service()->SetForceKeepSessionState();
  service()->StartCleanup();
  Mock::VerifyAndClearExpectations(service());
}
