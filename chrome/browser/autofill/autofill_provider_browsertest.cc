// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/android_autofill/browser/android_autofill_manager.h"
#include "components/android_autofill/browser/test_autofill_provider.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"

using ::testing::_;
using ::testing::Invoke;

namespace autofill {

using mojom::SubmissionSource;

namespace {

class MockAutofillProvider : public TestAutofillProvider {
 public:
  // WebContents takes ownership of the MockAutofillProvider.
  explicit MockAutofillProvider(content::WebContents* web_contents)
      : TestAutofillProvider(web_contents) {}

  ~MockAutofillProvider() override = default;

  MOCK_METHOD4(OnFormSubmitted,
               void(AndroidAutofillManager* manager,
                    const FormData& form,
                    bool,
                    SubmissionSource));

  MOCK_METHOD6(OnQueryFormFieldAutofill,
               void(AndroidAutofillManager* manager,
                    int32_t id,
                    const FormData& form,
                    const FormFieldData& field,
                    const gfx::RectF& bounding_box,
                    bool autoselect_first_suggestion));

  void OnQueryFormFieldAutofillImpl(AndroidAutofillManager* manager,
                                    int32_t id,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& bounding_box,
                                    bool autoselect_first_suggestion) {
    queried_form_ = form;
  }

  void OnFormSubmittedImpl(AndroidAutofillManager*,
                           const FormData& form,
                           bool success,
                           SubmissionSource source) {
    submitted_form_ = form;
  }

  const FormData& queried_form() { return queried_form_; }

  const FormData& submitted_form() { return submitted_form_; }

 private:
  FormData queried_form_;
  FormData submitted_form_;
};

}  // namespace

class AutofillProviderBrowserTest : public InProcessBrowserTest {
 public:
  AutofillProviderBrowserTest() {}
  ~AutofillProviderBrowserTest() override {}

  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    autofill_client_ = std::make_unique<TestAutofillClient>();
    // WebContents takes ownership of the MockAutofillProvider.
    autofill_provider_ = new MockAutofillProvider(WebContents());
    // Serve both a.com and b.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Necessary to avoid flakiness or failure due to input arriving
  // before the first compositor commit.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void CreateContentAutofillDriverFactoryForSubFrame() {
    content::WebContents* web_contents = WebContents();
    ASSERT_TRUE(web_contents != NULL);

    web_contents->RemoveUserData(
        ContentAutofillDriverFactory::
            kContentAutofillDriverFactoryWebContentsUserDataKey);

    // Replace the ContentAutofillDriverFactory for sub frame.
    ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
        web_contents, autofill_client_.get(), "en-US",
        BrowserAutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
        base::BindRepeating(&AndroidAutofillManager::Create));
  }

  void TearDownOnMainThread() override {
    testing::Mock::VerifyAndClearExpectations(autofill_provider_);
  }

  content::RenderFrameHost* GetMainFrame() {
    return WebContents()->GetMainFrame();
  }

  content::WebContents* WebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SimulateUserTypingInFocusedField() {
    content::WebContents* web_contents = WebContents();

    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                              ui::DomCode::US_O, ui::VKEY_O, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('R'),
                              ui::DomCode::US_R, ui::VKEY_R, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('A'),
                              ui::DomCode::US_A, ui::VKEY_A, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('R'),
                              ui::DomCode::US_R, ui::VKEY_R, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('Y'),
                              ui::DomCode::US_Y, ui::VKEY_Y, false, false,
                              false, false);
  }

  void SetLabelChangeExpectationAndTriggerQuery() {
    // One query for the single click, and a second query when the typing is
    // simulated.
    base::RunLoop run_loop;
    EXPECT_CALL(*autofill_provider_, OnQueryFormFieldAutofill(_, _, _, _, _, _))
        .Times(testing::Exactly(2))
        .WillOnce(Invoke(autofill_provider_,
                         &MockAutofillProvider::OnQueryFormFieldAutofillImpl))
        .WillOnce(
            testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

    EXPECT_CALL(*autofill_provider_, OnFormSubmitted)
        .WillOnce(Invoke(autofill_provider_,
                         &MockAutofillProvider::OnFormSubmittedImpl));

    ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(
                                                "/autofill/label_change.html"));

    std::string focus("document.getElementById('address').focus();");
    ASSERT_TRUE(content::ExecuteScript(GetMainFrame(), focus));

    SimulateUserTypingInFocusedField();
    run_loop.Run();
  }

  void ChangeLabelAndCheckResult(const std::string& element_id,
                                 bool expect_forms_same) {
    std::string change_label_and_submit =
        "document.getElementById('" + element_id +
        "').innerHTML='address change';"
        "document.getElementById('submit_button').click();";

    ASSERT_TRUE(
        content::ExecuteScript(GetMainFrame(), change_label_and_submit));
    // Need to pay attention for a message that XHR has finished since there
    // is no navigation to wait for.
    content::DOMMessageQueue message_queue;
    std::string message;
    while (message_queue.WaitForMessage(&message)) {
      if (message == "\"SUBMISSION_FINISHED\"")
        break;
    }

    EXPECT_EQ("\"SUBMISSION_FINISHED\"", message);
    EXPECT_EQ(2u, autofill_provider_->queried_form().fields.size());
    EXPECT_EQ(2u, autofill_provider_->submitted_form().fields.size());
    EXPECT_EQ(expect_forms_same,
              autofill_provider_->submitted_form().SimilarFormAs(
                  autofill_provider_->queried_form()));
  }

 protected:
  MockAutofillProvider* autofill_provider_;

 private:
  std::unique_ptr<TestAutofillClient> autofill_client_;
};

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTest,
                       FrameDetachedOnFormlessSubmission) {
  CreateContentAutofillDriverFactoryForSubFrame();
  EXPECT_CALL(*autofill_provider_,
              OnFormSubmitted(_, _, _, SubmissionSource::FRAME_DETACHED))
      .Times(1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/autofill/frame_detached_on_formless_submit.html"));

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue;

  std::string focus(
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('address_field').focus();");
  ASSERT_TRUE(content::ExecuteScript(GetMainFrame(), focus));

  SimulateUserTypingInFocusedField();
  std::string fill_and_submit =
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('submit_button').click();";

  ASSERT_TRUE(content::ExecuteScript(GetMainFrame(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"SUBMISSION_FINISHED\"")
      break;
  }
  EXPECT_EQ("\"SUBMISSION_FINISHED\"", message);
}

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTest,
                       FrameDetachedOnFormSubmission) {
  CreateContentAutofillDriverFactoryForSubFrame();
  EXPECT_CALL(*autofill_provider_,
              OnFormSubmitted(_, _, _, SubmissionSource::FORM_SUBMISSION))
      .Times(1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/autofill/frame_detached_on_form_submit.html"));

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue;

  std::string focus(
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('address_field').focus();");
  ASSERT_TRUE(content::ExecuteScript(GetMainFrame(), focus));

  SimulateUserTypingInFocusedField();
  std::string fill_and_submit =
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('submit_button').click();";

  ASSERT_TRUE(content::ExecuteScript(GetMainFrame(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"SUBMISSION_FINISHED\"")
      break;
  }
  EXPECT_EQ("\"SUBMISSION_FINISHED\"", message);
}

class AutofillProviderBrowserTestWithSkipFlagOn
    : public AutofillProviderBrowserTest {
 public:
  AutofillProviderBrowserTestWithSkipFlagOn() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillSkipComparingInferredLabels);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AutofillProviderBrowserTestWithSkipFlagOff
    : public AutofillProviderBrowserTest {
 public:
  AutofillProviderBrowserTestWithSkipFlagOff() {
    feature_list_.InitAndDisableFeature(
        features::kAutofillSkipComparingInferredLabels);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1076487): These tests are flaky on all platforms.

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTestWithSkipFlagOn,
                       DISABLED_LabelTagChangeImpactFormComparing) {
  SetLabelChangeExpectationAndTriggerQuery();
  ChangeLabelAndCheckResult("label_id", false /*expect_forms_same*/);
}

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTestWithSkipFlagOn,
                       DISABLED_InferredLabelChangeNotImpactFormComparing) {
  SetLabelChangeExpectationAndTriggerQuery();
  ChangeLabelAndCheckResult("p_id", true /*expect_forms_same*/);
}

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTestWithSkipFlagOff,
                       DISABLED_LabelTagChangeImpactFormComparing) {
  SetLabelChangeExpectationAndTriggerQuery();
  ChangeLabelAndCheckResult("label_id", false /*expect_forms_same*/);
}

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTestWithSkipFlagOff,
                       DISABLED_InferredLabelChangeImpactFormComparing) {
  SetLabelChangeExpectationAndTriggerQuery();
  ChangeLabelAndCheckResult("p_id", false /*expect_forms_same*/);
}

}  // namespace autofill
