// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/picture_in_picture_window_controller.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "chrome/browser/ui/views/overlay/playback_image_button.h"
#include "chrome/browser/ui/views/overlay/skip_ad_label_button.h"
#include "chrome/browser/ui/views/overlay/track_image_button.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "services/media_session/public/cpp/features.h"
#include "skia/ext/image_operations.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/accelerators.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#endif

using ::testing::_;

namespace {

class MockPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  MockPictureInPictureWindowController() = default;

  // PictureInPictureWindowController:
  MOCK_METHOD0(Show, void());
  MOCK_METHOD1(Close, void(bool));
  MOCK_METHOD0(CloseAndFocusInitiator, void());
  MOCK_METHOD0(OnWindowDestroyed, void());
  MOCK_METHOD0(GetWindowForTesting, content::OverlayWindow*());
  MOCK_METHOD0(UpdateLayerBounds, void());
  MOCK_METHOD0(IsPlayerActive, bool());
  MOCK_METHOD0(GetInitiatorWebContents, content::WebContents*());
  MOCK_METHOD2(UpdatePlaybackState, void(bool, bool));
  MOCK_METHOD0(TogglePlayPause, bool());
  MOCK_METHOD1(SetAlwaysHidePlayPauseButton, void(bool));
  MOCK_METHOD0(SkipAd, void());
  MOCK_METHOD0(NextTrack, void());
  MOCK_METHOD0(PreviousTrack, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPictureInPictureWindowController);
};

const base::FilePath::CharType kPictureInPictureWindowSizePage[] =
    FILE_PATH_LITERAL("media/picture-in-picture/window-size.html");

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

  void LoadTabAndEnterPictureInPicture(Browser* browser,
                                       const base::FilePath& file_path) {
    GURL test_page_url = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory), file_path);
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

  // The WebContents that is passed to this method must have a
  // "isInPictureInPicture()" method returning a boolean and when the video
  // leaves Picture-in-Picture, it must change the WebContents title to
  // "leavepictureinpicture".
  void ExpectLeavePictureInPicture(content::WebContents* web_contents) {
    // 'leavepictureinpicture' is the title of the tab when the event is
    // received.
    const base::string16 expected_title =
        base::ASCIIToUTF16("leavepictureinpicture");
    EXPECT_EQ(
        expected_title,
        content::TitleWatcher(web_contents, expected_title).WaitAndGetTitle());

    bool in_picture_in_picture = true;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        web_contents, "isInPictureInPicture();", &in_picture_in_picture));
    EXPECT_FALSE(in_picture_in_picture);
  }

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

  void MoveMouseOver(OverlayWindowViews* window) {
    gfx::Point p(window->GetBounds().x(), window->GetBounds().y());
    ui::MouseEvent moved_over(ui::ET_MOUSE_MOVED, p, p, ui::EventTimeForNow(),
                              ui::EF_NONE, ui::EF_NONE);
    window->OnMouseEvent(&moved_over);
  }

 private:
  content::PictureInPictureWindowController* pip_window_controller_ = nullptr;
  MockPictureInPictureWindowController mock_controller_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowControllerBrowserTest);
};

// Checks the creation of the window controller, as well as basic window
// creation, visibility and activation.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CreationAndVisibilityAndActivation) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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

#if defined(TOOLKIT_VIEWS)
  auto* overlay_window = window_controller()->GetWindowForTesting();
  gfx::NativeWindow native_window =
      static_cast<OverlayWindowViews*>(overlay_window)->GetNativeWindow();
#if defined(OS_CHROMEOS) || \
    (defined(MAC_OS_X_VERSION_10_12) && !defined(MAC_OS_VERSION_10_13))
  EXPECT_FALSE(platform_util::IsWindowActive(native_window));
#else
  EXPECT_TRUE(platform_util::IsWindowActive(native_window));
#endif
#endif
}

#if !defined(OS_CHROMEOS)
class PictureInPicturePixelComparisonBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kDisableGpu);
  }

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

  bool ReadImageFile(const base::FilePath& file_path, SkBitmap* read_image) {
    std::string png_string;
    base::ReadFileToString(file_path, &png_string);
    return gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(png_string.data()),
        png_string.length(), read_image);
  }

  void TakeOverlayWindowScreenshot(OverlayWindowViews* overlay_window_views) {
    base::RunLoop run_loop;
    std::unique_ptr<viz::CopyOutputRequest> request =
        std::make_unique<viz::CopyOutputRequest>(
            viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
            base::BindOnce(
                &PictureInPicturePixelComparisonBrowserTest::ReadbackResult,
                base::Unretained(this), run_loop.QuitClosure()));
    overlay_window_views->GetLayerForTesting()->RequestCopyOfOutput(
        std::move(request));
    run_loop.Run();
  }

  bool CompareImages(const SkBitmap& actual_bmp, const SkBitmap& expected_bmp) {
    // Allowable error and thresholds because of small color shift by
    // video to image conversion and GPU issues.
    const int kAllowableError = 3;
    // Number of pixels with an error
    int error_pixels_count = 0;
    gfx::Rect error_bounding_rect;

    for (int x = 0; x < actual_bmp.width(); ++x) {
      for (int y = 0; y < actual_bmp.height(); ++y) {
        SkColor actual_color = actual_bmp.getColor(x, y);
        SkColor expected_color = expected_bmp.getColor(x, y);
        if ((fabs(SkColorGetR(actual_color) - SkColorGetR(expected_color)) >
             kAllowableError) ||
            (fabs(SkColorGetG(actual_color) - SkColorGetG(expected_color)) >
             kAllowableError) ||
            (fabs(SkColorGetB(actual_color) - SkColorGetB(expected_color))) >
                kAllowableError) {
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

  void Wait(base::TimeDelta timeout) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), timeout);
    run_loop.Run();
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
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(FILE_PATH_LITERAL(
                     "media/picture-in-picture/pixel_test.html")));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  SetUpWindowController(active_web_contents);
  ASSERT_NE(nullptr, window_controller());
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  OverlayWindowViews* overlay_window_views = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  overlay_window_views->SetSize(gfx::Size(402, 268));
  base::string16 expected_title = base::ASCIIToUTF16("resized");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
  Wait(base::TimeDelta::FromSeconds(3));
  TakeOverlayWindowScreenshot(overlay_window_views);

  SkBitmap expected_image;
  base::FilePath expected_image_path =
      GetFilePath(FILE_PATH_LITERAL("pixel_expected_video_play.png"));
  ASSERT_TRUE(ReadImageFile(expected_image_path, &expected_image));
  EXPECT_TRUE(CompareImages(GetResultBitmap(), expected_image));
}

// Plays a video in PiP. Trigger the play and pause control in PiP by using a
// mouse move. Capture the images and verift they are expected.
IN_PROC_BROWSER_TEST_F(PictureInPicturePixelComparisonBrowserTest,
                       PlayAndPauseControls) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(FILE_PATH_LITERAL(
                     "media/picture-in-picture/pixel_test.html")));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  OverlayWindowViews* overlay_window_views = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeVideoSrc();", &result));

  const int resize_width = 402, resize_height = 268;
  overlay_window_views->SetSize(gfx::Size(resize_width, resize_height));
  base::string16 expected_title = base::ASCIIToUTF16("resized");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  Wait(base::TimeDelta::FromSeconds(3));
  MoveMouseOver(overlay_window_views);
  TakeOverlayWindowScreenshot(overlay_window_views);

  base::FilePath expected_pause_image_path =
      GetFilePath(FILE_PATH_LITERAL("pixel_expected_pause_control.png"));
  base::FilePath expected_play_image_path =
      GetFilePath(FILE_PATH_LITERAL("pixel_expected_play_control.png"));
  // If the test image is cropped, usually off by 1 pixel, use another image.
  if (GetResultBitmap().width() < resize_width ||
      GetResultBitmap().height() < resize_height) {
    LOG(INFO) << "Actual image is cropped and backup images are used. "
              << "Test image dimension: "
              << "(" << GetResultBitmap().width() << "x"
              << GetResultBitmap().height() << "). "
              << "Expected image dimension: "
              << "(" << resize_width << "x" << resize_height << ")";
    expected_pause_image_path =
        GetFilePath(FILE_PATH_LITERAL("pixel_expected_pause_control_crop.png"));
    expected_play_image_path =
        GetFilePath(FILE_PATH_LITERAL("pixel_expected_play_control_crop.png"));
  }

  SkBitmap expected_image;
  ASSERT_TRUE(ReadImageFile(expected_pause_image_path, &expected_image));
  EXPECT_TRUE(CompareImages(GetResultBitmap(), expected_image));

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.pause();"));
  Wait(base::TimeDelta::FromSeconds(3));
  MoveMouseOver(overlay_window_views);
  TakeOverlayWindowScreenshot(overlay_window_views);
  ASSERT_TRUE(ReadImageFile(expected_play_image_path, &expected_image));
  EXPECT_TRUE(CompareImages(GetResultBitmap(), expected_image));
}
#endif  // !defined(OS_CHROMEOS)

// Tests that when an active WebContents accurately tracks whether a video
// is in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       TabIconUpdated) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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
  window_controller()->Close(true /* should_pause_video */);
  EXPECT_FALSE(active_web_contents->HasPictureInPictureVideo());

  // Reload page should not crash.
  ui_test_utils::NavigateToURL(browser(), test_page_url);
}

// Tests that when creating a Picture-in-Picture window a size is sent to the
// caller and if the window is resized, the caller is also notified.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResizeEventFired) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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

// Tests that when closing a Picture-in-Picture window, the video element is
// reflected as no longer in Picture-in-Picture.
// TODO(crbug.com/1001421): Flaky on ASan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CloseWindowWhilePlaying DISABLED_CloseWindowWhilePlaying
#else
#define MAYBE_CloseWindowWhilePlaying CloseWindowWhilePlaying
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MAYBE_CloseWindowWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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

  window_controller()->Close(true /* should_pause_video */);

  base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
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
      base::FilePath(kPictureInPictureWindowSizePage));
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
  window_controller()->Close(true /* should_pause_video */);

  base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when closing a Picture-in-Picture window, the video element
// no longer in Picture-in-Picture can't enter Picture-in-Picture right away.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowCantEnterPictureInPictureAgain) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(content::ExecuteScriptWithoutUserGesture(
      active_web_contents, "tryToEnterPictureInPictureAfterLeaving();"));

  bool in_picture_in_picture = false;
  EXPECT_TRUE(content::ExecuteScriptWithoutUserGestureAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  ASSERT_TRUE(window_controller());
  window_controller()->Close(true /* should_pause_video */);

  base::string16 expected_title =
      base::ASCIIToUTF16("failed to enter Picture-in-Picture after leaving");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when closing a Picture-in-Picture window from the Web API, the
// video element is not paused.
// Flaky on Linux and Windows: http://crbug.com/1001538
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       DISABLED_CloseWindowFromWebAPIWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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
  base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
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
// TODO(crbug.com/1001446): Flaky on ASan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_RequestPictureInPictureTwiceFromSameVideo \
  DISABLED_RequestPictureInPictureTwiceFromSameVideo
#else
#define MAYBE_RequestPictureInPictureTwiceFromSameVideo \
  RequestPictureInPictureTwiceFromSameVideo
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MAYBE_RequestPictureInPictureTwiceFromSameVideo) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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
  base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
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
// TODO(crbug.com/1001421): Flaky on ASan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_OpenSecondPictureInPictureStopsFirst \
  DISABLED_OpenSecondPictureInPictureStopsFirst
#else
#define MAYBE_OpenSecondPictureInPictureStopsFirst \
  OpenSecondPictureInPictureStopsFirst
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MAYBE_OpenSecondPictureInPictureStopsFirst) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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

  ExpectLeavePictureInPicture(active_web_contents);

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_TRUE(overlay_window->IsVisible());

  EXPECT_EQ(overlay_window->playback_state_for_testing(),
            OverlayWindowViews::PlaybackState::kPaused);
}

// Tests that resetting video src when video is in Picture-in-Picture session
// keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResetVideoSrcKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_TRUE(overlay_window->video_layer_for_testing()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.src = null;"));

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(overlay_window->video_layer_for_testing()->visible());
}

// Tests that updating video src when video is in Picture-in-Picture session
// keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       UpdateVideoSrcKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_TRUE(overlay_window->video_layer_for_testing()->visible());

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
  EXPECT_TRUE(overlay_window->video_layer_for_testing()->visible());
  EXPECT_FALSE(overlay_window->previous_track_controls_view_for_testing()
                   ->layer()
                   ->visible());
  EXPECT_FALSE(overlay_window->play_pause_controls_view_for_testing()
                   ->layer()
                   ->visible());
  EXPECT_FALSE(overlay_window->next_track_controls_view_for_testing()
                   ->layer()
                   ->visible());
}

// Tests that changing video src to media stream when video is in
// Picture-in-Picture session keep Picture-in-Picture window opened.
IN_PROC_BROWSER_TEST_F(
    PictureInPictureWindowControllerBrowserTest,
    ChangeVideoSrcToMediaStreamKeepsPictureInPictureWindowOpened) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_TRUE(overlay_window->video_layer_for_testing()->visible());

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
  EXPECT_TRUE(overlay_window->video_layer_for_testing()->visible());
  EXPECT_FALSE(overlay_window->previous_track_controls_view_for_testing()
                   ->layer()
                   ->visible());
  EXPECT_FALSE(overlay_window->play_pause_controls_view_for_testing()
                   ->layer()
                   ->visible());
  EXPECT_FALSE(overlay_window->next_track_controls_view_for_testing()
                   ->layer()
                   ->visible());
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
      base::FilePath(kPictureInPictureWindowSizePage));
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

  window_controller()->Close(true /* should_pause_video */);

  // Wait for the window to close.
  base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
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
  window_controller()->Close(true /* should_pause_video */);

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
      base::FilePath(kPictureInPictureWindowSizePage));
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

// Closing a tab that lost Picture-in-Picture because a new tab entered it
// should not close the current Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PictureInPictureDoNotCloseAfterClosingTab) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* initial_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(initial_web_contents != nullptr);

  SetUpWindowController(initial_web_contents);

  {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        initial_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Open a new tab in the browser and starts Picture-in-Picture.
  AddTabAtIndex(1, test_page_url, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_web_contents != nullptr);

  {
    content::PictureInPictureWindowController* pip_window_controller =
        content::PictureInPictureWindowController::GetOrCreateForWebContents(
            new_web_contents);

    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        new_web_contents, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);

    EXPECT_TRUE(pip_window_controller->GetWindowForTesting()->IsVisible());

    // 'left' is sent when the first tab leaves Picture-in-Picture.
    base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
    EXPECT_EQ(expected_title,
              content::TitleWatcher(initial_web_contents, expected_title)
                  .WaitAndGetTitle());
  }

  // Closing the initial tab should not get the new tab to leave
  // Picture-in-Picture.
  browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  base::RunLoop().RunUntilIdle();

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      new_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
}

// Killing an iframe that lost Picture-in-Picture because the main frame entered
// it should not close the current Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PictureInPictureDoNotCloseAfterKillingFrame) {
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

  {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        iframe, "enterPictureInPicture();", &result));
    EXPECT_TRUE(result);
  }

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  base::RunLoop().RunUntilIdle();

  {
    bool in_picture_in_picture = false;
    ASSERT_TRUE(
        ExecuteScriptAndExtractBool(iframe,
                                    "window.domAutomationController.send(!!"
                                    "document.pictureInPictureElement);",
                                    &in_picture_in_picture));
    EXPECT_FALSE(in_picture_in_picture);
  }

  // Removing the iframe should not lead to the main frame leaving
  // Picture-in-Picture.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, active_web_contents->GetAllFrames().size());
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  base::RunLoop().RunUntilIdle();

  {
    bool in_picture_in_picture = false;
    ASSERT_TRUE(
        ExecuteScriptAndExtractBool(active_web_contents,
                                    "window.domAutomationController.send(!!"
                                    "document.pictureInPictureElement);",
                                    &in_picture_in_picture));
    EXPECT_TRUE(in_picture_in_picture);
  }
}

// Checks setting disablePictureInPicture on video just after requesting
// Picture-in-Picture doesn't result in a window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       RequestPictureInPictureAfterDisablePictureInPicture) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::PictureInPictureWindowController* first_controller =
      window_controller();
  EXPECT_TRUE(first_controller->GetWindowForTesting()->IsVisible());

  Browser* second_browser = CreateBrowser(browser()->profile());
  LoadTabAndEnterPictureInPicture(
      second_browser, base::FilePath(kPictureInPictureWindowSizePage));

  content::PictureInPictureWindowController* second_controller =
      window_controller();
  EXPECT_FALSE(first_controller->GetWindowForTesting()->IsVisible());
  EXPECT_TRUE(second_controller->GetWindowForTesting()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterPictureInPictureThenFullscreen) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

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
      base::FilePath(kPictureInPictureWindowSizePage));
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
      base::FilePath(kPictureInPictureWindowSizePage));
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

// Tests that when a new surface id is sent to the Picture-in-Picture window, it
// doesn't move back to its default position.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       SurfaceIdChangeDoesNotMoveWindow) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

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

    overlay_window->SetSurfaceId(viz::SurfaceId(
        viz::FrameSinkId(1, 1),
        viz::LocalSurfaceId(9, base::UnguessableToken::Create())));
    overlay_window->UpdateVideoSize(gfx::Size(200, 100));

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
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);
  ASSERT_TRUE(overlay_window->IsVisible());

  // Simulate closing from the system.
  overlay_window->OnNativeWidgetDestroyed();

  ExpectLeavePictureInPicture(active_web_contents);
}

// Tests that the play/pause icon state is properly updated when a
// Picture-in-Picture is created after a reload.
// TODO(crbug.com/1001421): Flaky on ASan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PlayPauseStateAtCreation DISABLED_PlayPauseStateAtCreation
#else
#define MAYBE_PlayPauseStateAtCreation PlayPauseStateAtCreation
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MAYBE_PlayPauseStateAtCreation) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));

  bool is_paused = true;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());

  EXPECT_EQ(overlay_window->playback_state_for_testing(),
            OverlayWindowViews::PlaybackState::kPlaying);

  EXPECT_TRUE(overlay_window->video_layer_for_testing()->visible());

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

    EXPECT_EQ(overlay_window->playback_state_for_testing(),
              OverlayWindowViews::PlaybackState::kPaused);
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
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

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
  ExpectLeavePictureInPicture(
      browser()->tab_strip_model()->GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterUsingControllerThenEnterUsingWebContents) {
  auto* pip_window_manager = PictureInPictureWindowManager::GetInstance();
  ASSERT_NE(nullptr, pip_window_manager);

  // Show the non-WebContents based Picture-in-Picture window controller.
  EXPECT_CALL(mock_controller(), GetInitiatorWebContents())
      .WillOnce(testing::Return(nullptr));
  EXPECT_CALL(mock_controller(), Show());
  pip_window_manager->EnterPictureInPictureWithController(&mock_controller());

  // Now show the WebContents based Picture-in-Picture window controller.
  // This should close the existing window and show the new one.
  EXPECT_CALL(mock_controller(), Close(_));
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);
  EXPECT_TRUE(overlay_window->IsVisible());
}

// This checks that a video in Picture-in-Picture with preload none, when
// changing source willproperly update the associated media player id. This is
// checked by closing the window because the test it at a too high level to be
// able to check the actual media player id being used.
// TODO(crbug.com/1001421): Flaky on ASan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PreloadNoneSrcChangeThenLoad DISABLED_PreloadNoneSrcChangeThenLoad
#else
#define MAYBE_PreloadNoneSrcChangeThenLoad PreloadNoneSrcChangeThenLoad
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MAYBE_PreloadNoneSrcChangeThenLoad) {
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

  window_controller()->Close(true /* should_pause_video */);

  // The video should leave Picture-in-Picture mode.
  ExpectLeavePictureInPicture(active_web_contents);
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
      base::FilePath(kPictureInPictureWindowSizePage));
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

  ash::AcceleratorController::Get()->Process(
      ui::Accelerator(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_NONE));
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

// Tests that the back-to-tab, close, and resize controls move properly as
// the window changes quadrants.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MovingQuadrantsMovesBackToTabAndResizeControls) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
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
  // The resize button hit test should start a top left resizing drag.
  EXPECT_EQ(HTTOPLEFT, overlay_window_views->GetResizeHTComponent());

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
  // The resize button hit test should start a top right resizing drag.
  EXPECT_EQ(HTTOPRIGHT, overlay_window_views->GetResizeHTComponent());

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
  // The resize button hit test should start a bottom left resizing drag.
  EXPECT_EQ(HTBOTTOMLEFT, overlay_window_views->GetResizeHTComponent());

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
  // The resize button hit test should start a bottom right resizing drag.
  EXPECT_EQ(HTBOTTOMRIGHT, overlay_window_views->GetResizeHTComponent());
}

#endif  // defined(OS_CHROMEOS)

// Tests that the Play/Pause button is displayed appropriately in the
// Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PlayPauseButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  // Play/Pause button is displayed if video is not a mediastream.
  MoveMouseOver(overlay_window);
  EXPECT_TRUE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());

  // Play/Pause button is hidden if video is a mediastream.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeVideoSrcToMediaStream();", &result));
  EXPECT_TRUE(result);
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());

  // Play/Pause button is not hidden anymore when video is not a mediastream.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeVideoSrc();", &result));
  EXPECT_TRUE(result);
  MoveMouseOver(overlay_window);
  EXPECT_TRUE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());
}

// Check that page visibility API events are fired when tab is hidden, shown,
// and even occluded.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PageVisibilityEventsFired) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "addVisibilityChangeEventListener();"));

  // Hide page and check that the document visibility is hidden.
  active_web_contents->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Show page and check that the document visibility is visible.
  active_web_contents->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Occlude page and check that the document visibility is hidden.
  active_web_contents->WasOccluded();
  expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Check that page visibility API events are fired even when video is in
// Picture-in-Picture and video playback is not disrupted.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PageVisibilityEventsFiredWhenPictureInPicture) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));

  bool is_paused = true;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);

  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "addVisibilityChangeEventListener();"));

  // Hide page and check that the document visibility is hidden.
  active_web_contents->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Check that the video is still in Picture-in-Picture and playing.
  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
  ASSERT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);

  // Show page and check that the document visibility is visible.
  active_web_contents->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Check that the video is still in Picture-in-Picture and playing.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
  ASSERT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);

  // Occlude page and check that the document visibility is hidden.
  active_web_contents->WasOccluded();
  expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Check that the video is still in Picture-in-Picture and playing.
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
  ASSERT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);
}

class MediaSessionPictureInPictureWindowControllerBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaSession,SkipAd");
    scoped_feature_list_.InitWithFeatures(
        {media_session::features::kMediaSessionService}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a Skip Ad button is displayed in the Picture-in-Picture window
// when Media Session Action "skipad" is handled by the website.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       SkipAdButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  // Skip Ad button is not displayed initially when mouse is hovering over the
  // window.
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->skip_ad_controls_view_for_testing()->layer()->visible());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Skip Ad button is not displayed if video is not playing even if mouse is
  // hovering over the window and media session action handler has been set.
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('skipad');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->skip_ad_controls_view_for_testing()->layer()->visible());

  // Play video and check that Skip Ad button is now displayed when
  // video plays and mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_TRUE(
      overlay_window->skip_ad_controls_view_for_testing()->layer()->visible());

  // Unset action handler and check that Skip Ad button is not displayed when
  // video plays and mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "unsetMediaSessionActionHandler('skipad');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->skip_ad_controls_view_for_testing()->layer()->visible());
}

// Tests that the Play/Plause button is displayed in the Picture-in-Picture
// window when Media Session actions "play" and "pause" are handled by the
// website even if video is a media stream.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PlayPauseButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Play/Pause button is hidden if playing video is a mediastream and mouse is
  // hovering over the window.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeVideoSrcToMediaStream();", &result));
  EXPECT_TRUE(result);
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());

  // Play second video (non-muted) so that Media Session becomes active.
  ASSERT_TRUE(
      content::ExecuteScript(active_web_contents, "secondVideo.play();"));

  // Set Media Session action "play" handler and check that Play/Pause button
  // is still hidden when mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "setMediaSessionActionHandler('play');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());

  // Set Media Session action "pause" handler and check that Play/Pause button
  // is now displayed when mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "setMediaSessionActionHandler('pause');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_TRUE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());

  // Unset Media Session action "pause" handler and check that Play/Pause button
  // is hidden when mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "unsetMediaSessionActionHandler('pause');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());

  ASSERT_TRUE(
      content::ExecuteScript(active_web_contents, "exitPictureInPicture();"));

  // Reset Media Session action "pause" handler and check that Play/Pause button
  // is now displayed when mouse is hovering over the window when it enters
  // Picture-in-Picture again.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "setMediaSessionActionHandler('pause');"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_TRUE(
      overlay_window->play_pause_controls_view_for_testing()->IsDrawn());
}

// Tests that a Next Track button is displayed in the Picture-in-Picture window
// when Media Session Action "nexttrack" is handled by the website.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       NextTrackButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  // Next Track button is not displayed initially when mouse is hovering over
  // the window.
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->next_track_controls_view_for_testing()->IsDrawn());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Next Track button is not displayed if video is not playing even if mouse is
  // hovering over the window and media session action handler has been set.
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('nexttrack');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->next_track_controls_view_for_testing()->IsDrawn());

  // Play video and check that Next Track button is now displayed when
  // video plays and mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_TRUE(
      overlay_window->next_track_controls_view_for_testing()->IsDrawn());

  gfx::Rect next_track_bounds =
      overlay_window->next_track_controls_view_for_testing()
          ->GetBoundsInScreen();

  // Unset action handler and check that Next Track button is not displayed when
  // video plays and mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "unsetMediaSessionActionHandler('nexttrack');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->next_track_controls_view_for_testing()->IsDrawn());

  // Next Track button is still at the same previous location.
  EXPECT_EQ(next_track_bounds,
            overlay_window->next_track_controls_view_for_testing()
                ->GetBoundsInScreen());
}

// Tests that Next Track button bounds are updated right away when
// Picture-in-Picture window controls are hidden.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       NextTrackButtonBounds) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  gfx::Rect next_track_bounds =
      overlay_window->next_track_controls_view_for_testing()
          ->GetBoundsInScreen();

  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('nexttrack');"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(next_track_bounds,
            overlay_window->next_track_controls_view_for_testing()
                ->GetBoundsInScreen());
}

// Tests that a Previous Track button is displayed in the Picture-in-Picture
// window when Media Session Action "previoustrack" is handled by the website.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PreviousTrackButtonVisibility) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  // Previous Track button is not displayed initially when mouse is hovering
  // over the window.
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->previous_track_controls_view_for_testing()->IsDrawn());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Previous Track button is not displayed if video is not playing even if
  // mouse is hovering over the window and media session action handler has been
  // set.
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('previoustrack');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->previous_track_controls_view_for_testing()->IsDrawn());

  // Play video and check that Previous Track button is now displayed when
  // video plays and mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_TRUE(
      overlay_window->previous_track_controls_view_for_testing()->IsDrawn());

  gfx::Rect previous_track_bounds =
      overlay_window->previous_track_controls_view_for_testing()
          ->GetBoundsInScreen();

  // Unset action handler and check that Previous Track button is not displayed
  // when video plays and mouse is hovering over the window.
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "unsetMediaSessionActionHandler('previoustrack');"));
  base::RunLoop().RunUntilIdle();
  MoveMouseOver(overlay_window);
  EXPECT_FALSE(
      overlay_window->previous_track_controls_view_for_testing()->IsDrawn());
  // Previous Track button is still at the same previous location.
  EXPECT_EQ(previous_track_bounds,
            overlay_window->previous_track_controls_view_for_testing()
                ->GetBoundsInScreen());
}

// Tests that Previous Track button bounds are updated right away when
// Picture-in-Picture window controls are hidden.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PreviousTrackButtonBounds) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  gfx::Rect previous_track_bounds =
      overlay_window->previous_track_controls_view_for_testing()
          ->GetBoundsInScreen();

  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('previoustrack');"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(previous_track_bounds,
            overlay_window->previous_track_controls_view_for_testing()
                ->GetBoundsInScreen());
}

// Tests that clicking the Skip Ad button in the Picture-in-Picture window
// calls the Media Session Action "skipad" handler function.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       SkipAdHandlerCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('skipad');"));
  base::RunLoop().RunUntilIdle();

  // Simulates user clicking "Skip Ad" and check the handler function is called.
  window_controller()->SkipAd();
  base::string16 expected_title = base::ASCIIToUTF16("skipad");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that clicking the Play/Pause button in the Picture-in-Picture window
// calls the Media Session actions "play" and "pause" handler functions.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PlayPauseHandlersCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "setMediaSessionActionHandler('play');"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "setMediaSessionActionHandler('pause');"));
  base::RunLoop().RunUntilIdle();

  // Simulates user clicking "Play/Pause" and check that the "pause" handler
  // function is called.
  window_controller()->TogglePlayPause();
  base::string16 expected_title = base::ASCIIToUTF16("pause");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.pause();"));

  // Simulates user clicking "Play/Pause" and check that the "play" handler
  // function is called.
  window_controller()->TogglePlayPause();
  expected_title = base::ASCIIToUTF16("play");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that clicking the Next Track button in the Picture-in-Picture window
// calls the Media Session Action "nexttrack" handler function.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       NextTrackHandlerCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('nexttrack');"));
  base::RunLoop().RunUntilIdle();

  // Simulates user clicking "Next Track" and check the handler function is
  // called.
  window_controller()->NextTrack();
  base::string16 expected_title = base::ASCIIToUTF16("nexttrack");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that clicking the Previous Track button in the Picture-in-Picture
// window calls the Media Session Action "previoustrack" handler function.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       PreviousTrackHandlerCalled) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScript(
      active_web_contents, "setMediaSessionActionHandler('previoustrack');"));
  base::RunLoop().RunUntilIdle();

  // Simulates user clicking "Previous Track" and check the handler function is
  // called.
  window_controller()->PreviousTrack();
  base::string16 expected_title = base::ASCIIToUTF16("previoustrack");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that stopping Media Sessions closes the Picture-in-Picture window.
IN_PROC_BROWSER_TEST_F(MediaSessionPictureInPictureWindowControllerBrowserTest,
                       StopMediaSessionClosesPictureInPictureWindow) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  base::RunLoop().RunUntilIdle();

  content::MediaSession::Get(active_web_contents)
      ->Stop(content::MediaSession::SuspendType::kUI);
  base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

class AutoPictureInPictureWindowControllerBrowserTest
    : public PictureInPictureWindowControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PictureInPictureWindowControllerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "AutoPictureInPicture");
  }
};

// Hide page and check that entering Auto Picture-in-Picture is not triggered.
// This test is most likely going to be flaky the day the tested thing fails.
// Do NOT disable test. Ping /chrome/browser/picture_in_picture/OWNERS instead.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWindowControllerBrowserTest,
                       AutoEnterPictureInPictureIsNotTriggeredInRegularWebApp) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "video.autoPictureInPicture = true;"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "addVisibilityChangeEventListener();"));

  // Hide page and check that there is no video that enters Picture-in-Picture
  // automatically.
  active_web_contents->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);
}

// Show page and check that exiting Auto Picture-in-Picture is triggered.
// This test is most likely going to be flaky the day the tested thing fails.
// Do NOT disable test. Ping /chrome/browser/picture_in_picture/OWNERS instead.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWindowControllerBrowserTest,
                       AutoExitPictureInPictureIsTriggeredInRegularWebApp) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "video.autoPictureInPicture = true;"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "addVisibilityChangeEventListener();"));

  // Hide page.
  active_web_contents->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  // Enter Picture-in-Picture manually.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  // Show page and check that video left Picture-in-Picture automatically.
  active_web_contents->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);
}

// Show/hide fullscreen page and check that Auto Picture-in-Picture is
// triggered.
IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPictureTriggeredWhenFullscreen) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "enterFullscreen()"));
  base::string16 expected_title = base::ASCIIToUTF16("fullscreen");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents,
                                     "addPictureInPictureEventListeners();"));
  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "video.autoPictureInPicture = true;"));

  SetUpWindowController(active_web_contents);
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());

  // Hide page and check that video entered Picture-in-Picture automatically.
  active_web_contents->WasHidden();
  expected_title = base::ASCIIToUTF16("enterpictureinpicture");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Show page and check that video left Picture-in-Picture automatically.
  active_web_contents->WasShown();
  expected_title = base::ASCIIToUTF16("leavepictureinpicture");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

namespace {

class ChromeContentBrowserClientOverrideWebAppScope
    : public ChromeContentBrowserClient {
 public:
  ChromeContentBrowserClientOverrideWebAppScope() = default;
  ~ChromeContentBrowserClientOverrideWebAppScope() override = default;

  void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                           content::WebPreferences* web_prefs) override {
    ChromeContentBrowserClient::OverrideWebkitPrefs(rvh, web_prefs);

    web_prefs->web_app_scope = web_app_scope_;
  }

  void set_web_app_scope(const GURL& web_app_scope) {
    web_app_scope_ = web_app_scope;
  }

 private:
  GURL web_app_scope_;
};

}  // namespace

class WebAppPictureInPictureWindowControllerBrowserTest
    : public extensions::ExtensionBrowserTest {
 public:
  WebAppPictureInPictureWindowControllerBrowserTest() = default;
  ~WebAppPictureInPictureWindowControllerBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void InstallAndLaunchPWA() {
    // Install PWA
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL app_url = embedded_test_server()->GetURL(
        "/extensions/auto_picture_in_picture/main.html");
    WebApplicationInfo web_app_info;
    web_app_info.app_url = app_url;
    web_app_info.scope = app_url.GetWithoutFilename();
    web_app_info.open_as_window = true;
    const extensions::Extension* extension =
        extensions::browsertest_util::InstallBookmarkApp(
            browser()->profile(), std::move(web_app_info));
    ASSERT_TRUE(extension);

    // Launch PWA
    ui_test_utils::UrlLoadObserver url_observer(
        app_url, content::NotificationService::AllSources());
    Browser* app_browser = extensions::browsertest_util::LaunchAppBrowser(
        browser()->profile(), extension);
    url_observer.Wait();

    web_contents_ = app_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(web_contents_));
    ASSERT_NE(nullptr, web_contents_);

    SetWebAppScope(app_url.GetOrigin());
  }

  void SetWebAppScope(const GURL web_app_scope) {
    ChromeContentBrowserClientOverrideWebAppScope browser_client_;
    browser_client_.set_web_app_scope(web_app_scope);

    content::ContentBrowserClient* original_browser_client_ =
        content::SetBrowserClientForTesting(&browser_client_);

    web_contents_->GetRenderViewHost()->OnWebkitPreferencesChanged();

    content::SetBrowserClientForTesting(original_browser_client_);
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::WebContents* web_contents_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebAppPictureInPictureWindowControllerBrowserTest);
};

// Show/hide pwa page and check that Auto Picture-in-Picture is triggered.
IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPicture) {
  InstallAndLaunchPWA();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(),
                                                   "playVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = true;"));

  // Hide page and check that video entered Picture-in-Picture automatically.
  web_contents()->WasHidden();
  base::string16 expected_title =
      base::ASCIIToUTF16("video.enterpictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Show page and check that video left Picture-in-Picture automatically.
  web_contents()->WasShown();
  expected_title = base::ASCIIToUTF16("video.leavepictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());
}

// Show pwa page and check that Auto Picture-in-Picture is not triggered if
// document is not inside the scope specified in the Web App Manifest.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureNotTriggeredIfDocumentNotInWebAppScope) {
  InstallAndLaunchPWA();
  SetWebAppScope(GURL("http://www.foobar.com"));
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(),
                                                   "playVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = true;"));

  // Hide page and check that the video did not entered
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);
}

// Show pwa page and check that Auto Picture-in-Picture is not triggered if
// video is not playing.
IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       AutoPictureInPictureNotTriggeredIfVideoNotPlaying) {
  InstallAndLaunchPWA();
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = true;"));
  bool is_paused = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(web_contents(), "isPaused();", &is_paused));
  EXPECT_TRUE(is_paused);

  // Hide page and check that the video did not entered
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);
}

// Check that Auto Picture-in-Picture is not triggered if there's already a
// video in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureWhenPictureInPictureWindowAlreadyVisible) {
  InstallAndLaunchPWA();

  // Enter Picture-in-Picture for the first video and set Auto
  // Picture-in-Picture for the second video.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(),
                                                   "playVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "playSecondVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(), "secondVideo.autoPictureInPicture = true;"));

  // Hide page and check that the second video did not entered
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Check that the first video is still in Picture-in-Picture.
  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
}

// Check that video does not leave Picture-in-Picture automatically when it
// doesn't have the Auto Picture-in-Picture attribute set.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureNotTriggeredOnPageShownIfNoAttribute) {
  InstallAndLaunchPWA();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(),
                                                   "playVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = true;"));

  // Hide page and check that video entered Picture-in-Picture automatically.
  web_contents()->WasHidden();
  base::string16 expected_title =
      base::ASCIIToUTF16("video.enterpictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = false;"));

  // Show page and check that video did not leave Picture-in-Picture
  // automatically as it doesn't have the Auto Picture-in-Picture attribute set
  // anymore.
  web_contents()->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Check that the video is still in Picture-in-Picture.
  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
}

// TODO(http://crbug/1001249): flaky.
// Check that Auto Picture-in-Picture applies only to the video element whose
// autoPictureInPicture attribute was set most recently
IN_PROC_BROWSER_TEST_F(WebAppPictureInPictureWindowControllerBrowserTest,
                       DISABLED_AutoPictureInPictureAttributeApplies) {
  InstallAndLaunchPWA();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(),
                                                   "playVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = true;"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "playSecondVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(), "secondVideo.autoPictureInPicture = true;"));

  // Hide page and check that second video is the video that enters
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  base::string16 expected_title =
      base::ASCIIToUTF16("secondVideo.enterpictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Show page and unset Auto Picture-in-Picture attribute on second video.
  web_contents()->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(), "secondVideo.autoPictureInPicture = false;"));

  // Hide page and check that first video is the video that enters
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  expected_title = base::ASCIIToUTF16("video.enterpictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Show page and unset Auto Picture-in-Picture attribute on first video.
  web_contents()->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = false;"));

  // Hide page and check that there is no video that enters Picture-in-Picture
  // automatically.
  web_contents()->WasHidden();
  expected_title = base::ASCIIToUTF16("hidden");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Show page and append a video with Auto Picture-in-Picture attribute.
  web_contents()->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "addHtmlVideoWithAutoPictureInPicture();", &result));
  ASSERT_TRUE(result);

  // Hide page and check that the html video is the video that enters
  // Picture-in-Picture automatically.
  web_contents()->WasHidden();
  expected_title = base::ASCIIToUTF16("htmlVideo.enterpictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());
}

// Check that video does not leave Picture-in-Picture automatically when it
// not the most recent element with the Auto Picture-in-Picture attribute set.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureNotTriggeredOnPageShownIfNotEnteredAutoPictureInPicture) {
  InstallAndLaunchPWA();
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents(),
                                                   "playVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = true;"));

  // Hide page and check that video entered Picture-in-Picture automatically.
  web_contents()->WasHidden();
  base::string16 expected_title =
      base::ASCIIToUTF16("video.enterpictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "playSecondVideo();", &result));
  ASSERT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(), "secondVideo.autoPictureInPicture = true;"));

  // Show page and check that video did not leave Picture-in-Picture
  // automatically as it's not the most recent element with the Auto
  // Picture-in-Picture attribute set anymore.
  web_contents()->WasShown();
  expected_title = base::ASCIIToUTF16("visible");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Check that the video is still in Picture-in-Picture.
  bool in_picture_in_picture = false;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      web_contents(), "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);
}

// Check that video with no audio that is paused when hidden is still eligible
// to enter Auto Picture-in-Picture and resumes playback.
IN_PROC_BROWSER_TEST_F(
    WebAppPictureInPictureWindowControllerBrowserTest,
    AutoPictureInPictureTriggeredOnPageHiddenIfVideoPausedWhenHidden) {
  InstallAndLaunchPWA();

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents(), "changeVideoSrcToNoAudioTrackVideo();", &result));
  EXPECT_TRUE(result);
  ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                     "video.autoPictureInPicture = true;"));

  // Hide page and check that video entered Picture-in-Picture automatically
  // and is playing.
  web_contents()->WasHidden();
  base::string16 expected_title =
      base::ASCIIToUTF16("video.enterpictureinpicture");
  EXPECT_EQ(
      expected_title,
      content::TitleWatcher(web_contents(), expected_title).WaitAndGetTitle());

  // Check that video playback is still playing.
  bool is_paused = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(web_contents(), "isPaused();", &is_paused));
  EXPECT_FALSE(is_paused);
}

// Check that video with no audio that is paused when hidden resumes playback
// when it enters Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       VideoWithNoAudioPausedWhenHiddenResumesPlayback) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kPictureInPictureWindowSizePage));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "changeVideoSrcToNoAudioTrackVideo();", &result));
  EXPECT_TRUE(result);

  ASSERT_TRUE(
      content::ExecuteScript(active_web_contents, "addPauseEventListener();"));

  // Hide page and check that the video is paused first.
  active_web_contents->WasHidden();
  base::string16 expected_title = base::ASCIIToUTF16("pause");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  ASSERT_TRUE(
      content::ExecuteScript(active_web_contents, "addPlayEventListener();"));

  // Enter Picture-in-Picture.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "enterPictureInPicture();", &result));
  EXPECT_TRUE(result);

  // Check that video playback has resumed.
  expected_title = base::ASCIIToUTF16("play");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that exiting Picture-in-Picture when the video has no source fires the
// event and resolves the callback.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ExitFireEventAndCallbackWhenNoSource) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents,
                                     "video.src=''; exitPictureInPicture();"));

  // 'left' is sent when the first video leaves Picture-in-Picture.
  base::string16 expected_title = base::ASCIIToUTF16("leavepictureinpicture");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when closing the window after the player was reset, the <video>
// element is still notified.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResetPlayerCloseWindowNotifiesElement) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Video should be in Picture-in-Picture.
  {
    bool in_picture_in_picture = false;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(active_web_contents,
                                            "isInPictureInPicture();",
                                            &in_picture_in_picture));
    EXPECT_TRUE(in_picture_in_picture);
  }

  // Reset video source and wait for the notification.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "resetVideo();"));
  base::string16 expected_title = base::ASCIIToUTF16("emptied");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  window_controller()->Close(true /* should_pause_video */);

  // Video should no longer be in Picture-in-Picture.
  ExpectLeavePictureInPicture(active_web_contents);
}

// TODO(crbug.com/1002489): Test is flaky on Linux.
#if defined(OS_LINUX)
#define MAYBE_UpdateMaxSize DISABLED_UpdateMaxSize
#else
#define MAYBE_UpdateMaxSize UpdateMaxSize
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MAYBE_UpdateMaxSize) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(nullptr, active_web_contents);

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);

  // Size should be half the work area.
  gfx::Size window_size(100, 100);
  window_size = overlay_window->UpdateMaxSize(gfx::Rect(100, 100), window_size);
  EXPECT_EQ(gfx::Size(50, 50), window_size);
  EXPECT_EQ(gfx::Size(50, 50), overlay_window->GetMaximumSize());

  // If the max size increases then we should keep the existing window size.
  window_size = overlay_window->UpdateMaxSize(gfx::Rect(200, 200), window_size);
  EXPECT_EQ(gfx::Size(50, 50), window_size);
  EXPECT_EQ(gfx::Size(100, 100), overlay_window->GetMaximumSize());

  // If the max size decreases then we should shrink to fit.
  window_size = overlay_window->UpdateMaxSize(gfx::Rect(50, 50), window_size);
  EXPECT_EQ(gfx::Size(25, 25), window_size);
  EXPECT_EQ(gfx::Size(25, 25), overlay_window->GetMaximumSize());
}

// Tests that play/pause video playback is toggled if there are no focus
// afforfances on the Picture-in-Picture window buttons when user hits space
// keyboard key.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       SpaceKeyTogglePlayPause) {
  LoadTabAndEnterPictureInPicture(
      browser(), base::FilePath(kPictureInPictureWindowSizePage));

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(
      content::ExecuteScript(active_web_contents, "addPauseEventListener();"));

  OverlayWindowViews* overlay_window = static_cast<OverlayWindowViews*>(
      window_controller()->GetWindowForTesting());
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->GetFocusManager()->GetFocusedView());

  ui::KeyEvent space_key_pressed(ui::ET_KEY_PRESSED, ui::VKEY_SPACE,
                                 ui::DomCode::SPACE, ui::EF_NONE);
  overlay_window->OnKeyEvent(&space_key_pressed);

  base::string16 expected_title = base::ASCIIToUTF16("pause");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}
