// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/support_tool/screenshot_data_collector.h"

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_picker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

using testing::StartsWith;

namespace {

constexpr char kBase64Header[] = "data:image/jpeg;base64,";

const FakeDesktopMediaPickerFactory::TestFlags base_flags({
    .expect_screens = true,
    .expect_windows = true,
    .expect_tabs = true,
});

}  // namespace

class ScreenshotDataCollectorBrowserTest : public InProcessBrowserTest {
 public:
  ScreenshotDataCollectorBrowserTest() = default;

 protected:
  content::DesktopMediaID OpenNewTab(const GURL& url) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    content::RenderFrameHost* host =
        ui_test_utils::NavigateToURLWithDisposition(
            browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
            ui_test_utils::BrowserTestWaitFlags::
                BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    EXPECT_TRUE(host);
    return content::DesktopMediaID(
        content::DesktopMediaID::TYPE_WEB_CONTENTS,
        content::DesktopMediaID::kNullId,
        content::WebContentsMediaCaptureId(host->GetProcess()->GetID(),
                                           host->GetRoutingID()));
  }

  // Ensure that TestFlags outlive FakeDesktopMediaPickerFactory.
  FakeDesktopMediaPickerFactory::TestFlags test_flags_ = base_flags;
};

IN_PROC_BROWSER_TEST_F(ScreenshotDataCollectorBrowserTest,
                       TakeScreenshotOfTab) {
  ScreenshotDataCollector data_collector;
  FakeDesktopMediaPickerFactory picker_factory;

  // Open a new tab.
  content::DesktopMediaID new_page_id =
      OpenNewTab(GURL(chrome::kChromeUINewTabPageURL));

  // Select `new_page_id` in FakeDesktopMediaPickerFactory.
  test_flags_.selected_source = new_page_id;
  picker_factory.SetTestFlags(&test_flags_, 1);
  data_collector.SetPickerFactoryForTesting(&picker_factory);

  // Take a screenshot of the new tab.
  base::test::TestFuture<std::optional<SupportToolError>> test_future_new_page;
  data_collector.CollectDataAndDetectPII(
      test_future_new_page.GetCallback(),
      /*task_runner_for_redaction_tool=*/nullptr,
      /*redaction_tool_container=*/nullptr);
  const std::optional<SupportToolError> error = test_future_new_page.Get();
  EXPECT_EQ(error, std::nullopt);
  const std::string new_page_screenshot = data_collector.GetScreenshotBase64();
  EXPECT_THAT(new_page_screenshot, StartsWith(kBase64Header));
}
