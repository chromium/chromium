// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/email_verifier_delegate.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;

class MockEmailVerifier : public content::webid::EmailVerifier {
 public:
  MOCK_METHOD(void,
              Verify,
              (const std::string&, const std::string&, OnEmailVerifiedCallback),
              (override));
};

class EmailVerificationBrowserTest : public InProcessBrowserTest {
 public:
  EmailVerificationBrowserTest() {
    feature_list_.InitAndEnableFeature(::features::kEmailVerificationProtocol);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &EmailVerificationBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path() != "/autofill/email_verification.html") {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(R"(
<!DOCTYPE html>
<html>
<body>
  <form id="testform">
    <input type="email" id="email" name="email" nonce="test_nonce">
    <input
      type="hidden"
      id="verification"
      autocomplete="email-verification-token"
    >
    <input type="submit" id="submit_button">
  </form>
  <script>
    window.tokenPromise = new Promise(resolve => {
      document.getElementById('testform').onsubmit = (event) => {
        event.preventDefault();
        const token = document.getElementById('verification').value;
        resolve(token);
      };
    });
  </script>
</body>
</html>
    )");
    response->set_content_type("text/html");
    return response;
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockEmailVerifier> email_verifier_;
};

IN_PROC_BROWSER_TEST_F(EmailVerificationBrowserTest, FullFlowRendererStorage) {
  GURL url =
      embedded_test_server()->GetURL("/autofill/email_verification.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // 1. Setup the EmailVerifierDelegate with our mock.
  email_verifier_ = std::make_unique<NiceMock<MockEmailVerifier>>();
  auto* verifier_ptr = email_verifier_.get();
  content::webid::EmailVerifier::SetForFrameForTest(
      web_contents()->GetPrimaryMainFrame(), std::move(email_verifier_));

  // 2. Prepare a profile for Autofill.
  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(EMAIL_ADDRESS, u"test@example.com");
  AddTestProfile(browser()->profile(), profile);

  const std::string kTestToken = "renderer_side_token_abc";
  EXPECT_CALL(*verifier_ptr, Verify("test@example.com", "test_nonce", _))
      .WillOnce(RunOnceCallback<2>(kTestToken));

  BrowserAutofillManager* manager = static_cast<BrowserAutofillManager*>(
      &ContentAutofillDriver::GetForRenderFrameHost(
           web_contents()->GetPrimaryMainFrame())
           ->GetAutofillManager());

  // Wait for forms to be processed.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !test_api(*manager).form_structures().empty(); }));

  const FormStructure* form_structure = nullptr;
  for (const FormStructure* form : test_api(*manager).form_structures()) {
    if (!form->fields().empty() && form->field(0)->nonce() == u"test_nonce") {
      form_structure = form;
      break;
    }
  }
  ASSERT_TRUE(form_structure);

  const_cast<AutofillField*>(form_structure->field(0))
      ->set_autofilled_type(EMAIL_ADDRESS);

  FormGlobalId form_id = form_structure->global_id();
  FieldGlobalId field_id = form_structure->field(0)->global_id();

  // 3. Simulate selection (triggers Verify -> SendEmailVerificationToken).
  // The token is now in renderer's memory but not in DOM.
  base::flat_set<FieldGlobalId> filled_field_ids = {field_id};
  manager->NotifyObservers(&AutofillManager::Observer::OnFillOrPreviewForm,
                           form_id, mojom::ActionPersistence::kFill,
                           filled_field_ids, &profile);

  // 4. Submit the form.
  // This will trigger FormTracker::WillSendSubmitEvent ->
  // AutofillAgent::OnBeforeFormSubmitted. The attribute is injected
  // SYNCHRONOUSLY.
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('testform').requestSubmit();"));

  // 5. Verify the token was found in onsubmit WITHOUT any delay.
  EXPECT_EQ(
      kTestToken,
      content::EvalJs(web_contents(), "window.tokenPromise").ExtractString());
}

}  // namespace
}  // namespace autofill
