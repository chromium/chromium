// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/media/webrtc/display_media_access_handler.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Wait for a NativeWindow to receive window focus.
bool FocusWidgetAndWait(content::WebContents* contents) {
  gfx::NativeWindow native_window = contents->GetTopLevelNativeWindow();
  auto* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(native_window);
  if (!browser_view) {
    return false;
  }

  return base::test::RunUntil([browser_view]() {
    browser_view->frame()->Activate();
    return browser_view->frame()->IsActive();
  });
}

}  // namespace

class DisplayMediaAccessHandlerInteractiveUITest
    : public WebRtcTestBase,
      public testing::WithParamInterface<
          std::tuple</*focus_opener=*/bool, /*request_from_opener=*/bool>>,
      public DesktopMediaPickerManager::DialogObserver {
 public:
  DisplayMediaAccessHandlerInteractiveUITest() = default;
  ~DisplayMediaAccessHandlerInteractiveUITest() override = default;

  DisplayMediaAccessHandlerInteractiveUITest(
      const DisplayMediaAccessHandlerInteractiveUITest&) = delete;
  DisplayMediaAccessHandlerInteractiveUITest& operator=(
      const DisplayMediaAccessHandlerInteractiveUITest&) = delete;

  void SetUpOnMainThread() override {
    WebRtcTestBase::SetUpOnMainThread();

    // Don't allow system audio to be selected.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(
            web_contents->GetBrowserContext());
    content_settings->SetDefaultContentSetting(
        ContentSettingsType::DISPLAY_MEDIA_SYSTEM_AUDIO, CONTENT_SETTING_BLOCK);
  }

  // DesktopMediaPickerManager::DialogObserver implementation:
  void OnDialogOpened(const DesktopMediaPicker::Params& params) override {
    actual_ui_web_contents_ = params.web_contents;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnDialogClosed() override {}

  std::unique_ptr<base::RunLoop> run_loop_;

  // Okay since this is never dereferenced anywhere.  We could work around the
  // dangling reference, but it's fairly obfuscated.  This is a lot clearer
  // what's actually going on.
  raw_ptr<content::WebContents, DisableDanglingPtrDetection>
      actual_ui_web_contents_ = nullptr;
};

// Verify that the picker shows up in the correct window when document picture
// in picture is involved, based on whether `getDisplayMedia()` is called from
// the opener's navigator or pip's navigator, and which window has the focus.
IN_PROC_BROWSER_TEST_P(DisplayMediaAccessHandlerInteractiveUITest,
                       PickerUiSelectsCorrectWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const bool focus_opener = std::get<0>(GetParam());
  const bool request_from_opener = std::get<1>(GetParam());
#if BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_OZONE_WAYLAND)
  // Wayland doesn't support changing window activation programmatically, so we
  // can't focus the opener.  The pip window will have the focus when it opens.
  // Note that if it doesn't have focus for some reason (i.e., something
  // changes), then the `FocusAndWait()` call, below, will time out waiting for
  // it to become focused.
  if (focus_opener) {
    GTEST_SKIP();
  }
#endif
#endif

  // Navigate to an empty page.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  DesktopMediaPickerManager* picker_manager = DesktopMediaPickerManager::Get();
  picker_manager->AddObserver(this);

  content::WebContents* opener_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  run_loop_ = std::make_unique<base::RunLoop>();
  // Open a pip window and wait for it to show up.  This allows us to change the
  // focus if we want.
  EXPECT_EQ(true, content::EvalJs(opener_web_contents->GetPrimaryMainFrame(),
                                  R"((async () => {
    var pip = await documentPictureInPicture.requestWindow();
    return await new Promise((resolve, reject) => {
    pip.requestAnimationFrame(()=>{resolve(true);});
    }
    );
  })())"));

  content::WebContents* pip_web_contents =
      PictureInPictureWindowManager::GetInstance()->GetChildWebContents();

  // Focus the correct WebContents.
  auto* focus_web_contents =
      focus_opener ? opener_web_contents : pip_web_contents;
  FocusWidgetAndWait(focus_web_contents);

  // Request media from the correct navigator.
  auto* request_web_contents =
      request_from_opener ? opener_web_contents : pip_web_contents;
  EXPECT_EQ(true, content::EvalJs(request_web_contents->GetPrimaryMainFrame(),
                                  R"((async () => {
    navigator.mediaDevices.getDisplayMedia({
        audio: true, systemAudio: 'include'});
    return true;
  })())"));
  run_loop_->Run();

  // Verify that the picker showed up in the correct window.  Requests from pip
  // should not be modified, but requests from the opener should depend on
  // whether it was the pip window or opener that was focused.
  auto* expected_ui_web_contents =
      request_from_opener ? focus_web_contents : pip_web_contents;
  EXPECT_EQ(actual_ui_web_contents_, expected_ui_web_contents);
}

INSTANTIATE_TEST_SUITE_P(DisplayMediaAccessHandlerInteractiveUITest,
                         DisplayMediaAccessHandlerInteractiveUITest,
                         testing::Combine(testing::Bool(), testing::Bool()));
