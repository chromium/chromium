// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/media_buildflags.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#endif

// Window capture through the getDisplayMedia() backend is available on Aura
// and Mac.
#if BUILDFLAG(ENABLE_LIBAOM) && BUILDFLAG(ENABLE_SCREEN_CAPTURE) && \
    (defined(USE_AURA) || BUILDFLAG(IS_MAC))

namespace {

// A page that is fully red and repaints periodically, so that the compositor
// keeps producing frames during the recording and the recording can be checked
// for pixels of the inspected page.
constexpr char kRedPageUrl[] =
    "data:text/html,<body style='margin:0;background:rgb(255,0,0)'>"
    "<script>"
    "let toggle = false;"
    "setInterval(() => {"
    "  toggle = !toggle;"
    "  document.body.style.background ="
    "      toggle ? 'rgb(255,0,0)' : 'rgb(254,0,0)';"
    "}, 250);"
    "</script></body>";

// Plays `mp4` in a <video> element and decodes frames at several points in
// time. For each frame, measures the fraction of its pixels that are clearly
// red or black. Returns the video size, the samples, and the values for the
// last frame, or null if the video cannot be decoded.
constexpr char kAnalyzeVideoScript[] = R"(
  (async () => {
    const video = document.createElement('video');
    video.muted = true;
    video.src = 'data:video/mp4;base64,' + $1;
    document.body.appendChild(video);
    const seek = async (time) => {
      video.currentTime = time;
      await new Promise(resolve => video.onseeked = resolve);
    };
    try {
      await new Promise((resolve, reject) => {
        video.onloadeddata = resolve;
        video.onerror = () => reject(video.error);
      });
    } catch (e) {
      return null;
    }
    const canvas = document.createElement('canvas');
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
    const context = canvas.getContext('2d');
    const measure = () => {
      context.drawImage(video, 0, 0);
      const {data} = context.getImageData(0, 0, canvas.width, canvas.height);
      let red = 0;
      let black = 0;
      for (let i = 0; i < data.length; i += 4) {
        if (data[i] > 200 && data[i + 1] < 60 && data[i + 2] < 60) {
          ++red;
        }
        if (data[i] < 20 && data[i + 1] < 20 && data[i + 2] < 20) {
          ++black;
        }
      }
      const pixels = data.length / 4;
      return {red: red / pixels, black: black / pixels};
    };
    const end = Math.max(0, video.duration - 0.001);
    const samples = [];
    const kSampleCount = 10;
    for (let i = 0; i < kSampleCount; ++i) {
      const time = end * i / (kSampleCount - 1);
      await seek(time);
      samples.push({time, ...measure()});
    }
    const last = samples[samples.length - 1];
    return {width: video.videoWidth, height: video.videoHeight,
            duration: video.duration, red: last.red, black: last.black,
            maxRed: Math.max(...samples.map(sample => sample.red)),
            samples: samples.map(sample =>
                sample.time.toFixed(2) + 's: red ' + sample.red.toFixed(2) +
                ', black ' + sample.black.toFixed(2)).join('; ')};
  })()
)";

// The number of animation frames that the inspected page produces after the
// recording starts.
constexpr int kAnimationFrameCount = 30;

// The thresholds in the tests assume the default light theme and the default
// sizes of the browser window and of docked DevTools:
// - The red fraction depends on how much of the window the page fills. The
//   browser UI and the DevTools UI do not use saturated red.
// - The black fraction counts near-black pixels. A dark theme can add such
//   pixels, and so can the transparent border for the frame shadow, which the
//   window has on Linux with Wayland. The failure messages show it, but no
//   test checks it.
// If a test fails after a change of the window layout or the theme, check these
// assumptions before you suspect the capture.

struct VideoAnalysis {
  int width = 0;
  int height = 0;
  double duration = 0;
  // The values for the last frame.
  double red = 0;
  double black = 0;
  // The largest red fraction of all sampled frames.
  double max_red = 0;
  std::string samples;
};

// Describes `analysis` in failure messages.
std::ostream& operator<<(std::ostream& os, const VideoAnalysis& analysis) {
  return os << "size " << analysis.width << "x" << analysis.height
            << ", duration " << analysis.duration << "s, samples ["
            << analysis.samples << "]";
}

class DevToolsWindowRecordingTest : public DevToolsProtocolTestBase {
 protected:
  void SetUp() override {
    // The pixel checks need real pixels in the captured frames.
    EnablePixelOutput();
    DevToolsProtocolTestBase::SetUp();
  }

  content::WebContents* web_contents() override {
    return target_web_contents_ ? target_web_contents_.get()
                                : DevToolsProtocolTestBase::web_contents();
  }

  content::WebContents* inspected_web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // Waits until the renderer of the inspected page has painted two more
  // frames.
  void WaitForInspectedPagePaint() {
    EXPECT_TRUE(content::ExecJs(
        inspected_web_contents(),
        "new Promise(resolve => "
        "requestAnimationFrame(() => requestAnimationFrame(resolve)))"));
  }

  // Changes the background of the inspected page in each of the next `count`
  // animation frames, between two colors that the analysis counts as red.
  // Each change damages the page, so that the compositor produces new frames
  // that the capture device can record.
  void AnimateInspectedPage(int count) {
    EXPECT_TRUE(content::ExecJs(
        inspected_web_contents(),
        content::JsReplace(
            "new Promise(resolve => {"
            "  let remaining = $1;"
            "  const step = () => {"
            "    document.body.style.background ="
            "        remaining % 2 ? 'rgb(255,0,0)' : 'rgb(250,0,0)';"
            "    if (--remaining > 0) {"
            "      requestAnimationFrame(step);"
            "    } else {"
            "      requestAnimationFrame(() => resolve());"
            "    }"
            "  };"
            "  requestAnimationFrame(step);"
            "})",
            count)));
  }

  void NavigateToRedPage() {
    EXPECT_TRUE(
        content::NavigateToURL(inspected_web_contents(), GURL(kRedPageUrl)));
    // Wait until the renderer has painted the red background.
    WaitForInspectedPagePaint();
  }

  DevToolsWindow* OpenDevTools(bool is_docked) {
    NavigateToRedPage();
    DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
        inspected_web_contents(), is_docked);
    target_web_contents_ =
        DevToolsWindowTesting::Get(window)->main_web_contents();
    // Docked DevTools make the page smaller. Wait until the page has painted
    // at the new size, so that the first recorded frame shows the page.
    WaitForInspectedPagePaint();
    return window;
  }

  void CloseDevTools(DevToolsWindow* window) {
    DetachProtocolClient();
    target_web_contents_ = nullptr;
    DevToolsWindowTesting::CloseDevToolsWindowSync(window);
  }

  // Starts a recording of the browser window and returns the stream handle,
  // or std::nullopt if Page.startScreenRecording fails.
  std::optional<std::string> StartRecording() {
    base::DictValue params;
    params.Set("maxWidth", 3840);
    params.Set("maxHeight", 2160);
    params.Set("captureArea", "browserWindow");
    const base::DictValue* result =
        SendCommandSync("Page.startScreenRecording", std::move(params));
    if (!result) {
      return std::nullopt;
    }
    std::string stream = *result->FindString("stream");
    // The first frames of a window capture can be black. Let the page produce
    // more frames, so that the recording has frames that show the page.
    AnimateInspectedPage(kAnimationFrameCount);
    return stream;
  }

  // Stops the recording and returns the MP4 data.
  std::string StopRecording(const std::string& handle) {
    EXPECT_TRUE(SendCommandSync("Page.stopScreenRecording"));
    std::string mp4;
    while (true) {
      base::DictValue read_params;
      read_params.Set("handle", handle);
      read_params.Set("size", 1 << 20);
      const base::DictValue* read_result =
          SendCommandSync("IO.read", std::move(read_params));
      if (!read_result) {
        ADD_FAILURE() << "IO.read failed";
        break;
      }
      if (const std::string* data = read_result->FindString("data")) {
        if (read_result->FindBool("base64Encoded").value_or(false)) {
          std::string decoded;
          EXPECT_TRUE(base::Base64Decode(*data, &decoded));
          mp4 += decoded;
        } else {
          mp4 += *data;
        }
      }
      if (read_result->FindBool("eof").value_or(false)) {
        break;
      }
    }
    base::DictValue close_params;
    close_params.Set("handle", handle);
    SendCommandSync("IO.close", std::move(close_params));
    return mp4;
  }

  // Saves `mp4` for manual inspection if the
  // DEVTOOLS_SCREEN_RECORDING_OUTPUT_DIR environment variable is set.
  void MaybeSave(const std::string& mp4, const std::string& name) {
    std::optional<std::string> dir = base::Environment::Create()->GetVar(
        "DEVTOOLS_SCREEN_RECORDING_OUTPUT_DIR");
    if (!dir || dir->empty()) {
      return;
    }
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(
        base::FilePath::FromUTF8Unsafe(*dir).AppendASCII(name + ".mp4"), mp4));
  }

  // Decodes frames of `mp4`. Adds a test failure if that fails.
  VideoAnalysis Analyze(const std::string& mp4) {
    content::EvalJsResult result = content::EvalJs(
        inspected_web_contents(),
        content::JsReplace(kAnalyzeVideoScript, base::Base64Encode(mp4)));
    VideoAnalysis analysis;
    const base::Value value = std::move(result).TakeValue();
    if (!value.is_dict()) {
      ADD_FAILURE() << "The recording cannot be decoded";
      return analysis;
    }
    analysis.width = value.GetDict().FindInt("width").value_or(0);
    analysis.height = value.GetDict().FindInt("height").value_or(0);
    analysis.duration = value.GetDict().FindDouble("duration").value_or(0);
    analysis.red = value.GetDict().FindDouble("red").value_or(0);
    analysis.black = value.GetDict().FindDouble("black").value_or(0);
    analysis.max_red = value.GetDict().FindDouble("maxRed").value_or(0);
    if (const std::string* samples = value.GetDict().FindString("samples")) {
      analysis.samples = *samples;
    }
    return analysis;
  }

  MediaStreamCaptureIndicator* indicator() {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->GetMediaStreamCaptureIndicator()
        .get();
  }

  raw_ptr<content::WebContents> target_web_contents_ = nullptr;
};

// Checks that a docked DevTools target records the whole browser window:
// the toolbar, the DevTools UI and the inspected page.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, DockedDevToolsTarget) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  Attach();
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle) << "Page.startScreenRecording failed";
  std::string mp4 = StopRecording(*handle);
  MaybeSave(mp4, "browser_window");

  VideoAnalysis analysis = Analyze(mp4);
  gfx::Rect devtools_bounds = target_web_contents_->GetContainerBounds();
  // The page is in the video, together with DevTools and the browser UI.
  EXPECT_GT(analysis.max_red, 0.1) << analysis;
  EXPECT_LT(analysis.max_red, 0.8) << analysis;
  // The window is taller than the contents area, because it also has the
  // tab strip and the toolbar.
  EXPECT_GT(analysis.height, devtools_bounds.height());
  CloseDevTools(window);
}

// Checks that the recording registers with the capture indicator like
// getDisplayMedia() window capture, and unregisters when it stops.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, Indicator) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  Attach();
  EXPECT_FALSE(indicator()->IsCapturingWindow(inspected_web_contents()));
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle);
  EXPECT_TRUE(indicator()->IsCapturingWindow(inspected_web_contents()));
  StopRecording(*handle);
  EXPECT_FALSE(indicator()->IsCapturingWindow(inspected_web_contents()));
  CloseDevTools(window);
}

// Checks that Stop in the screen sharing UI stops the capture, but that
// Page.stopScreenRecording still returns the recording.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, StopFromUI) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  Attach();
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle);

  // This is what the Stop button of the screen sharing UI does.
  indicator()->StopMediaCapturing(inspected_web_contents(),
                                  MediaStreamCaptureIndicator::kDisplayMedia);
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !indicator()->IsCapturingWindow(inspected_web_contents());
  }));

  std::string mp4 = StopRecording(*handle);
  MaybeSave(mp4, "stop_from_ui");
  VideoAnalysis analysis = Analyze(mp4);
  EXPECT_GT(analysis.max_red, 0.1) << analysis;
  CloseDevTools(window);
}

// Checks that an undocked DevTools target returns an error, because the
// inspected page is in another window.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, UndockedDevToolsTarget) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/false);
  Attach();
  EXPECT_FALSE(StartRecording());
  ASSERT_TRUE(error());
  CloseDevTools(window);
}

// Checks that untrusted clients cannot record the browser window.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, UntrustedClient) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  target_web_contents_ = inspected_web_contents();
  SetIsTrusted(false);
  Attach();
  EXPECT_FALSE(StartRecording());
  ASSERT_TRUE(error());
  CloseDevTools(window);
}

// Checks that the policy that disables screen capture also disables the
// recording of the browser window.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, PolicyDisallowsCapture) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  Profile::FromBrowserContext(inspected_web_contents()->GetBrowserContext())
      ->GetPrefs()
      ->SetBoolean(prefs::kScreenCaptureAllowed, false);
  Attach();
  EXPECT_FALSE(StartRecording());
  ASSERT_TRUE(error());
  CloseDevTools(window);
}

// Checks how the per-origin policy lists apply. The recording uses an empty
// origin, because a CDP client and not the inspected page asks for it. A
// pattern for a specific origin does not match the empty origin, but the "*"
// pattern matches all origins, so it allows the recording.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, PolicyOriginLists) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  PrefService* prefs =
      Profile::FromBrowserContext(inspected_web_contents()->GetBrowserContext())
          ->GetPrefs();
  prefs->SetBoolean(prefs::kScreenCaptureAllowed, false);
  base::ListValue specific_origin;
  specific_origin.Append("https://example.com");
  prefs->SetList(prefs::kWindowCaptureAllowedByOrigins,
                 std::move(specific_origin));
  Attach();
  EXPECT_FALSE(StartRecording());
  ASSERT_TRUE(error());

  base::ListValue all_origins;
  all_origins.Append("*");
  prefs->SetList(prefs::kWindowCaptureAllowedByOrigins, std::move(all_origins));
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle) << "Page.startScreenRecording failed";
  StopRecording(*handle);
  CloseDevTools(window);
}

// Checks that a tab target records its browser window too.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, TabTarget) {
  NavigateToRedPage();
  Attach();
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle);
  std::string mp4 = StopRecording(*handle);
  MaybeSave(mp4, "tab_target");
  VideoAnalysis analysis = Analyze(mp4);
  EXPECT_GT(analysis.max_red, 0.3) << analysis;
  EXPECT_GT(analysis.height,
            inspected_web_contents()->GetContainerBounds().height());
}

// Checks that the inspected page target records the same browser window as the
// docked DevTools target, with DevTools in it. Audio comes from the target, so
// a client attaches to the page target to record the page's audio.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest,
                       TabTargetWithDockedDevTools) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  gfx::Rect devtools_bounds = target_web_contents_->GetContainerBounds();
  // Attach to the inspected page instead of DevTools.
  target_web_contents_ = nullptr;
  Attach();
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle) << "Page.startScreenRecording failed";
  std::string mp4 = StopRecording(*handle);
  MaybeSave(mp4, "tab_target_with_docked_devtools");

  VideoAnalysis analysis = Analyze(mp4);
  // The page is in the video, together with DevTools and the browser UI.
  EXPECT_GT(analysis.max_red, 0.1) << analysis;
  EXPECT_LT(analysis.max_red, 0.8) << analysis;
  EXPECT_GT(analysis.height, devtools_bounds.height());
  CloseDevTools(window);
}

#if !BUILDFLAG(IS_CHROMEOS)
// Returns the visible Stop button of the screen sharing notification, or
// nullptr. ChromeOS shows the notification in the system tray instead.
views::MdTextButton* FindNotificationStopButton() {
  for (views::Widget* widget : views::test::WidgetTest::GetAllWidgets()) {
    if (widget->GetName() != "ScreenCaptureNotificationUIViews") {
      continue;
    }
    std::vector<views::View*> views = {widget->GetRootView()};
    while (!views.empty()) {
      views::View* view = views.back();
      views.pop_back();
      // Match the text, because the notification has other buttons, for
      // example the hidden "Change source" button.
      auto* button = views::AsViewClass<views::MdTextButton>(view);
      if (button && button->GetVisible() &&
          button->GetText() ==
              l10n_util::GetStringUTF16(
                  IDS_MEDIA_SCREEN_CAPTURE_NOTIFICATION_STOP)) {
        return button;
      }
      for (views::View* child : view->children()) {
        views.push_back(child);
      }
    }
  }
  return nullptr;
}

// Checks that a click on the real Stop button of the notification stops the
// capture without crashing, and that the recording keeps the frames until
// then.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, StopButtonInNotification) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  Attach();
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle);

  views::MdTextButton* stop_button = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&] {
    stop_button = FindNotificationStopButton();
    return stop_button != nullptr;
  }));
  views::test::ButtonTestApi(stop_button).NotifyDefaultMouseClick();
  EXPECT_TRUE(base::test::RunUntil([&] {
    return !indicator()->IsCapturingWindow(inspected_web_contents());
  }));
  EXPECT_FALSE(FindNotificationStopButton());

  std::string mp4 = StopRecording(*handle);
  MaybeSave(mp4, "stop_button_in_notification");
  VideoAnalysis analysis = Analyze(mp4);
  EXPECT_GT(analysis.max_red, 0.1) << analysis;
  CloseDevTools(window);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Checks that the video follows the size of the browser window when the window
// is resized, instead of letterboxing it into the initial size.
IN_PROC_BROWSER_TEST_F(DevToolsWindowRecordingTest, Resize) {
  DevToolsWindow* window = OpenDevTools(/*is_docked=*/true);
  Attach();
  std::optional<std::string> handle = StartRecording();
  ASSERT_TRUE(handle);

  views::Widget* browser_widget = views::Widget::GetWidgetForNativeWindow(
      inspected_web_contents()->GetTopLevelNativeWindow());
  ASSERT_TRUE(browser_widget);
  gfx::Rect bounds = browser_widget->GetWindowBoundsInScreen();
  const int initial_page_width =
      inspected_web_contents()->GetContainerBounds().width();
  // Change the aspect ratio, so that a fixed frame size would show letterbox
  // bars.
  browser_widget->SetBounds(gfx::Rect(
      bounds.origin(), gfx::Size(bounds.width() * 3 / 5, bounds.height())));
  EXPECT_TRUE(base::test::RunUntil([&] {
    return inspected_web_contents()->GetContainerBounds().width() <
           initial_page_width;
  }));
  const gfx::Rect resized_bounds = browser_widget->GetWindowBoundsInScreen();
  NavigateToRedPage();
  AnimateInspectedPage(kAnimationFrameCount);
  std::string mp4 = StopRecording(*handle);
  MaybeSave(mp4, "resize");

  VideoAnalysis analysis = Analyze(mp4);
  ASSERT_GT(analysis.height, 0) << analysis;
  // The last frame has the aspect ratio of the resized window. A frame that
  // keeps the initial size would have the initial aspect ratio, with the
  // window in it between letterbox bars. Do not count black pixels, because
  // on Linux the window can have a transparent border for the frame shadow,
  // which is black in the video.
  const double video_aspect_ratio =
      static_cast<double>(analysis.width) / analysis.height;
  const double resized_aspect_ratio =
      static_cast<double>(resized_bounds.width()) / resized_bounds.height();
  const double initial_aspect_ratio =
      static_cast<double>(bounds.width()) / bounds.height();
  EXPECT_NEAR(video_aspect_ratio, resized_aspect_ratio, 0.05)
      << analysis << ", resized window " << resized_bounds.ToString()
      << ", initial window " << bounds.ToString();
  EXPECT_GT(std::abs(video_aspect_ratio - initial_aspect_ratio), 0.1)
      << analysis;
  EXPECT_GT(analysis.red, 0.1) << analysis;
  CloseDevTools(window);
}

}  // namespace

#endif  // BUILDFLAG(ENABLE_LIBAOM) && BUILDFLAG(ENABLE_SCREEN_CAPTURE) &&
        // (defined(USE_AURA) || BUILDFLAG(IS_MAC))
