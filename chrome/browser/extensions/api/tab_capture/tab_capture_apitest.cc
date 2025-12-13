// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/gl_switches.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif

namespace extensions {

namespace {

constexpr char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";
constexpr char kValidChromeURL[] = "chrome://version";

class TabCaptureApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Specify smallish window size to make testing of tab capture less CPU
    // intensive.
    command_line->AppendSwitchASCII(::switches::kWindowSize, "300,300");
    // MSan and GL do not get along so avoid using the GPU with MSan.
    // TODO(crbug.com/40260482): Remove this after fixing feature
    // detection in 0c tab capture path as it'll no longer be needed.
#if !BUILDFLAG(IS_CHROMEOS) && !defined(MEMORY_SANITIZER)
    command_line->AppendSwitch(::switches::kUseGpuInTests);
#endif
  }

  void AddExtensionToCommandLineAllowlist() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAllowlistedExtensionID, kExtensionId);
  }

 protected:
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::vector<tabs::TabAlert> GetTabAlertStatesForContents(
      content::WebContents* web_contents) {
    return tabs::TabAlertController::From(
               tabs::TabInterface::GetFromContents(web_contents))
        ->GetAllActiveAlerts();
  }

  void SimulateMouseClickInCurrentTab() {
    content::SimulateMouseClick(GetActiveWebContents(), 0,
                                blink::WebMouseEvent::Button::kLeft);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
};

// A very simple test to verify chrome.tabCapture.capture() returns a media
// stream. Does not rely on chrome.tabs, so can be run on all platforms.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, StartTabCapture) {
  AddExtensionToCommandLineAllowlist();
  ASSERT_TRUE(RunExtensionTest("tab_capture/start_tab_capture",
                               {.extension_url = "start_tab_capture.html"}))
      << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests API behaviors, including info queries, and constraints violations.
// TODO(crbug.com/427298135): Port to desktop Android when chrome.tabs is more
// fully supported.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ApiTests) {
  AddExtensionToCommandLineAllowlist();
  ASSERT_TRUE(RunExtensionTest("tab_capture/api_tests",
                               {.extension_url = "api_tests.html"}))
      << message_;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Tests that tab capture video frames can be received in a VIDEO element.
// TODO(crbug.com/216820236): This test is flaky.
// Possible culprit:
// The script uses a complicated animation loop to create frames, when simpler
// CSS animations would be fine.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, DISABLED_EndToEndWithoutRemoting) {
  AddExtensionToCommandLineAllowlist();
  // Note: The range of acceptable colors is quite large because there's no way
  // to know whether software compositing is being used for screen capture; and,
  // if software compositing is being used, there is no color space management
  // and color values can be off by a lot. That said, color accuracy is being
  // tested by a suite of content_browsertests.
  ASSERT_TRUE(RunExtensionTest(
      "tab_capture/end_to_end",
      {.extension_url = "end_to_end.html?method=local&colorDeviation=50"}))
      << message_;
}

// Tests that getUserMedia() is NOT a way to start tab capture.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, GetUserMediaTest) {
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  ASSERT_TRUE(RunExtensionTest("tab_capture/get_user_media_test",
                               {.extension_url = "get_user_media_test.html"}))
      << message_;

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  content::RenderFrameHost* const main_frame =
      NavigateToURLInNewTab(GURL(url::kAboutBlankURL));
  ASSERT_TRUE(main_frame);
  listener.Reply(base::StringPrintf("web-contents-media-stream://%i:%i",
                                    main_frame->GetProcess()->GetDeprecatedID(),
                                    main_frame->GetRoutingID()));

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Make sure tabCapture.capture only works if the tab has been granted
// permission via an extension icon click or the extension is allowlisted.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ActiveTabPermission) {
  ExtensionTestMessageListener before_open_tab("ready1",
                                               ReplyBehavior::kWillReply);
  ExtensionTestMessageListener before_grant_permission(
      "ready2", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener before_open_new_tab("ready3",
                                                   ReplyBehavior::kWillReply);
  ExtensionTestMessageListener before_allowlist_extension(
      "ready4", ReplyBehavior::kWillReply);

  ASSERT_TRUE(
      RunExtensionTest("tab_capture/active_tab_permission_test",
                       {.extension_url = "active_tab_permission_test.html"}))
      << message_;

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());
  content::RenderFrameHost* const main_frame =
      NavigateToURLInNewTab(GURL(url::kAboutBlankURL));
  ASSERT_TRUE(main_frame);
  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(main_frame);
  ASSERT_TRUE(web_contents) << "Failed to open new tab";
  before_open_tab.Reply("");

  // Grant permission and make sure capture succeeds.
  EXPECT_TRUE(before_grant_permission.WaitUntilSatisfied());
  const Extension* extension = ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          kExtensionId);
  ActiveTabPermissionGranter::FromWebContents(web_contents)
      ->GrantIfRequested(extension);
  before_grant_permission.Reply("");

  // Open a new tab and make sure capture is denied.
  EXPECT_TRUE(before_open_new_tab.WaitUntilSatisfied());
  NavigateToURLInNewTab(GURL(url::kAboutBlankURL));
  before_open_new_tab.Reply("");

  // Add extension to allowlist and make sure capture succeeds.
  EXPECT_TRUE(before_allowlist_extension.WaitUntilSatisfied());
  AddExtensionToCommandLineAllowlist();
  before_allowlist_extension.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests that fullscreen transitions during a tab capture session dispatch
// events to the onStatusChange listener.  The test loads a page that toggles
// fullscreen mode, using the Fullscreen Javascript API, in response to mouse
// clicks. The fullscreen API requires a user gesture.
// TODO(crbug.com/427298135): Port to desktop Android. Currently times out
// without useful stack or logs.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, FullscreenEvents) {
  AddExtensionToCommandLineAllowlist();

  ExtensionTestMessageListener capture_started("tab_capture_started");
  ExtensionTestMessageListener entered_fullscreen("entered_fullscreen");

  ASSERT_TRUE(RunExtensionTest("tab_capture/fullscreen_test",
                               {.extension_url = "fullscreen_test.html"}))
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
  catcher.RestrictToBrowserContext(profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, GrantForChromePages) {
  ExtensionTestMessageListener before_open_tab("ready1",
                                               ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      RunExtensionTest("tab_capture/active_tab_chrome_pages",
                       {.extension_url = "active_tab_chrome_pages.html"}))
      << message_;
  EXPECT_TRUE(before_open_tab.WaitUntilSatisfied());

  // Open a tab on a chrome:// page and make sure we can capture.
  NavigateToURLInNewTab(GURL(kValidChromeURL));
  content::WebContents* web_contents = GetActiveWebContents();
  const Extension* extension = ExtensionRegistry::Get(
      web_contents->GetBrowserContext())->enabled_extensions().GetByID(
          kExtensionId);
  ActiveTabPermissionGranter::FromWebContents(web_contents)
      ->GrantIfRequested(extension);
  before_open_tab.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests that a tab in incognito mode can be captured.
// TODO(crbug.com/427298135): Port to desktop Android when incognito is better
// supported in extensions tests.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, CaptureInSplitIncognitoMode) {
  AddExtensionToCommandLineAllowlist();
  ASSERT_TRUE(RunExtensionTest(
      "tab_capture/start_tab_capture",
      {.extension_url = "start_tab_capture.html", .open_in_incognito = true},
      {.allow_in_incognito = true}))
      << message_;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, Constraints) {
  AddExtensionToCommandLineAllowlist();
  ASSERT_TRUE(RunExtensionTest("tab_capture/constraints",
                               {.extension_url = "constraints.html"}))
      << message_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests that the tab indicator (in the tab strip) is shown during tab capture.
// TODO(crbug.com/427298135): Port this to BrowserWindowInterface after
// TabListInterfaceObserver supports TabChangedAt().
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, TabIndicator) {
  content::WebContents* const contents = GetActiveWebContents();
  ASSERT_THAT(GetTabAlertStatesForContents(contents), ::testing::IsEmpty());

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
    const raw_ptr<Browser> browser_;
    base::OnceClosure on_tab_changed_;
  };

  ASSERT_THAT(GetTabAlertStatesForContents(contents), ::testing::IsEmpty());

  // Run an extension test that just turns on tab capture, which should cause
  // the indicator to turn on.
  AddExtensionToCommandLineAllowlist();
  ASSERT_TRUE(RunExtensionTest("tab_capture/start_tab_capture",
                               {.extension_url = "start_tab_capture.html"}))
      << message_;

  // Run the browser until the indicator turns on.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  IndicatorChangeObserver observer(browser());
  while (!base::Contains(GetTabAlertStatesForContents(contents),
                         tabs::TabAlert::kTabCapturing)) {
    if (base::TimeTicks::Now() - start_time >
            TestTimeouts::action_max_timeout()) {
      EXPECT_THAT(GetTabAlertStatesForContents(contents),
                  ::testing::Contains(tabs::TabAlert::kTabCapturing));
      return;
    }
    observer.WaitForTabChange();
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, MultipleExtensions) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load both extensions and wait for them to be ready.
  base::FilePath base_path = test_data_dir_.AppendASCII("tab_capture")
                                 .AppendASCII("multiple_extensions");

  ExtensionTestMessageListener extension_a_ready("ready",
                                                 ReplyBehavior::kWillReply);
  auto* extension_a = LoadExtension(base_path.AppendASCII("a"));
  ASSERT_TRUE(extension_a);
  extension_a_ready.set_extension_id(extension_a->id());
  ASSERT_TRUE(extension_a_ready.WaitUntilSatisfied());

  ExtensionTestMessageListener extension_b_ready("ready",
                                                 ReplyBehavior::kWillReply);
  auto* extension_b = LoadExtension(base_path.AppendASCII("b"));
  ASSERT_TRUE(extension_b);
  extension_b_ready.set_extension_id(extension_b->id());
  ASSERT_TRUE(extension_b_ready.WaitUntilSatisfied());

  // Open a page and grant permissions.
  content::RenderFrameHost* const main_frame =
      NavigateToURLInNewTab(embedded_test_server()->GetURL("/simple.html"));
  ASSERT_TRUE(main_frame);
  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(main_frame);
  ASSERT_TRUE(web_contents) << "Failed to open new tab";
  auto* perm_granter =
      ActiveTabPermissionGranter::FromWebContents(web_contents);
  // It doesn't seem to work to grant permissions for both extensions at the
  // same time. We start with extension_a.
  perm_granter->GrantIfRequested(extension_a);

  // Set up success listeners.
  ExtensionTestMessageListener extension_a_success("success");
  extension_a_success.set_extension_id(extension_a->id());
  ExtensionTestMessageListener extension_b_success("success");
  extension_b_success.set_extension_id(extension_b->id());

  // Start a tab capture from extension_a.
  extension_a_ready.Reply("");
  extension_a_ready.Reset();
  ASSERT_TRUE(extension_a_ready.WaitUntilSatisfied());

  perm_granter->GrantIfRequested(extension_b);

  // To reproduce crbug.com/1370338, we have extension_b spam tab capture
  // requests until one or the other extension successfully captures the tab.
  while (!extension_a_success.was_satisfied() &&
         !extension_b_success.was_satisfied()) {
    extension_b_ready.Reply("");
    extension_b_ready.Reset();
    ASSERT_TRUE(extension_b_ready.WaitUntilSatisfied());
  }
  // Only one capture should succeed.
  // TODO(crbug.com/40874553): Remove this restriction.
  ASSERT_TRUE(extension_a_success.was_satisfied() !=
              extension_b_success.was_satisfied());
  // Avoid CHECK for forgotten reply in ExtensionTestMessageListener destructor.
  extension_a_ready.Reply("");
  extension_b_ready.Reply("");
}

}  // namespace

}  // namespace extensions
