// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/picture_in_picture_window_controller.h"

#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/picture_in_picture/picture_in_picture_control_info.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget_observer.h"
#endif

#if defined(OS_CHROMEOS)
#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ui/base/accelerators/accelerator.h"
#endif

using ::testing::_;

namespace {

class MockPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  MockPictureInPictureWindowController() = default;

  // PictureInPictureWindowController:
  MOCK_METHOD0(Show, gfx::Size());
  MOCK_METHOD2(Close, void(bool, bool));
  MOCK_METHOD0(OnWindowDestroyed, void());
  MOCK_METHOD1(SetPictureInPictureCustomControls,
               void(const std::vector<blink::PictureInPictureControlInfo>&));
  MOCK_METHOD2(EmbedSurface, void(const viz::SurfaceId&, const gfx::Size&));
  MOCK_METHOD0(GetWindowForTesting, content::OverlayWindow*());
  MOCK_METHOD0(UpdateLayerBounds, void());
  MOCK_METHOD0(IsPlayerActive, bool());
  MOCK_METHOD0(GetInitiatorWebContents, content::WebContents*());
  MOCK_METHOD2(UpdatePlaybackState, void(bool, bool));
  MOCK_METHOD0(TogglePlayPause, bool());
  MOCK_METHOD1(CustomControlPressed, void(const std::string&));
  MOCK_METHOD1(SetAlwaysHidePlayPauseButton, void(bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPictureInPictureWindowController);
};

}  // namespace

class PictureInPictureWindowControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  PictureInPictureWindowControllerBrowserTest() = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpWindowController(content::WebContents* web_contents) {
    pip_window_controller_ =
        content::PictureInPictureWindowController::GetOrCreateForWebContents(
            web_contents);
  }

  content::PictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

  MockPictureInPictureWindowController& mock_controller() {
    return mock_controller_;
  }

  void LoadTabAndEnterPictureInPicture(Browser* browser) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(
            FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
    ui_test_utils::NavigateToURL(browser, test_page_url);

    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    ASSERT_NE(nullptr, active_web_contents);

    SetUpWindowController(active_web_contents);

    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        active_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

#if !defined(OS_ANDROID)
  class WidgetBoundsChangeWaiter : public views::WidgetObserver {
   public:
    explicit WidgetBoundsChangeWaiter(views::Widget* widget)
        : widget_(widget), initial_bounds_(widget->GetWindowBoundsInScreen()) {
      widget_->AddObserver(this);
    }

    ~WidgetBoundsChangeWaiter() final { widget_->RemoveObserver(this); }

    void OnWidgetBoundsChanged(views::Widget* widget, const gfx::Rect&) final {
      run_loop_.Quit();
    }

    void Wait() {
      if (widget_->GetWindowBoundsInScreen() != initial_bounds_)
        return;
      run_loop_.Run();
    }

   private:
    views::Widget* widget_;
    gfx::Rect initial_bounds_;
    base::RunLoop run_loop_;
  };
#endif  // !defined(OS_ANDROID)

 private:
  content::PictureInPictureWindowController* pip_window_controller_ = nullptr;
  MockPictureInPictureWindowController mock_controller_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowControllerBrowserTest);
};

class ControlPictureInPictureWindowControllerBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// Checks the creation of the window controller, as well as basic window
// creation and visibility.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CreationAndVisibility) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller() != nullptr);

  ASSERT_TRUE(window_controller()->GetWindowForTesting() != nullptr);
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
}

#if (defined(OS_MACOSX) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)
class PictureInPicturePixelComparisonBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  base::FilePath GetFilePath(base::FilePath::StringPieceType relative_path) {
    base::FilePath base_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &base_dir));
    // The path relative to <chromium src> for pixel test data.
    const base::FilePath::StringPieceType kTestDataPath =
        FILE_PATH_LITERAL("chrome/test/data/media/picture-in-picture/");
    base::FilePath full_path =
        base_dir.Append(kTestDataPath).Append(relative_path);
    return full_path;
  }

  void ReadbackResult(base::RepeatingClosure quit_run_loop,
                      std::unique_ptr<viz::CopyOutputResult> result) {
    ASSERT_FALSE(result->IsEmpty());
    EXPECT_EQ(viz::CopyOutputResult::Format::RGBA_BITMAP, result->format());
    result_bitmap_ = std::make_unique<SkBitmap>(result->AsSkBitmap());
    EXPECT_TRUE(result_bitmap_->readyToDraw());
    quit_run_loop.Run();
  }

  bool SaveBitmap(base::FilePath& file_path, SkBitmap& bitmap) {
    base::File file(file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_OPEN_ALWAYS);
    CHECK(file.IsValid());
    std::vector<unsigned char> png_data;
    CHECK(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_data));
    char* data = reinterpret_cast<char*>(&png_data[0]);
    int size = static_cast<int>(png_data.size());
    return base::WriteFile(file_path, data, size) == size;
  }

  void TakeOverlayWindowScreenshot(OverlayWindowViews* overlay_window_views) {
    base::RunLoop run_loop;
    std::unique_ptr<viz::CopyOutputRequest> request =
        std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
            base::BindOnce(
                &PictureInPicturePixelComparisonBrowserTest::ReadbackResult,
                base::Unretained(this), run_loop.QuitClosure()));
    overlay_window_views->GetLayer()->RequestCopyOfOutput(std::move(request));
    run_loop.Run();
  }

  bool CompareImages(const SkBitmap& actual_bmp) {
    // Number of pixels with an error
    int error_pixels_count = 0;
    gfx::Rect error_bounding_rect;

    for (int x = 0; x < actual_bmp.width(); ++x) {
      for (int y = 0; y < actual_bmp.height(); ++y) {
        SkColor actual_color = actual_bmp.getColor(x, y);
        // Check color is Yellow. The difference is caused by video conversion.
        // TODO(cliffordcheng): Compare with an expected image instead of just
        // checking pixel RGB color.
        if (SkColorGetR(actual_color) != 254 &&
            SkColorGetG(actual_color) != 253 &&
            SkColorGetB(actual_color) != 0) {
          ++error_pixels_count;
          error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
        }
      }
    }
    if (error_pixels_count != 0) {
      LOG(ERROR) << "Number of pixel with an error: " << error_pixels_count;
      LOG(ERROR) << "Error Bounding Box : " << error_bounding_rect.ToString();
      return false;
    }
    return true;
  }

  SkBitmap& GetResultBitmap() { return *result_bitmap_; }

 private:
  std::unique_ptr<SkBitmap> result_bitmap_;
};

// TODO(cliffordcheng): enable on Windows when compile errors are resolved.
// Plays a video and then trigger Picture-in-Picture. Grabs a screenshot of
// Picture-in-Picture window and verifies it's as expected.
IN_PROC_BROWSER_TEST_F(PictureInPicturePixelComparisonBrowserTest, VideoPlay) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/pixel_test.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  SetUpWindowController(active_web_contents);
  ASSERT_NE(nullptr, window_controller());

  ASSERT_NE(nullptr, window_controller()->GetWindowForTesting());
  ASSERT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  OverlayWindowViews* overlay_window_views = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  overlay_window_views->SetSize(gfx::Size(600, 400));
  base::string16 expected_title = base::ASCIIToUTF16("resized");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
  TakeOverlayWindowScreenshot(overlay_window_views);

  std::string test_image = "pixel_test_actual_0.png";
  base::FilePath test_image_path = GetFilePath(test_image);
  ASSERT_TRUE(SaveBitmap(test_image_path, GetResultBitmap()));
  EXPECT_TRUE(CompareImages(GetResultBitmap()));
}
#endif  // (defined(OS_MACOSX) || defined(OS_LINUX)) && !defined(OS_CHROMEOS)

// Tests that when an active WebContents accurately tracks whether a video
// is in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       TabIconUpdated) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // First test there is no video playing in Picture-in-Picture.
  EXPECT_FALSE(active_web_contents->HasPictureInPictureVideo());

  // Start playing video in Picture-in-Picture and retest with the
  // opposite assertion.
  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(active_web_contents->HasPictureInPictureVideo());

  // Stop video being played Picture-in-Picture and check if that's tracked.
  window_controller()->Close(true /* should_pause_video */,
                             true /* should_reset_pip_player */);
  EXPECT_FALSE(active_web_contents->HasPictureInPictureVideo());

  // Reload page should not crash.
  ui_test_utils::NavigateToURL(browser(), test_page_url);
}

#if !defined(OS_ANDROID)

// Tests that when creating a Picture-in-Picture window a size is sent to the
// caller and if the window is resized, the caller is also notified.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResizeEventFired) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  content::OverlayWindow* overlay_window =
      window_controller()->GetWindowForTesting();
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->IsVisible());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  static_cast<OverlayWindowViews*>(overlay_window)
      ->SetSize(gfx::Size(400, 400));

  base::string16 expected_title = base::ASCIIToUTF16("resized");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when a custom control is clicked on a Picture-in-Picture window
// an event is sent to the caller.
IN_PROC_BROWSER_TEST_F(ControlPictureInPictureWindowControllerBrowserTest,
                       PictureInPictureControlEventFired) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  content::OverlayWindow* overlay_window =
      window_controller()->GetWindowForTesting();
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->IsVisible());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  std::string control_id = "Test custom control ID";
  base::string16 expected_title = base::ASCIIToUTF16(control_id);

  window_controller()->CustomControlPressed(control_id);

  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when a custom control is clicked on a Picture-in-Picture window
// with one custom control on it, the correct event is sent to the caller.
IN_PROC_BROWSER_TEST_F(ControlPictureInPictureWindowControllerBrowserTest,
                       PictureInPictureAddControlAndFireEvent) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  content::OverlayWindow* overlay_window =
      window_controller()->GetWindowForTesting();
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->IsVisible());

  LoadTabAndEnterPictureInPicture(browser());

  const std::string kControlId = "control_id";
  EXPECT_EQ(true, content::EvalJs(
                      active_web_contents,
                      content::JsReplace("setControls([$1]);", kControlId)));

  base::string16 expected_title = base::ASCIIToUTF16(kControlId);

  window_controller()->CustomControlPressed(kControlId);
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when the first custom control added to a Picture-in-Picture window
// with two custom controls on it is clicked the corresponding event is sent to
// the caller.
IN_PROC_BROWSER_TEST_F(ControlPictureInPictureWindowControllerBrowserTest,
                       PictureInPictureAddTwoControlsAndFireLeftEvent) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  content::OverlayWindow* overlay_window =
      window_controller()->GetWindowForTesting();
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->IsVisible());

  LoadTabAndEnterPictureInPicture(browser());

  const std::string kLeftControlId = "left-control";
  const std::string kRightControlId = "right-control";
  EXPECT_EQ(true, content::EvalJs(
                      active_web_contents,
                      content::JsReplace("setControls([$1, $2]);",
                                         kLeftControlId, kRightControlId)));

  base::string16 left_expected_title = base::ASCIIToUTF16(kLeftControlId);

  window_controller()->CustomControlPressed(kLeftControlId);
  EXPECT_EQ(left_expected_title,
            content::TitleWatcher(active_web_contents, left_expected_title)
                .WaitAndGetTitle());
}

// Tests that when the second custom control added to a Picture-in-Picture
// window with two custom controls on it is clicked the corresponding event is
// sent to the caller.
IN_PROC_BROWSER_TEST_F(ControlPictureInPictureWindowControllerBrowserTest,
                       PictureInPictureAddTwoControlsAndFireRightEvent) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  content::OverlayWindow* overlay_window =
      window_controller()->GetWindowForTesting();
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->IsVisible());

  LoadTabAndEnterPictureInPicture(browser());

  const std::string kLeftControlId = "left-control";
  const std::string kRightControlId = "right-control";
  EXPECT_EQ(true, content::EvalJs(
                      active_web_contents,
                      content::JsReplace("setControls([$1, $2]);",
                                         kLeftControlId, kRightControlId)));

  base::string16 right_expected_title = base::ASCIIToUTF16(kRightControlId);

  window_controller()->CustomControlPressed(kRightControlId);
  EXPECT_EQ(right_expected_title,
            content::TitleWatcher(active_web_contents, right_expected_title)
                .WaitAndGetTitle());
}

#endif  // !defined(OS_ANDROID)

// Tests that when closing a Picture-in-Picture window, the video element is
// reflected as no longer in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  window_controller()->Close(true /* should_pause_video */,
                             true /* should_reset_pip_player */);

  base::string16 expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_TRUE(is_paused);
}

// Ditto, when the video isn't playing.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowWithoutPlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  ASSERT_TRUE(window_controller());
  window_controller()->Close(true /* should_pause_video */,
                             true /* should_reset_pip_player */);

  base::string16 expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when closing a Picture-in-Picture window from the Web API, the
// video element is not paused.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowFromWebAPIWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "exitPictureInPicture();"));

  // 'left' is sent when the first video leaves Picture-in-Picture.
  base::string16 expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);
}

// Tests that when starting a new Picture-in-Picture session from the same
// video, the video stays in Picture-in-Picture mode.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       RequestPictureInPictureTwiceFromSameVideo) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));

  {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        active_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "exitPictureInPicture();"));

  // 'left' is sent when the video leaves Picture-in-Picture.
  base::string16 expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        active_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

  in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);
}

// Tests that when starting a new Picture-in-Picture session from the same tab,
// the previous video is no longer in Picture-in-Picture mode.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       OpenSecondPictureInPictureStopsFirst) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "secondPictureInPicture();"));

  // 'left' is sent when the first video leaves Picture-in-Picture.
  base::string16 expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);

#if !defined(OS_ANDROID)
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_EQ(overlay_window->playback_state_for_testing(),
            OverlayWindowViews::PlaybackState::kPaused);
#endif
}

// Tests that resetting video src when video is in Picture-in-Picture session
// keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResetVideoSrcKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(browser());

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(
      window_controller()->GetWindowForTesting()->GetVideoLayer()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.src = null;"));

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_FALSE(
      window_controller()->GetWindowForTesting()->GetVideoLayer()->visible());
}

// Tests that updating video src when video is in Picture-in-Picture session
// keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       UpdateVideoSrcKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(browser());

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(
      window_controller()->GetWindowForTesting()->GetVideoLayer()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeVideoSrc();", &result));
  EXPECT_TRUE(result);

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(
      window_controller()->GetWindowForTesting()->GetVideoLayer()->visible());

#if !defined(OS_ANDROID)
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_FALSE(
      overlay_window->controls_parent_view_for_testing()->layer()->visible());
#endif
}

// Tests that changing video src to media stream when video is in
// Picture-in-Picture session keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(
    PictureInPictureWindowControllerBrowserTest,
    ChangeVideoSrcToMediaStreamKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(browser());

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(
      window_controller()->GetWindowForTesting()->GetVideoLayer()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeVideoSrcToMediaStream();", &result));
  EXPECT_TRUE(result);

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(
      window_controller()->GetWindowForTesting()->GetVideoLayer()->visible());

#if !defined(OS_ANDROID)
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_FALSE(
      overlay_window->controls_parent_view_for_testing()->layer()->visible());
#endif
}

// Tests that we can enter Picture-in-Picture when a video is not preloaded,
// using the metadata optimizations. This test is checking that there are no
// crashes.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterMetadataPosterOptimisation) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL(
          "media/picture-in-picture/player_metadata_poster.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);
}

// Tests that calling PictureInPictureWindowController::Close() twice has no
// side effect.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseTwiceSideEffects) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  window_controller()->Close(true /* should_pause_video */,
                             true /* should_reset_pip_player */);

  // Wait for the window to close.
  base::string16 expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool video_paused = false;

  // Video is paused after Picture-in-Picture window was closed.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "isPaused();", &video_paused));
  EXPECT_TRUE(video_paused);

  // Resume playback.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "isPaused();", &video_paused));
  EXPECT_FALSE(video_paused);

  // This should be a no-op because the window is not visible.
  window_controller()->Close(true /* should_pause_video */,
                             true /* should_reset_pip_player */);

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "isPaused();", &video_paused));
  EXPECT_FALSE(video_paused);
}

// Checks entering Picture-in-Picture on multiple tabs, where the initial tab
// has been closed.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PictureInPictureAfterClosingTab) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        active_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Open a new tab in the browser.
  AddTabAtIndex(1, test_page_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Once the initiator tab is closed, the controller should also be torn down.
  browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Open video in Picture-in-Picture mode again, on the new tab.
  active_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        active_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Checks setting disablePictureInPicture on video just after requesting
// Picture-in-Picture doesn't result in a window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       RequestPictureInPictureAfterDisablePictureInPicture) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  bool result = true;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "requestPictureInPictureAndDisable();", &result));
  EXPECT_FALSE(result);

  ASSERT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Checks that a video in Picture-in-Picture stops if its iframe is removed.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       FrameEnterLeaveClosesWindow) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-test.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      active_web_contents->GetAllFrames();
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  // Wait for video metadata to load.
  base::string16 expected_title = base::ASCIIToUTF16("loadedmetadata");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      iframe, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, active_web_contents->GetAllFrames().size());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CrossOriginFrameEnterLeaveCloseWindow) {
  GURL embed_url = embedded_test_server()->GetURL(
      "a.com", "/media/picture-in-picture/iframe-content.html");
  GURL main_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture-in-picture/iframe-test.html?embed_url=" +
                         embed_url.spec());

  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      active_web_contents->GetAllFrames();
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  // Wait for video metadata to load.
  base::string16 expected_title = base::ASCIIToUTF16("loadedmetadata");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      iframe, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, active_web_contents->GetAllFrames().size());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MultipleBrowserWindowOnePIPWindow) {
  LoadTabAndEnterPictureInPicture(browser());

  content::PictureInPictureWindowController* first_controller =
      window_controller();
  EXPECT_TRUE(first_controller->GetWindowForTesting()->IsVisible());

  Browser* second_browser = CreateBrowser(browser()->profile());
  LoadTabAndEnterPictureInPicture(second_browser);

  content::PictureInPictureWindowController* second_controller =
      window_controller();
  EXPECT_FALSE(first_controller->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(second_controller->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterPictureInPictureThenFullscreen) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "enterFullscreen()"));

  base::string16 expected_title = base::ASCIIToUTF16("fullscreen");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  EXPECT_TRUE(active_web_contents->IsFullscreenForCurrentTab());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterFullscreenThenPictureInPicture) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "enterFullscreen()"));

  base::string16 expected_title = base::ASCIIToUTF16("fullscreen");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_FALSE(active_web_contents->IsFullscreenForCurrentTab());
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterPictureInPictureThenNavigateAwayCloseWindow) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Same document navigations should not close Picture-in-Picture window.
  EXPECT_TRUE(content::ExecuteScript(
      active_web_contents, "window.location = '#foo'; window.history.back();"));
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Picture-in-Picture window should be closed after navigating away.
  GURL another_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-size.html")));
  ui_test_utils::NavigateToURL(browser(), another_page_url);
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

#if !defined(OS_ANDROID)

// Tests that when a new surface id is sent to the Picture-in-Picture window, it
// doesn't move back to its default position.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       SurfaceIdChangeDoesNotMoveWindow) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);
  ASSERT_TRUE(overlay_window->IsVisible());

  // Move and resize the window to the top left corner and wait for ack.
  {
    WidgetBoundsChangeWaiter waiter(overlay_window);

    overlay_window->SetBounds(gfx::Rect(0, 0, 400, 400));

    waiter.Wait();
  }

  // Wait for signal that the window was resized.
  base::string16 expected_title = base::ASCIIToUTF16("resized");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Simulate a new surface layer and a change in aspect ratio then wait for
  // ack.
  {
    WidgetBoundsChangeWaiter waiter(overlay_window);

    window_controller()->EmbedSurface(
        viz::SurfaceId(
            viz::FrameSinkId(1, 1),
            viz::LocalSurfaceId(9, base::UnguessableToken::Create())),
        gfx::Size(200, 100));

    waiter.Wait();
  }

  // The position should be closer to the (0, 0) than the default one (bottom
  // right corner). This will be reflected by checking that the position is
  // below (100, 100).
  EXPECT_LT(overlay_window->GetBounds().x(), 100);
  EXPECT_LT(overlay_window->GetBounds().y(), 100);
}

// Tests that the Picture-in-Picture state is properly updated when the window
// is closed at a system level.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowNotifiesController) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);
  ASSERT_TRUE(overlay_window->IsVisible());

  // Simulate closing from the system.
  overlay_window->OnNativeWidgetDestroyed();

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);
}

// Tests that the play/pause icon state is properly updated when a
// Picture-in-Picture is created after a reload.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PlayPauseStateAtCreation) {
  LoadTabAndEnterPictureInPicture(browser());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));

  bool is_paused = true;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(
      window_controller()->GetWindowForTesting()->GetVideoLayer()->visible());

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_TRUE(overlay_window->play_pause_controls_view_for_testing()
                  ->toggled_for_testing());

  ASSERT_TRUE(
      content::ExecuteScript(active_web_contents, "exitPictureInPicture();"));

  content::TestNavigationObserver observer(active_web_contents, 1);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  observer.Wait();

  {
    content::WebContents* active_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        active_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);

    bool is_paused = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                            &is_paused));
    EXPECT_TRUE(is_paused);

    OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
        window_controller()->GetWindowForTesting());

    EXPECT_FALSE(overlay_window->play_pause_controls_view_for_testing()
                     ->toggled_for_testing());
  }
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterUsingControllerShowsWindow) {
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_NE(nullptr, pip_window_manager);

  // Show the non-WebContents based Picture-in-Picture window controller.
  EXPECT_CALL(mock_controller(), Show());
  pip_window_manager->EnterPictureInPictureWithController(&mock_controller());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterUsingWebContentsThenUsingController) {
  // Enter using WebContents.
  LoadTabAndEnterPictureInPicture(browser());

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_NE(nullptr, overlay_window);
  EXPECT_TRUE(overlay_window->IsVisible());

  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_NE(nullptr, pip_window_manager);

  // The new Picture-in-Picture window should be shown.
  EXPECT_CALL(mock_controller(), Show());
  pip_window_manager->EnterPictureInPictureWithController(&mock_controller());

  // WebContents sourced Picture-in-Picture should stop.
  bool in_picture_in_picture = false;
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterUsingControllerThenEnterUsingWebContents) {
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_NE(nullptr, pip_window_manager);

  // Show the non-WebContents based Picture-in-Picture window controller.
  EXPECT_CALL(mock_controller(), Show());
  pip_window_manager->EnterPictureInPictureWithController(&mock_controller());

  // Now show the WebContents based Picture-in-Picture window controller.
  // This should close the existing window and show the new one.
  EXPECT_CALL(mock_controller(), Close(_, _));
  LoadTabAndEnterPictureInPicture(browser());

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);
  EXPECT_TRUE(overlay_window->IsVisible());
}

#endif  // !defined(OS_ANDROID)

// This checks that a video in Picture-in-Picture with preload none, when
// changing source willproperly update the associated media player id. This is
// checked by closing the window because the test it at a too high level to be
// able to check the actual media player id being used.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PreloadNoneSrcChangeThenLoad) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL(
          "media/picture-in-picture/player_preload_none.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(active_web_contents,
                                                   "play();", &result));
  ASSERT_TRUE(result);

  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  ASSERT_TRUE(result);

  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeSrcAndLoad();", &result));
  ASSERT_TRUE(result);

  window_controller()->Close(true /* should_pause_video */,
                             true /* should_reset_pip_player */);

  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &result));
  EXPECT_FALSE(result);
}

// Tests that opening a Picture-in-Picture window from a video in an iframe
// will not lead to a crash when the tab is closed while devtools is open.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       OpenInFrameWithDevToolsDoesNotCrash) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-test.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      active_web_contents->GetAllFrames();
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  // Wait for video metadata to load.
  base::string16 expected_title = base::ASCIIToUTF16("loadedmetadata");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Attaching devtools triggers the change in timing that leads to the crash.
  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      browser(), true /*is_docked=*/);

  {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        iframe, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  EXPECT_EQ(2u, active_web_contents->GetAllFrames().size());

  // Open a new tab in the browser.
  AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Closing the initiator should not crash Chrome.
  content::WebContentsDestroyedWatcher destroyed_watcher(active_web_contents);
  browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  destroyed_watcher.Wait();

  // Make sure the window and therefore Chrome_DevToolsADBThread shutdown
  // gracefully.
  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}

#if defined(OS_CHROMEOS)
// Tests that video in Picture-in-Picture is paused when user presses
// VKEY_MEDIA_PLAY_PAUSE key even if there's another media playing in a
// foreground tab.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       HandleMediaKeyPlayPause) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* first_active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(first_active_web_contents);
  EXPECT_TRUE(
      content::ExecuteScript(first_active_web_contents, "video.play();"));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      first_active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  Browser* second_browser = CreateBrowser(browser()->profile());
  ui_test_utils::NavigateToURL(second_browser, test_page_url);

  content::WebContents* second_active_web_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(second_active_web_contents);
  EXPECT_TRUE(
      content::ExecuteScript(second_active_web_contents, "video.play();"));

  ash::AcceleratorController* controller =
      ash::Shell::Get()->accelerator_controller();
  controller->Process(ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE));
  base::RunLoop().RunUntilIdle();

  bool is_paused = false;
  // Picture-in-Picture video in first browser window is paused.
  EXPECT_TRUE(ExecuteScriptAndExtractBool(first_active_web_contents,
                                          "isPaused();", &is_paused));
  EXPECT_TRUE(is_paused);

  // Video in second browser window is not paused.
  EXPECT_TRUE(ExecuteScriptAndExtractBool(second_active_web_contents,
                                          "isPaused();", &is_paused));
  EXPECT_FALSE(is_paused);
}

// Tests that the close and resize icons move properly as the window changes
// quadrants.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MovingQuadrantsMovesResizeControl) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  content::OverlayWindow* overlay_window =
      window_controller()->GetWindowForTesting();
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->IsVisible());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  ASSERT_TRUE(result);

  OverlayWindowViews* overlay_window_views =
      static_cast<OverlayWindowViews*>(overlay_window);

  // The PiP window starts in the bottom-right quadrant of the screen.
  gfx::Rect bottom_right_bounds = overlay_window_views->GetBounds();
  // The relative center point of the window.
  gfx::Point center(bottom_right_bounds.width() / 2,
                    bottom_right_bounds.height() / 2);
  gfx::Point close_button_position =
      overlay_window_views->close_image_position_for_testing();
  gfx::Point resize_button_position =
      overlay_window_views->resize_handle_position_for_testing();

  // The close button should be in the top right corner.
  EXPECT_LT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the top left corner.
  EXPECT_GT(center.x(), resize_button_position.x());
  EXPECT_GT(center.y(), resize_button_position.y());

  // Move the window to the bottom left corner.
  gfx::Rect bottom_left_bounds(0, bottom_right_bounds.y(),
                               bottom_right_bounds.width(),
                               bottom_right_bounds.height());
  overlay_window_views->SetBounds(bottom_left_bounds);
  close_button_position =
      overlay_window_views->close_image_position_for_testing();
  resize_button_position =
      overlay_window_views->resize_handle_position_for_testing();

  // The close button should be in the top left corner.
  EXPECT_GT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the top right corner.
  EXPECT_LT(center.x(), resize_button_position.x());
  EXPECT_GT(center.y(), resize_button_position.y());

  // Move the window to the top right corner.
  gfx::Rect top_right_bounds(bottom_right_bounds.x(), 0,
                             bottom_right_bounds.width(),
                             bottom_right_bounds.height());
  overlay_window_views->SetBounds(top_right_bounds);
  close_button_position =
      overlay_window_views->close_image_position_for_testing();
  resize_button_position =
      overlay_window_views->resize_handle_position_for_testing();

  // The close button should be in the top right corner.
  EXPECT_LT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the bottom left corner.
  EXPECT_GT(center.x(), resize_button_position.x());
  EXPECT_LT(center.y(), resize_button_position.y());

  // Move the window to the top left corner.
  gfx::Rect top_left_bounds(0, 0, bottom_right_bounds.width(),
                            bottom_right_bounds.height());
  overlay_window_views->SetBounds(top_left_bounds);
  close_button_position =
      overlay_window_views->close_image_position_for_testing();
  resize_button_position =
      overlay_window_views->resize_handle_position_for_testing();

  // The close button should be in the top right corner.
  EXPECT_LT(center.x(), close_button_position.x());
  EXPECT_GT(center.y(), close_button_position.y());
  // The resize button should be in the bottom right corner.
  EXPECT_LT(center.x(), resize_button_position.x());
  EXPECT_LT(center.y(), resize_button_position.y());
}

#endif  // defined(OS_CHROMEOS)
