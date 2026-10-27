// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_with_management_policy_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_alert.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr char kExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";
constexpr char kValidChromeURL[] = "chrome://version";

void SetUpTabCaptureCommandLine(base::CommandLine* command_line) {
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

class TabCaptureApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    SetUpTabCaptureCommandLine(command_line);
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

// Tests API behaviors, including info queries, and constraints violations.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, ApiTests) {
  AddExtensionToCommandLineAllowlist();
  ASSERT_TRUE(RunExtensionTest("tab_capture/api_tests",
                               {.extension_url = "api_tests.html"}))
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
      ->GrantIfRequested(extension, base::DoNothing());
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
// TODO(crbug.com/489494749): Port to desktop Android. Currently times out
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
      ->GrantIfRequested(extension, base::DoNothing());
  before_open_tab.Reply("");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that a tab in incognito mode can be captured.
IN_PROC_BROWSER_TEST_F(TabCaptureApiTest, CaptureInSplitIncognitoMode) {
  AddExtensionToCommandLineAllowlist();
  ASSERT_TRUE(RunExtensionTest(
      "tab_capture/start_tab_capture",
      {.extension_url = "start_tab_capture.html", .open_in_incognito = true},
      {.allow_in_incognito = true}))
      << message_;
}

// Exercises the profile boundaries of tabCapture.getMediaStreamId() by
// running the function directly, so the profile of the calling extension
// context can be controlled precisely.
class TabCaptureGetMediaStreamIdTest : public TabCaptureApiTest {
 protected:
  // Loads an extension with the tabCapture permission using the given
  // `incognito_mode` ("split" or "spanning"), enabled in incognito.
  const Extension* LoadTabCaptureExtension(const char* incognito_mode) {
    extension_dir_.WriteManifest(base::StringPrintf(
        R"({
             "name": "Tab capture profile boundaries",
             "version": "1",
             "manifest_version": 3,
             "incognito": "%s",
             "permissions": ["tabCapture"]
           })",
        incognito_mode));
    return LoadExtension(extension_dir_.UnpackedPath(),
                         {.allow_in_incognito = true});
  }

  // Grants `extension` permission to capture the tab hosting `web_contents`,
  // as an extension action invocation on that tab would.
  void GrantCapturePermission(const Extension* extension,
                              content::WebContents* web_contents) {
    ActiveTabPermissionGranter::FromWebContents(web_contents)
        ->GrantIfRequested(extension, base::DoNothing());
  }

  std::string BuildArgs(int target_tab_id, int consumer_tab_id) {
    return base::StringPrintf(R"([{"targetTabId": %d, "consumerTabId": %d}])",
                              target_tab_id, consumer_tab_id);
  }

  scoped_refptr<TabCaptureGetMediaStreamIdFunction> CreateFunction(
      const Extension* extension) {
    auto function = base::MakeRefCounted<TabCaptureGetMediaStreamIdFunction>();
    function->set_extension(extension);
    return function;
  }

 private:
  TestExtensionDir extension_dir_;
};

// The regular-profile instance of a split-mode extension only sees tabs from
// its own profile, so it cannot obtain a stream ID for an incognito tab.
IN_PROC_BROWSER_TEST_F(TabCaptureGetMediaStreamIdTest,
                       SplitModeRejectsIncognitoTargetTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/simple.html");

  const Extension* extension = LoadTabCaptureExtension("split");
  ASSERT_TRUE(extension);

  content::WebContents* consumer_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(consumer_contents, url));

  content::WebContents* incognito_contents =
      PlatformOpenURLOffTheRecord(profile(), url);
  ASSERT_TRUE(incognito_contents);
  GrantCapturePermission(extension, incognito_contents);

  EXPECT_EQ("Invalid tab specified.",
            api_test_utils::RunFunctionAndReturnError(
                CreateFunction(extension),
                BuildArgs(ExtensionTabUtil::GetTabId(incognito_contents),
                          ExtensionTabUtil::GetTabId(consumer_contents)),
                profile()));
}

// The regular-profile instance of a split-mode extension cannot designate a
// tab from the incognito profile as the consumer of a stream ID.
IN_PROC_BROWSER_TEST_F(TabCaptureGetMediaStreamIdTest,
                       SplitModeRejectsIncognitoConsumerTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/simple.html");

  const Extension* extension = LoadTabCaptureExtension("split");
  ASSERT_TRUE(extension);

  content::WebContents* target_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(target_contents, url));
  GrantCapturePermission(extension, target_contents);

  content::WebContents* incognito_contents =
      PlatformOpenURLOffTheRecord(profile(), url);
  ASSERT_TRUE(incognito_contents);

  EXPECT_EQ("Invalid tab specified.",
            api_test_utils::RunFunctionAndReturnError(
                CreateFunction(extension),
                BuildArgs(ExtensionTabUtil::GetTabId(target_contents),
                          ExtensionTabUtil::GetTabId(incognito_contents)),
                profile()));
}

// The incognito-profile instance of a split-mode extension can obtain a
// stream ID for a tab in its own profile.
IN_PROC_BROWSER_TEST_F(TabCaptureGetMediaStreamIdTest,
                       SplitModeIncognitoContextTargetsOwnProfileTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/simple.html");

  const Extension* extension = LoadTabCaptureExtension("split");
  ASSERT_TRUE(extension);

  content::WebContents* incognito_contents =
      PlatformOpenURLOffTheRecord(profile(), url);
  ASSERT_TRUE(incognito_contents);
  GrantCapturePermission(extension, incognito_contents);

  const int incognito_tab_id = ExtensionTabUtil::GetTabId(incognito_contents);
  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          CreateFunction(extension),
          BuildArgs(incognito_tab_id, incognito_tab_id),
          incognito_contents->GetBrowserContext());
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());
  EXPECT_FALSE(result->GetString().empty());
}

// A spanning-mode extension with incognito access can obtain a stream ID for
// an incognito tab.
IN_PROC_BROWSER_TEST_F(TabCaptureGetMediaStreamIdTest,
                       SpanningModeTargetsIncognitoTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/simple.html");

  const Extension* extension = LoadTabCaptureExtension("spanning");
  ASSERT_TRUE(extension);

  content::WebContents* consumer_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(consumer_contents, url));

  content::WebContents* incognito_contents =
      PlatformOpenURLOffTheRecord(profile(), url);
  ASSERT_TRUE(incognito_contents);
  GrantCapturePermission(extension, incognito_contents);

  std::optional<base::Value> result =
      api_test_utils::RunFunctionAndReturnSingleResult(
          CreateFunction(extension),
          BuildArgs(ExtensionTabUtil::GetTabId(incognito_contents),
                    ExtensionTabUtil::GetTabId(consumer_contents)),
          profile(), api_test_utils::FunctionMode::kIncognito);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());
  EXPECT_FALSE(result->GetString().empty());
}

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
    explicit IndicatorChangeObserver(BrowserWindowInterface* browser)
        : browser_(browser) {
      browser_->GetTabStripModel()->AddObserver(this);
    }

    void OnTabChangedAt(tabs::TabInterface* tab,
                        TabChangeType change_type) override {
      std::move(on_tab_changed_).Run();
    }

    void WaitForTabChange() {
      base::RunLoop run_loop;
      on_tab_changed_ = run_loop.QuitClosure();
      run_loop.Run();
    }

   private:
    const raw_ptr<BrowserWindowInterface> browser_;
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
  while (!std::ranges::contains(GetTabAlertStatesForContents(contents),
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
  perm_granter->GrantIfRequested(extension_a, base::DoNothing());

  // Set up success listeners.
  ExtensionTestMessageListener extension_a_success("success");
  extension_a_success.set_extension_id(extension_a->id());
  ExtensionTestMessageListener extension_b_success("success");
  extension_b_success.set_extension_id(extension_b->id());

  // Start a tab capture from extension_a.
  extension_a_ready.Reply("");
  extension_a_ready.Reset();
  ASSERT_TRUE(extension_a_ready.WaitUntilSatisfied());

  perm_granter->GrantIfRequested(extension_b, base::DoNothing());

  // To reproduce crbug.com/40869705, we have extension_b spam tab capture
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

// Tests the interaction between tab capture and hosts that enterprise policy
// restricts extensions from accessing.
class TabCaptureApiPolicyTest : public ExtensionApiTestWithManagementPolicy {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTestWithManagementPolicy::SetUpCommandLine(command_line);
    SetUpTabCaptureCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTestWithManagementPolicy::SetUpOnMainThread();
    test_dir_.WriteManifest(R"({
      "name": "Tab Capture Policy Test",
      "version": "0.1",
      "manifest_version": 3,
      "permissions": ["tabCapture"]
    })");
    test_dir_.WriteFile(FILE_PATH_LITERAL("page.html"), R"(
      <!doctype html>
      <script src="page.js"></script>
    )");

    // A test extension to help drive the tests below. Triggers tab capturing
    // and records associated results and states.
    test_dir_.WriteFile(FILE_PATH_LITERAL("page.js"), R"(
      let captureResult = 'none';
      let captureState = 'none';
      let captureStream = null;

      chrome.tabCapture.onStatusChanged.addListener((info) => {
        captureState = info.status;
      });

      function captureTabAsync(options) {
        // tabCapture.capture() doesn't yet support promises, so we have to do
        // a bit of a dance. See crbug.com/40944873.
        return new Promise((resolve, reject) => {
          chrome.tabCapture.capture(options, (stream) => {
            if (chrome.runtime.lastError) {
              reject(new Error(chrome.runtime.lastError.message));
            } else if (!stream) {
              reject(new Error('No stream returned'));
            } else {
              resolve(stream);
            }
          });
        });
      }

      async function captureActiveTab() {
        captureResult = 'none';
        try {
          const stream = await captureTabAsync({audio: true, video: true});
          captureResult = 'success';
          stream.getVideoTracks()[0].stop();
          stream.getAudioTracks()[0].stop();
        } catch (e) {
          captureResult = e.message;
        }
      }

      async function getMediaStreamId(tabId) {
        captureResult = 'none';
        try {
          await chrome.tabCapture.getMediaStreamId({targetTabId: tabId});
          captureResult = 'success';
        } catch (e) {
          captureResult = e.message;
        }
      }

      async function startCapture() {
        captureState = 'none';
        try {
          captureStream = await captureTabAsync({audio: false, video: true});
          captureStream.getVideoTracks()[0].onended = () => {
            captureState = 'stopped';
          };
          captureState = 'active';
        } catch (e) {
          captureState = 'error: ' + e.message;
        }
      }
    )");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());
    ASSERT_TRUE(extension);
    extension_id_ = extension->id();

    content::RenderFrameHost* const main_frame =
        NavigateToURLInNewTab(extension->GetResourceURL("page.html"));
    ASSERT_TRUE(main_frame);
    extension_contents_ = content::WebContents::FromRenderFrameHost(main_frame);
    ASSERT_TRUE(extension_contents_);
  }

  void TearDownOnMainThread() override {
    extension_contents_ = nullptr;
    ExtensionApiTestWithManagementPolicy::TearDownOnMainThread();
  }

  // Helper methods to activate certain methods in the test extension.
  void CaptureTab() {
    EXPECT_TRUE(content::ExecJs(extension_contents_, "captureActiveTab();"));
  }
  void GetMediaStreamIdForTab(int tab_id) {
    EXPECT_TRUE(
        content::ExecJs(extension_contents_,
                        base::StringPrintf("getMediaStreamId(%d);", tab_id)));
  }
  void StartCapture() {
    EXPECT_TRUE(content::ExecJs(extension_contents_, "startCapture();"));
  }
  std::string GetCaptureResult() {
    return content::EvalJs(extension_contents_, "captureResult")
        .ExtractString();
  }
  std::string GetCaptureState() {
    return content::EvalJs(extension_contents_, "captureState").ExtractString();
  }

  // Opens a new tab at `host` and grants it active tab.
  content::WebContents* OpenNewTabAndGrantActiveTab(const std::string& host) {
    content::RenderFrameHost* const main_frame = NavigateToURLInNewTab(
        embedded_test_server()->GetURL(host, "/simple.html"));
    EXPECT_TRUE(main_frame);
    content::WebContents* const web_contents =
        content::WebContents::FromRenderFrameHost(main_frame);
    ActiveTabPermissionGranter::FromWebContents(web_contents)
        ->GrantIfRequested(extension(), base::DoNothing());
    return web_contents;
  }

  const Extension* extension() {
    return ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
        extension_id_);
  }

 protected:
  TestExtensionDir test_dir_;
  ExtensionId extension_id_;
  raw_ptr<content::WebContents> extension_contents_ = nullptr;
};

// Tests that a tab showing a host blocked by enterprise policy cannot be
// captured, even if the extension has been invoked on that tab.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest, PolicyBlockedHostCapture) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  OpenNewTabAndGrantActiveTab("a.test");
  CaptureTab();
  EXPECT_EQ(tab_capture_errors::kCannotCapturePage, GetCaptureResult());
}

// Tests that a media stream id cannot be created for a tab showing a host
// blocked by enterprise policy, even if the extension has been invoked on
// that tab.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest,
                       PolicyBlockedHostGetMediaStreamId) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  content::WebContents* const web_contents =
      OpenNewTabAndGrantActiveTab("a.test");

  GetMediaStreamIdForTab(ExtensionTabUtil::GetTabId(web_contents));
  EXPECT_EQ(tab_capture_errors::kCannotCapturePage, GetCaptureResult());
}

// Tests that a policy blocking one host does not prevent capturing tabs
// showing other hosts.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest, PolicyBlockedOtherHostCapture) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  OpenNewTabAndGrantActiveTab("b.test");

  CaptureTab();
  EXPECT_EQ("success", GetCaptureResult());
}

// Tests that a policy blocking one host does not prevent creating media
// stream ids for tabs showing other hosts.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest,
                       PolicyBlockedOtherHostGetMediaStreamId) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  content::WebContents* const web_contents =
      OpenNewTabAndGrantActiveTab("b.test");

  GetMediaStreamIdForTab(ExtensionTabUtil::GetTabId(web_contents));
  EXPECT_EQ("success", GetCaptureResult());
}

// Tests that a tab showing a file:// URL cannot be captured if the extension
// does not have file access enabled.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest, FileUrlWithoutFileAccess) {
  base::FilePath test_file =
      test_data_dir_.AppendASCII("tab_capture").AppendASCII("balls.html");
  GURL file_url = net::FilePathToFileURL(test_file);

  content::RenderFrameHost* const main_frame = NavigateToURLInNewTab(file_url);
  ASSERT_TRUE(main_frame);
  content::WebContents* const web_contents =
      content::WebContents::FromRenderFrameHost(main_frame);
  ActiveTabPermissionGranter::FromWebContents(web_contents)
      ->GrantIfRequested(extension(), base::DoNothing());

  CaptureTab();
  EXPECT_EQ(tab_capture_errors::kCannotCapturePage, GetCaptureResult());
}

// Tests that an opaque URL (about:blank) whose precursor origin is blocked by
// enterprise policy cannot be captured.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest,
                       OpaqueOriginWithBlockedPrecursor) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  // Navigate a tab to the blocked host a.test.
  content::RenderFrameHost* const blocked_frame = NavigateToURLInNewTab(
      embedded_test_server()->GetURL("a.test", "/simple.html"));
  ASSERT_TRUE(blocked_frame);
  content::WebContents* const opener_contents =
      content::WebContents::FromRenderFrameHost(blocked_frame);

  // Open an about:blank tab with a.test as opener/precursor.
  content::WebContentsAddedObserver tab_observer;
  ASSERT_TRUE(content::ExecJs(opener_contents,
                              "window.open('about:blank', '_blank');"));
  content::WebContents* const popup_contents = tab_observer.GetWebContents();
  ASSERT_TRUE(popup_contents);
  EXPECT_TRUE(content::WaitForLoadStop(popup_contents));

  ActiveTabPermissionGranter::FromWebContents(popup_contents)
      ->GrantIfRequested(extension(), base::DoNothing());

  CaptureTab();
  EXPECT_EQ(tab_capture_errors::kCannotCapturePage, GetCaptureResult());
}

// Tests that navigating a captured tab to a policy-blocked host aborts the
// active capture session.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest,
                       PolicyBlockedHostNavigationAbortsCapture) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  {
    ExtensionManagementPolicyUpdater pref(&policy_provider_);
    pref.AddPolicyBlockedHost("*", "*://a.test");
  }

  // Start on an allowed host b.test.
  content::WebContents* const web_contents =
      OpenNewTabAndGrantActiveTab("b.test");

  StartCapture();
  EXPECT_EQ("active", GetCaptureState());

  // Navigate the captured tab to the policy-blocked host a.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/simple.html")));

  EXPECT_EQ("stopped", GetCaptureState());
}

// Tests that navigating a captured tab to a chrome:// URL aborts the active
// capture session, while normal cross-origin web navigations maintain capture.
IN_PROC_BROWSER_TEST_F(TabCaptureApiPolicyTest,
                       ChromeUrlNavigationAbortsCapture) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // 1. Direct navigation from a tab with an activeTab grant (b.test) to a
  // chrome:// URL should abort capture.
  content::WebContents* const web_contents1 =
      OpenNewTabAndGrantActiveTab("b.test");

  StartCapture();
  EXPECT_EQ("active", GetCaptureState());

  ASSERT_TRUE(content::NavigateToURL(web_contents1, GURL(kValidChromeURL)));
  EXPECT_EQ("stopped", GetCaptureState());

  // 2. Navigating cross-origin to another web host (a.test) should maintain
  // capture, and subsequently navigating to a chrome:// URL should abort it.
  content::WebContents* const web_contents2 =
      OpenNewTabAndGrantActiveTab("b.test");

  StartCapture();
  EXPECT_EQ("active", GetCaptureState());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents2, embedded_test_server()->GetURL("a.test", "/simple.html")));
  EXPECT_EQ("active", GetCaptureState());

  ASSERT_TRUE(content::NavigateToURL(web_contents2, GURL(kValidChromeURL)));
  EXPECT_EQ("stopped", GetCaptureState());
}

// Tests the interaction between tab capture and hosts on which the user has
// blocked all extensions.
class TabCaptureApiUserHostRestrictionsTest : public TabCaptureApiPolicyTest {
 public:
  TabCaptureApiUserHostRestrictionsTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionsMenuAccessControl);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that a tab showing a host on which the user has blocked all
// extensions cannot be captured.
IN_PROC_BROWSER_TEST_F(TabCaptureApiUserHostRestrictionsTest,
                       UserBlockedHostCapture) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const GURL page_url =
      embedded_test_server()->GetURL("a.test", "/simple.html");
  PermissionsManager::Get(profile())->AddUserRestrictedSite(
      url::Origin::Create(page_url));

  OpenNewTabAndGrantActiveTab("a.test");

  CaptureTab();
  EXPECT_EQ(tab_capture_errors::kCannotCapturePage, GetCaptureResult());
}

// Tests that a media stream id cannot be created for a tab showing a host on
// which the user has blocked all extensions.
IN_PROC_BROWSER_TEST_F(TabCaptureApiUserHostRestrictionsTest,
                       UserBlockedHostGetMediaStreamId) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const GURL page_url =
      embedded_test_server()->GetURL("a.test", "/simple.html");
  PermissionsManager::Get(profile())->AddUserRestrictedSite(
      url::Origin::Create(page_url));

  content::WebContents* const web_contents =
      OpenNewTabAndGrantActiveTab("a.test");

  GetMediaStreamIdForTab(ExtensionTabUtil::GetTabId(web_contents));
  EXPECT_EQ(tab_capture_errors::kCannotCapturePage, GetCaptureResult());
}

}  // namespace

}  // namespace extensions
