// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/feedback_private.h"

using extensions::api::feedback_private::FeedbackFlow;

namespace extensions {

// TODO(http://b/230376167): Remove or migrate the tests to work with WebUI
// Feedback.
class FeedbackTest : public ExtensionBrowserTest {
 public:
  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnableUserMediaScreenCapturing);
  }

 protected:
  bool IsFeedbackAppAvailable() { return false; }

  void StartFeedbackUI(FeedbackFlow flow,
                       const std::string& extra_diagnostics,
                       bool from_assistant = false,
                       bool include_bluetooth_logs = false,
                       bool show_questionnaire = false) {}

  void VerifyFeedbackAppLaunch() {}
};

class TestFeedbackUploaderDelegate
    : public feedback::FeedbackUploaderChrome::Delegate {
 public:
  explicit TestFeedbackUploaderDelegate(base::OnceClosure quit_callback)
      : quit_callback_(std::move(quit_callback)) {}

  void OnStartDispatchingReport() override { std::move(quit_callback_).Run(); }

 private:
  base::OnceClosure quit_callback_;
};

// TODO(crbug.com/40194697): disable tests.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_ShowFeedback) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kRegular, std::string());
  VerifyFeedbackAppLaunch();
}

// TODO(crbug.com/40194697): disable tests.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_ShowLoginFeedback) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kLogin, std::string());
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "$('page-url').hidden && $('attach-file-container').hidden && "
                "$('attach-file-note').hidden;"));
}

// Tests that there's an option in the email drop down box with a value ''.
// TODO(crbug.com/40194697): disable tests.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_AnonymousUser) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kRegular, std::string());
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  EXPECT_EQ(true, content::EvalJs(
                      content,
                      "((function() {"
                      "    var options = $('user-email-drop-down').options;"
                      "    for (var option in options) {"
                      "      if (options[option].value == '')"
                      "        return true;"
                      "    }"
                      "    return false;"
                      "  })());"));
}

// Ensures that when extra diagnostics are provided with feedback, they are
// injected properly in the system information.
// TODO(crbug.com/40194697): disable tests.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_ExtraDiagnostics) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kRegular, "Some diagnostics");
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  EXPECT_EQ(true, content::EvalJs(
                      content,
                      "((function() {"
                      "    var sysInfo = feedbackInfo.systemInformation;"
                      "    for (var info in sysInfo) {"
                      "      if (sysInfo[info].key == 'EXTRA_DIAGNOSTICS' &&"
                      "          sysInfo[info].value == 'Some diagnostics') {"
                      "        return true;"
                      "      }"
                      "    }"
                      "    return false;"
                      "  })());"));
}

// Ensures that when triggered from Assistant with Google account, Assistant
// checkbox are not hidden.
// Disabled due to flake: https://crbug.com/1240591
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_ShowFeedbackFromAssistant) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kGoogleInternal, std::string(),
                  /*from_assistant*/ true);
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "((function() {"
                "    if ($('assistant-checkbox-container') != null &&"
                "        $('assistant-checkbox-container').hidden == true) {"
                "      return false;"
                "    }"
                "    return true;"
                "  })());"));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Ensures that when triggered from a Google account and a Bluetooth related
// string is entered into the description, that we provide the option for
// uploading Bluetooth logs as well.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_ProvideBluetoothLogs) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kGoogleInternal, std::string(),
                  /*from_assistant*/ false, /*include_bluetooth_logs*/ true);
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  // It shouldn't be visible until we put the Bluetooth text into the
  // description.
  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "((function() {"
                "    if ($('bluetooth-checkbox-container') != null &&"
                "        $('bluetooth-checkbox-container').hidden == true) {"
                "      return true;"
                "    }"
                "    return false;"
                "  })());"));
  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "((function() {"
                "    var elem = document.getElementById('description-text');"
                "    elem.value = 'bluetooth';"
                "    elem.dispatchEvent(new Event('input', {}));"
                "    if ($('bluetooth-checkbox-container') != null &&"
                "        $('bluetooth-checkbox-container').hidden == false) {"
                "      return true;"
                "    }"
                "    return false;"
                "  })());"));
}

// Ensures that when triggered from a Google account and a Bluetooth related
// string is entered into the description, that we append Bluetooth-related
// questions to the issue description.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_AppendQuestionnaire) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kGoogleInternal, std::string(),
                  /*from_assistant*/ false, /*include_bluetooth_logs*/ true,
                  /*show_questionnaire*/ true);
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  // Questionnaire shouldn't be visible until we put the Bluetooth text into the
  // description.
  EXPECT_EQ(
      true,
      content::EvalJs(
          content,
          "((function() {"
          "    return !$('description-text').value.includes('please answer');"
          "  })());"));

  // Bluetooth questions should appear.
  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "((function() {"
                "    var elem = document.getElementById('description-text');"
                "    elem.value = 'bluetooth';"
                "    elem.dispatchEvent(new Event('input', {}));"
                "    return elem.value.includes('please answer')"
                "        && elem.value.includes('[Bluetooth]')"
                "        && !elem.value.includes('[WiFi]');"
                "  })());"));

  // WiFi questions should appear.
  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "((function() {"
                "    var elem = document.getElementById('description-text');"
                "    elem.value = 'wifi issue';"
                "    elem.dispatchEvent(new Event('input', {}));"
                "    return elem.value.includes('[WiFi]');"
                "  })());"));
}

// Questionnaires should not be displayed if it's not a Googler session.
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_AppendQuestionnaireNotGoogler) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kRegular, std::string(),
                  /*from_assistant*/ false, /*include_bluetooth_logs*/ false,
                  /*show_questionnaire*/ false);
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  // Questionnaire shouldn't be visible in the beginning.
  EXPECT_EQ(
      true,
      content::EvalJs(
          content,
          "((function() {"
          "    return !$('description-text').value.includes('[Bluetooth]');"
          "  })()));"));

  // Questionnaire should not appear even with a Bluetooth keyword.
  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "((function() {"
                "    var elem = document.getElementById('description-text');"
                "    elem.value = 'bluetooth';"
                "    elem.dispatchEvent(new Event('input', {}));"
                "    return !elem.value.includes('please answer');"
                "  })());"));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Disabled due to flake: https://crbug.com/1069870
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_GetTargetTabUrl) {
  const std::pair<std::string, std::string> test_cases[] = {
      {"https://www.google.com/", "https://www.google.com/"},
      {"chrome://version/", chrome::kChromeUIVersionURL},
      {chrome::kChromeUIBookmarksURL, chrome::kChromeUIBookmarksURL},
  };

  for (const auto& test_case : test_cases) {
    GURL expected_url = GURL(test_case.second);

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(test_case.first)));

    // Sanity check that we always have one tab in the browser.
    ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

    ASSERT_EQ(expected_url, browser()
                                ->tab_strip_model()
                                ->GetWebContentsAt(0)
                                ->GetLastCommittedURL());

    ASSERT_EQ(expected_url,
              chrome::GetTargetTabUrl(browser()->session_id(), 0));

    // Open a DevTools window.
    DevToolsWindow* devtools_window =
        DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);

    // Verify the expected url returned from GetTargetTabUrl against a
    // DevTools window.
    ASSERT_EQ(expected_url, chrome::GetTargetTabUrl(
                                DevToolsWindowTesting::Get(devtools_window)
                                    ->browser()
                                    ->session_id(),
                                0));

    DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
  }
}

// Disabled due to flake: https://crbug.com/1180373
IN_PROC_BROWSER_TEST_F(FeedbackTest, DISABLED_SubmissionTest) {
  WaitForExtensionViewsToLoad();

  ASSERT_TRUE(IsFeedbackAppAvailable());
  StartFeedbackUI(FeedbackFlow::kGoogleInternal, std::string());
  VerifyFeedbackAppLaunch();

  AppWindow* const window =
      PlatformAppBrowserTest::GetFirstAppWindowForBrowser(browser());
  ASSERT_TRUE(window);
  content::WebContents* const content = window->web_contents();

  // Set a delegate for the uploader which will be invoked when the report
  // normally would have been uploaded. We have it setup to then quit the
  // RunLoop which will then allow us to terminate.
  base::RunLoop run_loop;
  TestFeedbackUploaderDelegate delegate(run_loop.QuitClosure());
  feedback::FeedbackUploaderFactoryChrome::GetInstance()
      ->GetForBrowserContext(browser()->profile())
      ->set_feedback_uploader_delegate(&delegate);

  // Click the send button.
  EXPECT_EQ(true,
            content::EvalJs(
                content,
                "((function() {"
                "    if ($('send-report-button') != null) {"
                "      document.getElementById('send-report-button').click();"
                "      return true;"
                "    }"
                "    return false;"
                "  })());"));

  // This will DCHECK if the JS private API call doesn't return a value, which
  // is the main case we are concerned about.
  run_loop.Run();
  feedback::FeedbackUploaderFactoryChrome::GetInstance()
      ->GetForBrowserContext(browser()->profile())
      ->set_feedback_uploader_delegate(nullptr);
}

}  // namespace extensions
