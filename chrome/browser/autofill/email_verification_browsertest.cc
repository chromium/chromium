// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

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
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/browser/webid/email_verifier.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"

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

class TestRuntimeFeatureStateContext
    : public blink::RuntimeFeatureStateContext {
 public:
  TestRuntimeFeatureStateContext() {
    feature_overrides_
        [blink::mojom::RuntimeFeature::kEmailVerificationProtocol] = true;
  }
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
    SetupAutofillProfile(u"test@example.com");
  }

  class TestEmailVerificationAutofillClient : public TestContentAutofillClient {
   public:
    explicit TestEmailVerificationAutofillClient(
        content::WebContents* web_contents)
        : TestContentAutofillClient(web_contents) {}
    MOCK_METHOD(void, ShowEmailVerifiedToast, (), (override));
    MOCK_METHOD(void,
                ShowEmailVerificationPopup,
                (const gfx::RectF&,
                 const net::SchemefulSite&,
                 const std::u16string&,
                 base::OnceCallback<void(bool)>),
                (override));

    EmailVerifierDelegate& email_verifier_delegate() {
      return email_verifier_delegate_;
    }

   private:
    EmailVerifierDelegate email_verifier_delegate_{this};
  };

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TestEmailVerificationAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

  void EnableEmailVerificationFeatureForFrame(content::RenderFrameHost* rfh) {
    if (content::RuntimeFeatureStateDocumentData::GetForCurrentDocument(rfh)) {
      content::RuntimeFeatureStateDocumentData::DeleteForCurrentDocument(rfh);
    }
    content::RuntimeFeatureStateDocumentData::CreateForCurrentDocument(
        rfh, TestRuntimeFeatureStateContext());
  }

  MockEmailVerifier* SetupMockEmailVerifier(content::RenderFrameHost* rfh) {
    auto email_verifier = std::make_unique<NiceMock<MockEmailVerifier>>();
    MockEmailVerifier* verifier_ptr = email_verifier.get();
    content::webid::EmailVerifier::SetForFrameForTest(
        rfh, std::move(email_verifier));
    return verifier_ptr;
  }

  void SetupAutofillProfile(const std::u16string& email) {
    autofill_profile_ = test::GetFullProfile();
    autofill_profile_->SetRawInfo(EMAIL_ADDRESS, email);
    AddTestProfile(browser()->profile(), *autofill_profile_);
  }

  BrowserAutofillManager* GetBrowserAutofillManager(
      content::RenderFrameHost* rfh) {
    return static_cast<BrowserAutofillManager*>(
        &ContentAutofillDriver::GetForRenderFrameHost(rfh)
             ->GetAutofillManager());
  }

 private:
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
    <input type="email" id="email" name="email">
    <input
      type="hidden"
      id="verification"
      nonce="test_nonce"
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

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<
      testing::NiceMock<TestEmailVerificationAutofillClient>>
      autofill_client_injector_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockEmailVerifier> email_verifier_;

 protected:
  std::optional<AutofillProfile> autofill_profile_;
};

// Tests the full flow of the email verification protocol. It simulates a user
// autofilling an email field, which triggers the verification prompt. Once the
// user confirms, a verification token is retrieved from the identity provider
// and stored in the renderer's memory. Finally, the token is successfully
// injected into the DOM at form submission.
IN_PROC_BROWSER_TEST_F(EmailVerificationBrowserTest, FullFlowRendererStorage) {
  GURL url =
      embedded_test_server()->GetURL("/autofill/email_verification.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  EnableEmailVerificationFeatureForFrame(main_frame);

  // 1. Setup the EmailVerifierDelegate with our mock.
  const std::string kTestToken = "renderer_side_token_abc";
  MockEmailVerifier* verifier_ptr = SetupMockEmailVerifier(main_frame);
  EXPECT_CALL(*verifier_ptr, Verify("test@example.com", "test_nonce", _))
      .WillOnce(RunOnceCallback<2>(content::webid::EmailVerifier::Result{
          kTestToken, net::SchemefulSite(GURL("https://example.com"))}));

  BrowserAutofillManager* manager = GetBrowserAutofillManager(main_frame);

  // Wait for forms to be processed.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !test_api(*manager).form_structures().empty(); }));

  const FormStructure* form_structure = nullptr;
  for (const FormStructure* form : test_api(*manager).form_structures()) {
    bool found_nonce = std::ranges::any_of(
        form->fields(), [](const std::unique_ptr<AutofillField>& field) {
          return field->nonce() == u"test_nonce";
        });
    if (found_nonce) {
      form_structure = form;
      break;
    }
  }
  ASSERT_TRUE(form_structure);

  FieldGlobalId field_id = form_structure->field(0)->global_id();

  // 3. Simulate selection (triggers Verify -> SendEmailVerificationToken).
  // The token is now in renderer's memory but not in DOM.
  TestEmailVerificationAutofillClient* mock_client = client();
  ASSERT_EQ(&manager->client(), mock_client);

  base::RunLoop popup_run_loop;
  EXPECT_CALL(*mock_client, ShowEmailVerificationPopup)
      .WillOnce([&](const gfx::RectF&, const net::SchemefulSite&,
                    const std::u16string&,
                    base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
        popup_run_loop.Quit();
      });
  EXPECT_CALL(*mock_client, ShowEmailVerifiedToast);

  manager->FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form_structure->ToFormData(), field_id,
      &autofill_profile_.value(), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{});

  // 4. Wait for the deferred browser task to execute and trigger the popup
  // callback.
  popup_run_loop.Run();

  // 5. Submit the form.
  // This will trigger FormTracker::WillSendSubmitEvent ->
  // AutofillAgent::OnBeforeFormSubmitted. The attribute is injected
  // SYNCHRONOUSLY.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('email').value = 'test@example.com';"));
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('testform').requestSubmit();"));

  // 6. Verify the token was found in onsubmit WITHOUT any delay.
  EXPECT_EQ(
      kTestToken,
      content::EvalJs(web_contents(), "window.tokenPromise").ExtractString());
}

}  // namespace
}  // namespace autofill
