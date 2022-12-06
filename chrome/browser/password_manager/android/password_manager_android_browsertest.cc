// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

autofill::PasswordFormFillData GetTestFillData() {
  autofill::PasswordFormFillData fill_data;
  autofill::FormFieldData username_field;
  username_field.name = u"username_field";
  fill_data.username_field = username_field;
  autofill::FormFieldData password_field;
  password_field.name = u"password_field";
  password_field.form_control_type = "password";
  fill_data.password_field = password_field;
  // Renderer IDs should match real elements' IDs. They cannot be retrieved in a
  // content::BrowserTestBase, so they are guessed based on the fact the
  // username and password fields are the first two elements on the page.
  fill_data.username_field.unique_renderer_id = autofill::FieldRendererId(1);
  fill_data.password_field.unique_renderer_id = autofill::FieldRendererId(2);
  return fill_data;
}

}  // namespace

class PasswordManagerAndroidBrowserTest
    : public AndroidBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PasswordManagerAndroidBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~PasswordManagerAndroidBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server. This must be used in
    // conjunction with switches::kIgnoreCertificateErrors to work.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Setup HTTPS server serving files from standard test directory.
    static constexpr base::FilePath::CharType kDocRoot[] =
        FILE_PATH_LITERAL("chrome/test/data");
    https_server_.ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
    ASSERT_TRUE(https_server_.Start());
  }

  void NavigateToFile(const std::string& file_path) {
    PasswordsNavigationObserver observer(GetActiveWebContents());
    EXPECT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       https_server_.GetURL(file_path)));
    ASSERT_TRUE(observer.Wait());
  }

 private:
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_P(PasswordManagerAndroidBrowserTest,
                       TriggerFormSubmission) {
  base::HistogramTester uma_recorder;
  bool has_form_tag = GetParam();
  NavigateToFile(has_form_tag ? "/password/simple_password.html"
                              : "/password/no_form_element.html");

  password_manager::ContentPasswordManagerDriverFactory* driver_factory =
      password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
          GetActiveWebContents());
  password_manager::ContentPasswordManagerDriver* driver =
      driver_factory->GetDriverForFrame(
          GetActiveWebContents()->GetPrimaryMainFrame());

  PasswordsNavigationObserver observer(GetActiveWebContents());
  observer.SetPathToWaitFor("/password/done.html");

  // Send a fill data to render.
  autofill::PasswordFormFillData fill_data = GetTestFillData();
  // Don't fill right now, just inform the rendered that the form is fillable.
  // To make the test closer to TouchToFill, use |FillSuggestion| to fill a
  // credential later.
  fill_data.wait_for_username = true;
  driver->SetPasswordFillData(fill_data);

  // A user taps the username field.
  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "document.getElementById('username_field').focus();"));

  // A user accepts a credential in TouchToFill. That fills in the credential
  // and submits it.
  ChromePasswordManagerClient::FromWebContents(GetActiveWebContents())
      ->StartSubmissionTrackingAfterTouchToFill(u"username");
  driver->FillSuggestion(u"username", u"password");
  driver->TriggerFormSubmission();

  ASSERT_TRUE(observer.Wait());

  uma_recorder.ExpectTotalCount(
      "PasswordManager.TouchToFill.TimeToSuccessfulLogin", 1);
}

INSTANTIATE_TEST_SUITE_P(VariateFormElementPresence,
                         PasswordManagerAndroidBrowserTest,
                         testing::Bool());
