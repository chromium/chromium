// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/api/feedback_private.h"

using extensions::api::feedback_private::FeedbackFlow;

namespace extensions {

class FeedbackTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnableUserMediaScreenCapturing);
  }

 protected:
  void StartFeedbackUI(FeedbackFlow flow,
                       const std::string& extra_diagnostics,
                       bool from_assistant = false,
                       bool include_bluetooth_logs = false,
                       bool show_questionnaire = false) {
    extensions::FeedbackPrivateAPI* api =
        extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(
            browser()->profile());
    auto info = api->CreateFeedbackInfo(
        "Test description", "Test placeholder", "Test tag", extra_diagnostics,
        GURL("http://www.test.com"), flow, from_assistant,
        include_bluetooth_logs, show_questionnaire,
        /*from_chrome_labs_or_kaleidoscope=*/false);

    FeedbackDialog::CreateOrShow(browser()->profile(), *info);
  }

  void VerifyFeedbackAppLaunch() {
    feedback_dialog_ = FeedbackDialog::GetInstanceForTest();
    ASSERT_NE(nullptr, feedback_dialog_);

    base::RunLoop run_loop;
    EnsureFeedbackAppUIShown(feedback_dialog_, run_loop.QuitClosure());
    run_loop.Run();

    ASSERT_NE(nullptr, feedback_dialog_);
    ASSERT_NE(nullptr, feedback_dialog_->GetWidget());
    EXPECT_TRUE(feedback_dialog_->GetWidget()->IsVisible());
  }

 private:
  void EnsureFeedbackAppUIShown(FeedbackDialog* feedback_dialog,
                                base::OnceClosure callback) {
    auto* widget = feedback_dialog->GetWidget();
    ASSERT_NE(nullptr, widget);
    if (widget->IsActive()) {
      std::move(callback).Run();
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&FeedbackTest::EnsureFeedbackAppUIShown,
                         base::Unretained(this), feedback_dialog,
                         std::move(callback)),
          base::Seconds(1));
    }
  }

  FeedbackDialog* feedback_dialog_;
};

class TestFeedbackUploaderDelegate
    : public feedback::FeedbackUploaderChrome::Delegate {
 public:
  explicit TestFeedbackUploaderDelegate(base::RunLoop* quit_on_dispatch)
      : quit_on_dispatch_(quit_on_dispatch) {}

  void OnStartDispatchingReport() override { quit_on_dispatch_->Quit(); }

 private:
  raw_ptr<base::RunLoop> quit_on_dispatch_;
};

// TODO(http://b/225380600): Fix the tests on mac.
#if !BUILDFLAG(IS_MAC)

IN_PROC_BROWSER_TEST_F(FeedbackTest, ShowFeedback) {
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR, std::string());
  VerifyFeedbackAppLaunch();
}

IN_PROC_BROWSER_TEST_F(FeedbackTest, ShowLoginFeedback) {
  content::WebContentsAddedObserver observer;

  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_LOGIN, std::string());
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));
  ASSERT_EQ(u"Feedback", content->GetTitle());
  ASSERT_EQ("chrome://feedback/", content->GetURL().spec());

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "window.domAutomationController.send("
      "document.querySelector('#page-url').hidden && "
      "document.querySelector('#attach-file-container').hidden && "
      "document.querySelector('#attach-file-note').hidden);",
      &bool_result));
  EXPECT_TRUE(bool_result);
}

// Tests that there's an option in the email drop down box with a value ''.
IN_PROC_BROWSER_TEST_F(FeedbackTest, AnonymousUser) {
  content::WebContentsAddedObserver observer;

  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR, std::string());
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var options = "
      "document.querySelector('#user-email-drop-down').options;"
      "      for (var option in options) {"
      "        if (options[option].value == '')"
      "          return true;"
      "      }"
      "      return false;"
      "    })()));",
      &bool_result));

  EXPECT_TRUE(bool_result);
}

// Ensures that when extra diagnostics are provided with feedback, they are
// injected properly in the system information.
IN_PROC_BROWSER_TEST_F(FeedbackTest, ExtraDiagnostics) {
  content::WebContentsAddedObserver observer;

  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR, "Some diagnostics");
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var sysInfo = feedbackInfo.systemInformation;"
      "      for (var info in sysInfo) {"
      "        if (sysInfo[info].key == 'EXTRA_DIAGNOSTICS' &&"
      "            sysInfo[info].value == 'Some diagnostics') {"
      "          return true;"
      "        }"
      "      }"
      "      return false;"
      "    })()));",
      &bool_result));

  EXPECT_TRUE(bool_result);
}

// Ensures that when triggered from Assistant with Google account, Assistant
// checkbox are not hidden.
IN_PROC_BROWSER_TEST_F(FeedbackTest, ShowFeedbackFromAssistant) {
  content::WebContentsAddedObserver observer;

  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL, std::string(),
                  /*from_assistant*/ true);
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));

  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = "
      "        document.getElementById('assistant-checkbox-container');"
      "      if (elem != null &&  elem.hidden == true) "
      "      {"
      "        return false;"
      "      }"
      "      return true;"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Ensures that when triggered from a Google account and a Bluetooth related
// string is entered into the description, that we provide the option for
// uploading Bluetooth logs as well.
IN_PROC_BROWSER_TEST_F(FeedbackTest, ProvideBluetoothLogs) {
  content::WebContentsAddedObserver observer;

  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL, std::string(),
                  /*from_assistant*/ false, /*include_bluetooth_logs*/ true);
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));

  // It shouldn't be visible until we put the Bluetooth text into the
  // description.
  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var bluetooth = "
      "        document.getElementById('bluetooth-checkbox-container');"
      "      if (bluetooth != null &&  bluetooth.hidden == true) "
      "      { "
      "        return true; "
      "      } "
      "      return false; "
      "})()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
  bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = document.getElementById('description-text');"
      "      elem.value = 'bluetooth';"
      "      elem.dispatchEvent(new Event('input', {}));"
      "      var bluetooth = "
      "        document.getElementById('bluetooth-checkbox-container');"
      "      if (bluetooth != null && bluetooth.hidden == false) {"
      "        return true; "
      "      }"
      "      return false;"
      "})()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
}

// Ensures that when triggered from a Google account and a Bluetooth related
// string is entered into the description, that we append Bluetooth-related
// questions to the issue description.
IN_PROC_BROWSER_TEST_F(FeedbackTest, AppendQuestionnaire) {
  content::WebContentsAddedObserver observer;
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL, std::string(),
                  /*from_assistant*/ false, /*include_bluetooth_logs*/ true,
                  /*show_questionnaire*/ true);
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));

  // Questionnaire shouldn't be visible until we put the Bluetooth text into
  // the description.
  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = document.getElementById('description-text');"
      "      return !elem.value.includes('please answer');"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);

  // Bluetooth questions should appear.
  bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = document.getElementById('description-text');"
      "      elem.value = 'bluetooth';"
      "      elem.dispatchEvent(new Event('input', {}));"
      "      return elem.value.includes('please answer')"
      "          && elem.value.includes('[Bluetooth]')"
      "          && !elem.value.includes('[WiFi]');"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);

  // WiFi questions should appear.
  bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = document.getElementById('description-text');"
      "      elem.value = 'wifi issue';"
      "      elem.dispatchEvent(new Event('input', {}));"
      "      return elem.value.includes('[WiFi]');"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
}

// Questionnaires should not be displayed if it's not a Googler session.
IN_PROC_BROWSER_TEST_F(FeedbackTest, AppendQuestionnaireNotGoogler) {
  content::WebContentsAddedObserver observer;
  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_REGULAR, std::string(),
                  /*from_assistant*/ false, /*include_bluetooth_logs*/ false,
                  /*show_questionnaire*/ false);
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));

  // Questionnaire shouldn't be visible in the beginning.
  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = document.getElementById('description-text');"
      "      return !elem.value.includes('[Bluetooth]');"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);

  // Questionnaire should not appear even with a Bluetooth keyword.
  bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var elem = document.getElementById('description-text');"
      "      elem.value = 'bluetooth';"
      "      elem.dispatchEvent(new Event('input', {}));"
      "      return !elem.value.includes('please answer');"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(FeedbackTest, GetTargetTabUrl) {
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
  content::WebContentsAddedObserver observer;

  StartFeedbackUI(FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL, std::string());
  VerifyFeedbackAppLaunch();

  content::WebContents* content = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(content));

  // Set a delegate for the uploader which will be invoked when the report
  // normally would have been uploaded. We have it setup to then quit the
  // RunLoop which will then allow us to terminate.
  base::RunLoop run_loop;
  TestFeedbackUploaderDelegate delegate(&run_loop);
  feedback::FeedbackUploaderFactoryChrome::GetInstance()
      ->GetForBrowserContext(browser()->profile())
      ->set_feedback_uploader_delegate(&delegate);

  // Click the send button.
  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      content,
      "domAutomationController.send("
      "  ((function() {"
      "      var button = document.getElementById('send-report-button');"
      "      if (button != null) {"
      "        button.click();"
      "        return true;"
      "      }"
      "      return false;"
      "    })()));",
      &bool_result));
  EXPECT_TRUE(bool_result);

  // This will DCHECK if the JS private API call doesn't return a value, which
  // is the main case we are concerned about.
  run_loop.Run();
  feedback::FeedbackUploaderFactoryChrome::GetInstance()
      ->GetForBrowserContext(browser()->profile())
      ->set_feedback_uploader_delegate(nullptr);
}
#endif

}  // namespace extensions
