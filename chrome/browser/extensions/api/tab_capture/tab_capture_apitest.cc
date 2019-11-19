// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

const char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

class TabCaptureApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Specify smallish window size to make testing of tab capture less CPU
    // intensive.
    command_line->AppendSwitchASCII(::switches::kWindowSize, "300,300");
  }

  void AddExtensionToCommandLineWhitelist() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, kExtensionId);
  }

 protected:
  void SimulateMouseClickInCurrentTab() {
    content::SimulateMouseClick(
        browser()->tab_strip_model()->GetActiveWebContents(), 0,
        blink::WebMouseEvent::Button::kLeft);
  }
};

class TabCaptureApiPixelTest : public TabCaptureApiTest {
 public:
  void SetUp() override {
    // TODO(crbug/754872): Update this to match WCVCD content_browsertests.
    if (!IsTooIntensiveForThisPlatform())
      EnablePixelOutput();
    TabCaptureApiTest::SetUp();
  }

 protected:
  bool IsTooIntensiveForThisPlatform() const {
#if defined(NDEBUG)
    // The tests are too slow to succeed with software GL on the bots.
    return UsingSoftwareGL();
#else
    // The tests only run on release builds.
    return true;
#endif
  }
};

// Tests the logic that examines the constraints to determine the starting
// off-screen tab size.
TEST(TabCaptureCaptureOffscreenTabTest, DetermineInitialSize) {
  using extensions::api::tab_capture::CaptureOptions;
  using extensions::api::tab_capture::MediaStreamConstraint;

  // Empty options --> 1280x720
  CaptureOptions options;
  EXPECT_EQ(gfx::Size(1280, 720),
            TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
                options));

  // Use specified mandatory maximum size.
  options.video_constraints.reset(new MediaStreamConstraint());
  base::DictionaryValue* properties =
      &options.video_constraints->mandatory.additional_properties;
  properties->SetInteger("maxWidth", 123);
  properties->SetInteger("maxHeight", 456);
  EXPECT_EQ(gfx::Size(123, 456),
            TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
                options));

  // Use default size if larger than mandatory minimum size.  Else, use
  // mandatory minimum size.
  options.video_constraints.reset(new MediaStreamConstraint());
  properties = &options.video_constraints->mandatory.additional_properties;
  properties->SetInteger("minWidth", 123);
  properties->SetInteger("minHeight", 456);
  EXPECT_EQ(gfx::Size(1280, 720),
            TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
                options));
  properties->SetInteger("minWidth", 2560);
  properties->SetInteger("minHeight", 1440);
  EXPECT_EQ(gfx::Size(2560, 1440),
            TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
                options));

  // Use specified optional maximum size, if no mandatory size was specified.
  options.video_constraints.reset(new MediaStreamConstraint());
  options.video_constraints->optional.reset(
      new MediaStreamConstraint::Optional());
  properties = &options.video_constraints->optional->additional_properties;
  properties->SetInteger("maxWidth", 456);
  properties->SetInteger("maxHeight", 123);
  EXPECT_EQ(gfx::Size(456, 123),
            TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
                options));
  // ...unless a mandatory minimum size was specified:
  options.video_constraints->mandatory.additional_properties.SetInteger(
      "minWidth", 500);
  options.video_constraints->mandatory.additional_properties.SetInteger(
      "minHeight", 600);
  EXPECT_EQ(gfx::Size(500, 600),
            TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
                options));

  // Use default size if larger than optional minimum size.  Else, use optional
  // minimum size.
  options.video_constraints.reset(new MediaStreamConstraint());
  options.video_constraints->optional.reset(
      new MediaStreamConstraint::Optional());
  properties = &options.video_constraints->optional->additional_properties;
  properties->SetInteger("minWidth", 9999);
  properties->SetInteger("minHeight", 8888);
  EXPECT_EQ(gfx::Size(9999, 8888),
            TabCaptureCaptureOffscreenTabFunction::DetermineInitialSize(
                options));
}

// Flaky on Mac. See https://crbug.com/764464.
#if defined(OS_MACOSX) || (defined(OS_LINUX) && defined(MEMORY_SANITIZER))
#define MAYBE_ApiTests DISABLED_ApiTests
#else
#define MAYBE_ApiTests ApiTests
#endif
// Tests API behaviors, including info queries, and constraints violations.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_ApiTests) {
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "api_tests.html")) << message_;
}

#if (defined(OS_MACOSX) && defined(ADDRESS_SANITIZER)) || defined(OS_LINUX) || \
    defined(OS_WIN)
// Flaky on ASAN on Mac, and on Linux and Windows. See https://crbug.com/674497.
#define MAYBE_MaxOffscreenTabs DISABLED_MaxOffscreenTabs
#else
#define MAYBE_MaxOffscreenTabs MaxOffscreenTabs
#endif
// Tests that there is a maximum limitation to the number of simultaneous
// off-screen tabs.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_MaxOffscreenTabs) {
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "max_offscreen_tabs.html"))
      << message_;
}

// Tests that tab capture video frames can be received in a VIDEO element.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPixelTest, EndToEndWithoutRemoting) {
  if (IsTooIntensiveForThisPlatform()) {
    LOG(WARNING) << "Skipping this CPU-intensive test on this platform/build.";
    return;
  }
  AddExtensionToCommandLineWhitelist();
  // Note: The range of acceptable colors is quite large because there's no way
  // to know whether software compositing is being used for screen capture; and,
  // if software compositing is being used, there is no color space management
  // and color values can be off by a lot. That said, color accuracy is being
  // tested by a suite of content_browsertests.
  ASSERT_TRUE(RunExtensionSubtest(
      "tab_capture", "end_to_end.html?method=local&colorDeviation=50"))
      << message_;
}

// Tests that video frames are captured, transported via WebRTC, and finally
// received in a VIDEO element.  More allowance is provided for color deviation
// because of the additional layers of video processing performed within
// WebRTC.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPixelTest, EndToEndThroughWebRTC) {
  if (IsTooIntensiveForThisPlatform()) {
    LOG(WARNING) << "Skipping this CPU-intensive test on this platform/build.";
    return;
  }
  AddExtensionToCommandLineWhitelist();
  // See note in EndToEndWithoutRemoting test about why |colorDeviation| is
  // being set so high.
  ASSERT_TRUE(RunExtensionSubtest(
      "tab_capture", "end_to_end.html?method=webrtc&colorDeviation=50"))
      << message_;
}

// Tests that tab capture video frames can be received in a VIDEO element from
// an off-screen tab.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPixelTest, OffscreenTabEndToEnd) {
  if (IsTooIntensiveForThisPlatform()) {
    LOG(WARNING) << "Skipping this CPU-intensive test on this platform/build.";
    return;
  }
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "offscreen_end_to_end.html"))
      << message_;
  // Verify that offscreen profile has been destroyed.
  ASSERT_FALSE(profile()->HasOffTheRecordProfile());
}

#if defined(OS_MACOSX)
// Timeout on Mac. crbug.com/864250
#define MAYBE_OffscreenTabEvilTests DISABLED_OffscreenTabEvilTests
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
// Flaky on Linux and ChromeOS. crbug.com/895120
#define MAYBE_OffscreenTabEvilTests DISABLED_OffscreenTabEvilTests
#else
#define MAYBE_OffscreenTabEvilTests OffscreenTabEvilTests
#endif

// Tests that off-screen tabs can't do evil things (e.g., access local files).
IN_PROC_BROWSER_TEST_F(TabCaptureApiPixelTest, MAYBE_OffscreenTabEvilTests) {
  if (IsTooIntensiveForThisPlatform()) {
    LOG(WARNING) << "Skipping this CPU-intensive test on this platform/build.";
    return;
  }
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "offscreen_evil_tests.html"))
      << message_;
  // Verify that offscreen profile has been destroyed.
  ASSERT_FALSE(profile()->HasOffTheRecordProfile());
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_GetUserMediaTest DISABLED_GetUserMediaTest
#else
#define MAYBE_GetUserMediaTest GetUserMediaTest
#endif
// Tests that getUserMedia() is NOT a way to start tab capture.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_GetUserMediaTest) {
  ExtensionTestMessageListener listener("ready", true);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "get_user_media_test.html"))
      << message_;

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::OpenURLParams params(GURL("about:blank"), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);

  content::RenderFrameHost* const main_frame = web_contents->GetMainFrame();
  ASSERT_TRUE(main_frame);
  listener.Reply(base::StringPrintf("web-contents-media-stream://%i:%i",
                                    main_frame->GetProcess()->GetID(),
                                    main_frame->GetRoutingID()));

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163, http://crbug.com/427730
// Make sure tabCapture.capture only works if the tab has been granted
// permission via an extension icon click or the extension is whitelisted.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, DISABLED_ActiveTabPermission) {
  ExtensionTestMessageListener before_open_tab("ready1", true);
  ExtensionTestMessageListener before_grant_permission("ready2", true);
  ExtensionTestMessageListener before_open_new_tab("ready3", true);
  ExtensionTestMessageListener before_whitelist_extension("ready4", true);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture",
                                  "active_tab_permission_test.html"))
      << message_;

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());
  content::OpenURLParams params(GURL("http://google.com"), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  before_open_tab.Reply("");

  // Grant permission and make sure capture succeeds.
  EXPECT_TRUE(before_grant_permission.WaitUntilSatisfied());
  const Extension* extension = ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          kExtensionId);
  TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()->GrantIfRequested(extension);
  before_grant_permission.Reply("");

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_new_tab.WaitUntilSatisfied());
  browser()->OpenURL(params);
  before_open_new_tab.Reply("");

  // Add extension to whitelist and make sure capture succeeds.
  EXPECT_TRUE(before_whitelist_extension.WaitUntilSatisfied());
  AddExtensionToCommandLineWhitelist();
  before_whitelist_extension.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Flaky: http://crbug.com/675851
// Tests that fullscreen transitions during a tab capture session dispatch
// events to the onStatusChange listener.  The test loads a page that toggles
// fullscreen mode, using the Fullscreen Javascript API, in response to mouse
// clicks.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, DISABLED_FullscreenEvents) {
  AddExtensionToCommandLineWhitelist();

  ExtensionTestMessageListener capture_started("tab_capture_started", false);
  ExtensionTestMessageListener entered_fullscreen("entered_fullscreen", false);

  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "fullscreen_test.html"))
      << message_;
  EXPECT_TRUE(capture_started.WaitUntilSatisfied());

  // Click on the page to trigger the Javascript that will toggle the tab into
  // fullscreen mode.
  SimulateMouseClickInCurrentTab();
  EXPECT_TRUE(entered_fullscreen.WaitUntilSatisfied());

  // Click again to exit fullscreen mode.
  SimulateMouseClickInCurrentTab();

  // Wait until the page examines its results and calls chrome.test.succeed().
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Times out on Win dbg bots: https://crbug.com/177163
// Flaky on MSan bots: https://crbug.com/294431
// But really, just flaky everywhere. http://crbug.com/294431#c33
// Make sure tabCapture API can be granted for Chrome:// pages.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, DISABLED_GrantForChromePages) {
  ExtensionTestMessageListener before_open_tab("ready1", true);
  ASSERT_TRUE(RunExtensionSubtest("tab_capture",
                                  "active_tab_chrome_pages.html"))
      << message_;
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());

  // Open a tab on a chrome:// page and make sure we can capture.
  content::OpenURLParams params(GURL("chrome://version"), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* web_contents = browser()->OpenURL(params);
  const Extension* extension = ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          kExtensionId);
  TabHelper::FromWebContents(web_contents)
      ->active_tab_permission_granter()->GrantIfRequested(extension);
  before_open_tab.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_CaptureInSplitIncognitoMode DISABLED_CaptureInSplitIncognitoMode
#else
#define MAYBE_CaptureInSplitIncognitoMode CaptureInSplitIncognitoMode
#endif
// Tests that a tab in incognito mode can be captured.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_CaptureInSplitIncognitoMode) {
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture",
                                  "start_tab_capture.html",
                                  kFlagEnableIncognito | kFlagUseIncognito))
      << message_;
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_Constraints DISABLED_Constraints
#else
#define MAYBE_Constraints Constraints
#endif
// Tests that valid constraints allow tab capture to start, while invalid ones
// do not.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_Constraints) {
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "constraints.html"))
      << message_;
}

// http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TabIndicator DISABLED_TabIndicator
#else
#define MAYBE_TabIndicator TabIndicator
#endif
// Tests that the tab indicator (in the tab strip) is shown during tab capture.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MAYBE_TabIndicator) {
  content::WebContents* const contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_THAT(chrome::GetTabAlertStatesForContents(contents),
              ::testing::IsEmpty());

  // A TabStripModelObserver that quits the MessageLoop whenever the
  // UI's model is sent an event that might change the indicator status.
  class IndicatorChangeObserver : public TabStripModelObserver {
   public:
    explicit IndicatorChangeObserver(Browser* browser) : browser_(browser) {
      browser_->tab_strip_model()->AddObserver(this);
    }

    void TabChangedAt(content::WebContents* contents,
                      int index,
                      TabChangeType change_type) override {
      std::move(on_tab_changed_).Run();
    }

    void WaitForTabChange() {
      base::RunLoop run_loop;
      on_tab_changed_ = run_loop.QuitClosure();
      run_loop.Run();
    }

   private:
    Browser* const browser_;
    base::OnceClosure on_tab_changed_;
  };

  ASSERT_THAT(chrome::GetTabAlertStatesForContents(contents),
              ::testing::IsEmpty());

  // Run an extension test that just turns on tab capture, which should cause
  // the indicator to turn on.
  AddExtensionToCommandLineWhitelist();
  ASSERT_TRUE(RunExtensionSubtest("tab_capture", "start_tab_capture.html"))
      << message_;

  // Run the browser until the indicator turns on.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  IndicatorChangeObserver observer(browser());
  while (!base::Contains(chrome::GetTabAlertStatesForContents(contents),
                         TabAlertState::TAB_CAPTURING)) {
    if (base::TimeTicks::Now() - start_time >
            TestTimeouts::action_max_timeout()) {
      EXPECT_THAT(chrome::GetTabAlertStatesForContents(contents),
                  ::testing::Contains(TabAlertState::TAB_CAPTURING));
      return;
    }
    observer.WaitForTabChange();
  }
}

}  // namespace

}  // namespace extensions
