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
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
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
using ::content::webid::EmailVerifier;
using ::testing::_;
using ::testing::NiceMock;

class MockEmailVerifier : public content::webid::EmailVerifier {
 public:
  MOCK_METHOD(void,
              CheckIfVerifiable,
              (const std::string&, IsVerifiableCallback),
              (override));
  MOCK_METHOD(void,
              Verify,
              (const Result&, const std::string&, OnEmailVerifiedCallback),
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
    MOCK_METHOD(void, ShowEmailVerifiedToast, (const GURL&), (override));
    MOCK_METHOD(void,
                ShowEmailVerificationPopup,
                (const gfx::RectF&,
                 const net::SchemefulSite&,
                 const std::u16string&,
                 base::OnceCallback<void(
                     AutofillClient::EmailVerificationPermissionUiResult)>),
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
  content::webid::EmailVerifier::Result result;
  result.email = "test@example.com";
  result.issuer_site = net::SchemefulSite(GURL("https://example.com"));
  result.issuance_endpoint = GURL("https://example.com/issuance");
  result.signing_alg_values_supported.push_back("RS256");

  EXPECT_CALL(*verifier_ptr, CheckIfVerifiable("test@example.com", _))
      .WillOnce(RunOnceCallback<1>(result));

  EXPECT_CALL(*verifier_ptr, Verify(_, "test_nonce", _))
      .WillOnce(RunOnceCallback<2>(kTestToken));

  BrowserAutofillManager* manager = GetBrowserAutofillManager(main_frame);

  // Wait for forms to be processed.
  const FormStructure* form_structure = WaitForMatchingForm(
      manager, base::BindRepeating([](const FormStructure& form) {
        return std::ranges::any_of(
            form.fields(), [](const std::unique_ptr<AutofillField>& field) {
              return field->nonce() == u"test_nonce";
            });
      }));
  ASSERT_TRUE(form_structure);

  FieldGlobalId field_id = form_structure->field(0)->global_id();

  // 2. Simulate selection (triggers Verify -> SendEmailVerificationToken).
  // The token is now in renderer's memory but not in DOM.
  TestEmailVerificationAutofillClient* mock_client = client();

  ASSERT_EQ(&manager->client(), mock_client);

  base::RunLoop popup_run_loop;
  EXPECT_CALL(*mock_client, ShowEmailVerificationPopup)
      .WillOnce([&](const gfx::RectF&, const net::SchemefulSite&,
                    const std::u16string&,
                    base::OnceCallback<void(
                        AutofillClient::EmailVerificationPermissionUiResult)>
                        callback) {
        std::move(callback).Run(
            AutofillClient::EmailVerificationPermissionUiResult::kAccepted);
        popup_run_loop.Quit();
      });
  EXPECT_CALL(*mock_client,
              ShowEmailVerifiedToast(GURL("https://example.com")));

  manager->FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form_structure->global_id(), field_id,
      &autofill_profile_.value(), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{});

  // 3. Wait for the deferred browser task to execute and trigger the popup
  // callback.
  popup_run_loop.Run();

  // 4. Submit the form.
  // This will trigger
  // AutofillAgent::EmailVerificationObserver::WillSendSubmitEvent. The token
  // value is injected into the verification token field.

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('email').value = 'test@example.com';"));
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('testform').requestSubmit();"));

  // 5. Verify the token was found in onsubmit.

  EXPECT_EQ(
      kTestToken,
      content::EvalJs(web_contents(), "window.tokenPromise").ExtractString());
}

// Tests the single-field refilling flow of the email verification protocol.
// It simulates a user first autofilling an email field (which triggers the
// initial verification and retrieves a token), and then replacing the value
// of the same email field with a different email address (which triggers a
// second verification flow). Finally, it verifies that the second, updated
// token is successfully injected into the DOM at form submission.
IN_PROC_BROWSER_TEST_F(EmailVerificationBrowserTest,
                       FullFlowSingleFieldRefilling) {
  GURL url =
      embedded_test_server()->GetURL("/autofill/email_verification.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  EnableEmailVerificationFeatureForFrame(main_frame);

  // 1. Setup the EmailVerifierDelegate with our mock.
  const std::string kTestToken1 = "renderer_side_token_abc";
  const std::string kTestToken2 = "other_renderer_side_token_xyz";
  MockEmailVerifier* verifier_ptr = SetupMockEmailVerifier(main_frame);

  EmailVerifier::Result result1;
  result1.email = "test@example.com";
  result1.issuer_site = net::SchemefulSite(GURL("https://example.com"));

  EmailVerifier::Result result2;
  result2.email = "other@example.com";
  result2.issuer_site = net::SchemefulSite(GURL("https://example.com"));

  testing::InSequence s;

  EXPECT_CALL(*verifier_ptr, CheckIfVerifiable("test@example.com", _))
      .WillOnce(RunOnceCallback<1>(result1));

  BrowserAutofillManager* manager = GetBrowserAutofillManager(main_frame);

  // Wait for forms to be processed.
  const FormStructure* form_structure = WaitForMatchingForm(
      manager, base::BindRepeating([](const FormStructure& form) {
        return std::ranges::any_of(
            form.fields(), [](const std::unique_ptr<AutofillField>& field) {
              return field->nonce() == u"test_nonce";
            });
      }));
  ASSERT_TRUE(form_structure);

  FieldGlobalId field_id = form_structure->field(0)->global_id();

  // 2. Simulate the initial form fill.
  TestEmailVerificationAutofillClient* mock_client = client();
  base::RunLoop popup_run_loop1;

  EXPECT_CALL(*mock_client, ShowEmailVerificationPopup)
      .WillOnce([&](const gfx::RectF&, const net::SchemefulSite&,
                    const std::u16string&,
                    base::OnceCallback<void(
                        AutofillClient::EmailVerificationPermissionUiResult)>
                        callback) {
        std::move(callback).Run(
            AutofillClient::EmailVerificationPermissionUiResult::kAccepted);
        popup_run_loop1.Quit();
      });

  EXPECT_CALL(
      *verifier_ptr,
      Verify(testing::Field(&EmailVerifier::Result::email, "test@example.com"),
             "test_nonce", _))
      .WillOnce(RunOnceCallback<2>(kTestToken1));

  ASSERT_EQ(&manager->client(), mock_client);

  manager->FillOrPreviewForm(
      mojom::ActionPersistence::kFill, form_structure->global_id(), field_id,
      &autofill_profile_.value(), AutofillTriggerSource::kPopup,
      /*blocked_fields=*/{});

  popup_run_loop1.Run();

  // 3. Simulate selecting a different value (triggers second Verify).
  base::RunLoop popup_run_loop2;

  EXPECT_CALL(*verifier_ptr, CheckIfVerifiable("other@example.com", _))
      .WillOnce(RunOnceCallback<1>(result2));

  EXPECT_CALL(*mock_client, ShowEmailVerificationPopup)
      .WillOnce([&](const gfx::RectF&, const net::SchemefulSite&,
                    const std::u16string&,
                    base::OnceCallback<void(
                        AutofillClient::EmailVerificationPermissionUiResult)>
                        callback) {
        std::move(callback).Run(
            AutofillClient::EmailVerificationPermissionUiResult::kAccepted);
        popup_run_loop2.Quit();
      });

  EXPECT_CALL(
      *verifier_ptr,
      Verify(testing::Field(&EmailVerifier::Result::email, "other@example.com"),
             "test_nonce", _))
      .WillOnce(RunOnceCallback<2>(kTestToken2));

  EXPECT_CALL(*mock_client, ShowEmailVerifiedToast);

  FormData form_data = form_structure->ToFormData();
  FormFieldData field_data = form_data.fields()[0];
  // Simulate that the field already has the filled value from the first fill.
  field_data.set_value(u"test@example.com");
  field_data.set_is_autofilled_according_to_renderer(true);

  manager->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      form_data.global_id(), field_data.global_id(), u"other@example.com",
      FillingProduct::kAddress, EMAIL_ADDRESS);

  popup_run_loop2.Run();

  // 4. Submit the form.
  // This will trigger
  // AutofillAgent::EmailVerificationObserver::WillSendSubmitEvent. The token
  // value is injected into the verification token field.

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('email').value = 'other@example.com';"));
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('testform').requestSubmit();"));

  // 5. Verify the second token was found in onsubmit.
  EXPECT_EQ(
      kTestToken2,
      content::EvalJs(web_contents(), "window.tokenPromise").ExtractString());
}

// Tests that the email verification popup triggers successfully when the email
// field is filled using Autocomplete (where the field type used is
// std::nullopt, but the field itself is classified as EMAIL_ADDRESS).
IN_PROC_BROWSER_TEST_F(EmailVerificationBrowserTest, FullFlowAutocomplete) {
  GURL url =
      embedded_test_server()->GetURL("/autofill/email_verification.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  EnableEmailVerificationFeatureForFrame(main_frame);

  // 1. Setup the EmailVerifierDelegate with our mock.
  const std::string kTestToken = "renderer_side_token_abc";
  MockEmailVerifier* verifier_ptr = SetupMockEmailVerifier(main_frame);

  EmailVerifier::Result result;
  result.email = "test@example.com";
  result.issuer_site = net::SchemefulSite(GURL("https://example.com"));
  result.issuance_endpoint = GURL("https://example.com/issuance");
  result.signing_alg_values_supported.push_back("RS256");

  EXPECT_CALL(*verifier_ptr, CheckIfVerifiable("test@example.com", _))
      .WillOnce(RunOnceCallback<1>(result));

  EXPECT_CALL(
      *verifier_ptr,
      Verify(testing::Field(&EmailVerifier::Result::email, "test@example.com"),
             "test_nonce", _))
      .WillOnce(RunOnceCallback<2>(kTestToken));

  BrowserAutofillManager* manager = GetBrowserAutofillManager(main_frame);

  // Wait for forms to be processed.
  const FormStructure* form_structure = WaitForMatchingForm(
      manager, base::BindRepeating([](const FormStructure& form) {
        return std::ranges::any_of(
            form.fields(), [](const std::unique_ptr<AutofillField>& field) {
              return field->nonce() == u"test_nonce";
            });
      }));
  ASSERT_TRUE(form_structure);

  // Simulate autocomplete suggestion selection.
  TestEmailVerificationAutofillClient* mock_client = client();
  ASSERT_EQ(&manager->client(), mock_client);

  base::RunLoop popup_run_loop;
  EXPECT_CALL(*mock_client, ShowEmailVerificationPopup)
      .WillOnce([&](const gfx::RectF&, const net::SchemefulSite&,
                    const std::u16string&,
                    base::OnceCallback<void(
                        AutofillClient::EmailVerificationPermissionUiResult)>
                        callback) {
        std::move(callback).Run(
            AutofillClient::EmailVerificationPermissionUiResult::kAccepted);
        popup_run_loop.Quit();
      });
  EXPECT_CALL(*mock_client, ShowEmailVerifiedToast);

  FormData form_data = form_structure->ToFormData();
  FormFieldData field_data = form_data.fields()[0];

  // In autocomplete filling, field_type_used is std::nullopt and product is
  // kAutocomplete.
  manager->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      form_data.global_id(), field_data.global_id(), u"test@example.com",
      FillingProduct::kAutocomplete, std::nullopt);

  // Wait for the deferred browser task to execute and trigger the popup
  // callback.
  popup_run_loop.Run();

  // Submit the form.
  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.getElementById('email').value = 'test@example.com';"));
  ASSERT_TRUE(content::ExecJs(
      web_contents(), "document.getElementById('testform').requestSubmit();"));

  // Verify the token was found in onsubmit.
  EXPECT_EQ(
      kTestToken,
      content::EvalJs(web_contents(), "window.tokenPromise").ExtractString());
}

}  // namespace
}  // namespace autofill
