// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/browser/media/webrtc/webrtc_logging_controller.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

class WebRtcTextLogCollectionAllowedPolicyTest : public policy::PolicyTest {
 public:
  ~WebRtcTextLogCollectionAllowedPolicyTest() override = default;

  WebRtcTextLogCollectionAllowedPolicyTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  const PrefService::Preference* GetPreference() const {
    auto* service = user_prefs::UserPrefs::Get(browser()->profile());
    return service->FindPreference(prefs::kWebRtcTextLogCollectionAllowed);
  }

  void SetPreferenceValue(bool value) {
    auto* service = user_prefs::UserPrefs::Get(browser()->profile());
    return service->SetBoolean(prefs::kWebRtcTextLogCollectionAllowed, value);
  }

  WebRtcLoggingController* CreateHostAndController() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    content::RenderProcessHost* host =
        web_contents->GetPrimaryMainFrame()->GetProcess();

    return WebRtcLoggingController::FromRenderProcessHost(host);
  }

  base::OnceCallback<void(bool, const std::string&)>
  LoggingCallbackExpectingSuccess(base::RunLoop* run_loop,
                                  bool expected_result) {
    return base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_result, bool value,
           const std::string& error_message) {
          EXPECT_EQ(expected_result, value);
          run_loop->Quit();
        },
        run_loop, expected_result);
  }

  base::OnceCallback<void(bool, const std::string&, const std::string&)>
  UploadDataDoneCallbackExpectingError(base::RunLoop* run_loop,
                                       bool expected_result) {
    return base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_result, bool value,
           const std::string& report_id, const std::string& error_message) {
          EXPECT_EQ(expected_result, value);
          EXPECT_STREQ("", report_id.c_str());
          EXPECT_STREQ(WebRtcLogUploader::kLogUploadDisabledMsg,
                       error_message.c_str());
          run_loop->Quit();
        },
        run_loop, expected_result);
  }

  base::OnceCallback<void(bool, const std::string&, const std::string&)>
  UploadDataDoneCallbackExpectLogNotStopped(base::RunLoop* run_loop,
                                            bool expected_result) {
    return base::BindOnce(
        [](base::RunLoop* run_loop, bool expected_result, bool value,
           const std::string& report_id, const std::string& error_message) {
          EXPECT_EQ(expected_result, value);
          EXPECT_STREQ("", report_id.c_str());
          EXPECT_STREQ("Logging not stopped or no log open.",
                       error_message.c_str());
          run_loop->Quit();
        },
        run_loop, expected_result);
  }
};

IN_PROC_BROWSER_TEST_F(WebRtcTextLogCollectionAllowedPolicyTest,
                       RunDisabledUploadLogTest) {
  SetPreferenceValue(false);
  const PrefService::Preference* const pref = GetPreference();
  ASSERT_EQ(pref->GetValue()->GetBool(), false);

  WebRtcLoggingController* webrtc_logging_controller =
      CreateHostAndController();

  {
    base::RunLoop run_loop;
    webrtc_logging_controller->StartLogging(
        LoggingCallbackExpectingSuccess(&run_loop, true));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    webrtc_logging_controller->StopLogging(
        LoggingCallbackExpectingSuccess(&run_loop, true));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    webrtc_logging_controller->UploadLog(
        UploadDataDoneCallbackExpectingError(&run_loop, false));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(WebRtcTextLogCollectionAllowedPolicyTest,
                       RunUploadStoredLogTest) {
  SetPreferenceValue(false);
  const PrefService::Preference* const pref = GetPreference();
  ASSERT_EQ(pref->GetValue()->GetBool(), false);

  WebRtcLoggingController* webrtc_logging_controller =
      CreateHostAndController();

  {
    base::RunLoop run_loop;
    webrtc_logging_controller->StartLogging(
        LoggingCallbackExpectingSuccess(&run_loop, true));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    webrtc_logging_controller->StopLogging(
        LoggingCallbackExpectingSuccess(&run_loop, true));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    webrtc_logging_controller->StoreLog(
        "test_log_id", LoggingCallbackExpectingSuccess(&run_loop, true));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    webrtc_logging_controller->UploadStoredLog(
        "test_log_id", UploadDataDoneCallbackExpectingError(&run_loop, false));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(WebRtcTextLogCollectionAllowedPolicyTest,
                       UploadLogBeforeLoggingStopped) {
  SetPreferenceValue(true);
  const PrefService::Preference* const pref = GetPreference();
  ASSERT_EQ(pref->GetValue()->GetBool(), true);

  WebRtcLoggingController* webrtc_logging_controller =
      CreateHostAndController();

  {
    base::RunLoop run_loop;
    webrtc_logging_controller->StartLogging(
        LoggingCallbackExpectingSuccess(&run_loop, true));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    webrtc_logging_controller->UploadLog(
        UploadDataDoneCallbackExpectLogNotStopped(&run_loop, false));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    webrtc_logging_controller->StopLogging(
        LoggingCallbackExpectingSuccess(&run_loop, true));
    run_loop.Run();
  }
}
