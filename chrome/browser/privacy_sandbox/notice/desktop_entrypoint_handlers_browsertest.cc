// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_callback.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/platform_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
namespace {

using ::testing::Mock;
using ::testing::NiceMock;

class PrivacySandboxNoticeEntryPointHandlersTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    mock_notice_service_ = static_cast<MockPrivacySandboxNoticeService*>(
        PrivacySandboxNoticeServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating([](content::BrowserContext*)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      NiceMock<MockPrivacySandboxNoticeService>>();
                })));

    mock_view_manager_ = std::make_unique<MockDesktopViewManager>();

    ON_CALL(*mock_notice_service_, GetDesktopViewManager())
        .WillByDefault(testing::Return(mock_view_manager_.get()));
  }

  void TearDownOnMainThread() override {
    mock_notice_service_ = nullptr;
    mock_view_manager_.reset();
  }

 protected:
  raw_ptr<MockPrivacySandboxNoticeService> mock_notice_service_;
  std::unique_ptr<MockDesktopViewManager> mock_view_manager_;
};

// Test that navigation alerts view manager.
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
                       TestNavigationCallsEntryPointCallback) {
  EXPECT_CALL(*mock_view_manager_.get(), HandleChromeOwnedPageNavigation)
      .Times(1);
  // Navigate
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  Mock::VerifyAndClearExpectations(mock_view_manager_.get());
}

}  // namespace
}  // namespace privacy_sandbox
