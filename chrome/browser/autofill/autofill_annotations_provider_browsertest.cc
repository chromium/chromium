// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace autofill {

namespace {

base::FilePath GetTestDataDir() {
  return base::FilePath(FILE_PATH_LITERAL("components/test/data/autofill"));
}

blink::mojom::AIPageContentOptionsPtr GetAIPageContentOptions() {
  return optimization_guide::DefaultAIPageContentOptions(
      /*on_critical_path =*/true);
}

}  // namespace

// AutofillAnnotationsProvider is a concept of the "content/" layer. But
// because we don't have a fully set up Autofill at the "content/" layer,
// these tests live in chrome/browser/autofill.
class AutofillAnnotationsProviderBrowserTest : public InProcessBrowserTest {
 public:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(autofill::ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver) {}

    [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
        int min_num_awaited_calls) {
      return forms_seen_waiter_.Wait(min_num_awaited_calls);
    }

   private:
    autofill::TestAutofillManagerWaiter forms_seen_waiter_{
        *this,
        {autofill::AutofillManagerEvent::kFormsSeen}};
  };

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(GetTestDataDir());
    content::SetupCrossSiteRedirector(https_server_.get());

    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor, "1.0");
  }

  void SetPageContent(
      base::OnceClosure quit_closure,
      optimization_guide::AIPageContentResultOrError page_content) {
    page_content_ = std::move(page_content->proto);
    std::move(quit_closure).Run();
  }

  optimization_guide::proto::AnnotatedPageContent& page_content() {
    return *page_content_;
  }

  void LoadData() {
    base::RunLoop run_loop;
    GetAIPageContent(
        web_contents(), GetAIPageContentOptions(),
        base::BindOnce(&AutofillAnnotationsProviderBrowserTest::SetPageContent,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    CHECK(page_content_);
  }

  void LoadPage(GURL url, int num_expected_forms) {
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents(), url, 1);

    {
      base::test::TestFuture<bool> future;
      web_contents()
          ->GetPrimaryMainFrame()
          ->GetRenderWidgetHost()
          ->InsertVisualStateCallback(future.GetCallback());
      ASSERT_TRUE(future.Wait()) << "Timeout waiting for syncing with renderer";
    }

    ASSERT_TRUE(autofill_manager()->WaitForFormsSeen(num_expected_forms));

    LoadData();
  }

  // Returns pointers to all `FormControlData` elements in `page_content`.
  std::vector<const optimization_guide::proto::FormControlData*>
  GetFormControlDatas(const optimization_guide::proto::AnnotatedPageContent&
                          page_content LIFETIME_BOUND) {
    std::vector<const optimization_guide::proto::FormControlData*>
        form_control_datas;

    optimization_guide::VisitContentNodes(
        page_content.root_node(),
        page_content.main_frame_data().document_identifier().serialized_token(),
        [&](const optimization_guide::proto::ContentNode& node,
            std::string_view document_identifier) {
          if (node.content_attributes().attribute_type() ==
              optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL) {
            form_control_datas.push_back(
                &node.content_attributes().form_control_data());
          }
        });

    return form_control_datas;
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  TestAutofillManager* autofill_manager() {
    return autofill_manager(web_contents()->GetPrimaryMainFrame());
  }

  TestAutofillManager* autofill_manager(content::RenderFrameHost* rfh) {
    return autofill_manager_injector_[rfh];
  }

  TestContentAutofillClient* client() {
    return autofill_client_injector_[web_contents()];
  }

 protected:
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillClientInjector<TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillManagerInjector<TestAutofillManager>
      autofill_manager_injector_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::optional<optimization_guide::proto::AnnotatedPageContent> page_content_;
};

// Verifies that the `autofill_section_id` and `coarse_autofill_field_type`
// attributes of a FormControlData are correctly set by Autofill.
IN_PROC_BROWSER_TEST_F(AutofillAnnotationsProviderBrowserTest,
                       AutofillFormAnnotations) {
  LoadPage(https_server()->GetURL("/address_sections_and_creditcard.html"),
           /*num_expected_forms=*/1);

  std::vector<const optimization_guide::proto::FormControlData*>
      form_control_datas = GetFormControlDatas(page_content());

  ASSERT_EQ(form_control_datas.size(), 15u);
  for (int i = 0; i < 14; ++i) {
    SCOPED_TRACE(testing::Message() << "Field " << i);
    // Shipping address:
    if (0 <= i && i <= 4) {
      EXPECT_EQ(form_control_datas[i]->autofill_section_id(), 0u);
      ASSERT_EQ(form_control_datas[i]->coarse_autofill_field_type_size(), 1);
      EXPECT_EQ(form_control_datas[i]->coarse_autofill_field_type(0),
                optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS);
    }
    // Billing address:
    if (5 <= i && i <= 9) {
      EXPECT_EQ(form_control_datas[i]->autofill_section_id(), 1u);
      ASSERT_EQ(form_control_datas[i]->coarse_autofill_field_type_size(), 1);
      EXPECT_EQ(form_control_datas[i]->coarse_autofill_field_type(0),
                optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS);
    }
    // Credit card:
    if (10 <= i && i <= 13) {
      EXPECT_EQ(form_control_datas[i]->autofill_section_id(), 2u);
      ASSERT_EQ(form_control_datas[i]->coarse_autofill_field_type_size(), 1);
      EXPECT_EQ(
          form_control_datas[i]->coarse_autofill_field_type(0),
          optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD);
    }
  }
  // Submit button:
  EXPECT_FALSE(form_control_datas[14]->has_autofill_section_id());
  EXPECT_EQ(form_control_datas[14]->coarse_autofill_field_type_size(), 0);
}

// Verifies that the `autofill_section_id` and `coarse_autofill_field_type`
// attributes of a FormControlData are correctly set by Autofill in case the
// main frame contains a <form> with an embedded <iframe>.
IN_PROC_BROWSER_TEST_F(AutofillAnnotationsProviderBrowserTest,
                       AutofillFormAnnotationsIframe) {
  LoadPage(
      https_server()->GetURL("/address_sections_and_creditcard_iframe.html"),
      /*num_expected_forms=*/2);

  std::vector<const optimization_guide::proto::FormControlData*>
      form_control_datas = GetFormControlDatas(page_content());

  ASSERT_EQ(form_control_datas.size(), 17u);
  for (int i = 0; i < 15; ++i) {
    SCOPED_TRACE(testing::Message() << "Field " << i);
    // Shipping address:
    if (0 <= i && i <= 4) {
      EXPECT_EQ(form_control_datas[i]->autofill_section_id(), 0u);
      ASSERT_EQ(form_control_datas[i]->coarse_autofill_field_type_size(), 1);
      EXPECT_EQ(form_control_datas[i]->coarse_autofill_field_type(0),
                optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS);
    }
    // Billing address:
    if (5 <= i && i <= 9) {
      EXPECT_EQ(form_control_datas[i]->autofill_section_id(), 1u);
      ASSERT_EQ(form_control_datas[i]->coarse_autofill_field_type_size(), 1);
      EXPECT_EQ(form_control_datas[i]->coarse_autofill_field_type(0),
                optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_ADDRESS);
    }
    // Credit card:
    if (10 <= i && i <= 14) {
      EXPECT_EQ(form_control_datas[i]->autofill_section_id(), 2u);
      ASSERT_EQ(form_control_datas[i]->coarse_autofill_field_type_size(), 1);
      EXPECT_EQ(
          form_control_datas[i]->coarse_autofill_field_type(0),
          optimization_guide::proto::COARSE_AUTOFILL_FIELD_TYPE_CREDIT_CARD);
    }
    // A bunch of buttons.
    if (15 <= i && i <= 16) {
      EXPECT_FALSE(form_control_datas[i]->has_autofill_section_id());
      EXPECT_EQ(form_control_datas[i]->coarse_autofill_field_type_size(), 0);
    }
  }
}

// Verifies that availability of autofill data is propagated in the
// `autofill_information` attribute in `AnnotatedPageContents`.
IN_PROC_BROWSER_TEST_F(AutofillAnnotationsProviderBrowserTest,
                       AutofillAvailabilityInformation) {
  client()->GetPersonalDataManager().address_data_manager().AddProfile(
      AutofillProfile(AutofillProfile::RecordType::kAccount,
                      AddressCountryCode("ES")));
  CreditCard card;
  test::SetCreditCardInfo(&card, "Elvis Presley", "4111111111111111", "12",
                          test::NextYear().c_str(), "123");
  client()->GetPersonalDataManager().payments_data_manager().AddCreditCard(
      card);
  LoadPage(https_server()->GetURL("/address_sections_and_creditcard.html"),
           /*num_expected_forms=*/1);
  ASSERT_TRUE(page_content().has_profile_information());
  ASSERT_TRUE(page_content().profile_information().has_autofill_information());
  const auto& autofill_info =
      page_content().profile_information().autofill_information();
  EXPECT_THAT(
      autofill_info.fillable_data(),
      testing::UnorderedElementsAre(
          optimization_guide::proto::AutofillInformation_FillableData_ADDRESS,
          optimization_guide::proto::
              AutofillInformation_FillableData_CREDIT_CARD));
}

class AutofillAnnotationsProviderRedactionBrowserTest
    : public AutofillAnnotationsProviderBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  AutofillAnnotationsProviderRedactionBrowserTest() {
    if (ShouldRedact()) {
      feature_list_.InitAndEnableFeature(
          optimization_guide::features::
              kAnnotatedPageContentAutofillCreditCardRedactions);
    } else {
      feature_list_.InitAndDisableFeature(
          optimization_guide::features::
              kAnnotatedPageContentAutofillCreditCardRedactions);
    }
  }

  bool ShouldRedact() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that sensitive fields are correctly redacted by Autofill.
IN_PROC_BROWSER_TEST_P(AutofillAnnotationsProviderRedactionBrowserTest,
                       Redaction) {
  LoadPage(
      https_server()->GetURL("/address_sections_and_creditcard_filled.html"),
      /*num_expected_forms=*/1);

  std::vector<const optimization_guide::proto::FormControlData*>
      form_control_datas = GetFormControlDatas(page_content());

  // Find the fields and check their redaction status.
  const optimization_guide::proto::FormControlData* name_full_field = nullptr;
  const optimization_guide::proto::FormControlData* address_field = nullptr;
  const optimization_guide::proto::FormControlData* phone_field = nullptr;
  const optimization_guide::proto::FormControlData* cc_name_field = nullptr;
  const optimization_guide::proto::FormControlData* cc_number_field = nullptr;
  const optimization_guide::proto::FormControlData* cc_exp_field = nullptr;

  for (const auto* field : form_control_datas) {
    if (field->field_name() == "shipping-name") {
      name_full_field = field;
    } else if (field->field_name() == "shipping-address") {
      address_field = field;
    } else if (field->field_name() == "shipping-phone") {
      phone_field = field;
    } else if (field->field_name() == "cc-name") {
      cc_name_field = field;
    } else if (field->field_name() == "cc-number") {
      cc_number_field = field;
    } else if (field->field_name() == "cc-exp") {
      cc_exp_field = field;
    }
  }

  ASSERT_TRUE(name_full_field);
  EXPECT_EQ(
      name_full_field->redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
  EXPECT_EQ(name_full_field->field_value(), "John Doe");

  ASSERT_TRUE(address_field);
  EXPECT_EQ(
      address_field->redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
  EXPECT_EQ(address_field->field_value(), "123 Main St");

  ASSERT_TRUE(phone_field);
  EXPECT_EQ(
      phone_field->redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
  EXPECT_EQ(phone_field->field_value(), "555-555-5555");

  ASSERT_TRUE(cc_name_field);
  EXPECT_EQ(
      cc_name_field->redaction_decision(),
      optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
  EXPECT_EQ(cc_name_field->field_value(), "Jane Doe");

  ASSERT_TRUE(cc_number_field);
  if (ShouldRedact()) {
    EXPECT_EQ(cc_number_field->redaction_decision(),
              optimization_guide::proto::
                  REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
    EXPECT_EQ(cc_number_field->field_value(), "");
  } else {
    EXPECT_EQ(
        cc_number_field->redaction_decision(),
        optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
    EXPECT_EQ(cc_number_field->field_value(), "4111111111111111");
  }

  ASSERT_TRUE(cc_exp_field);
  if (ShouldRedact()) {
    EXPECT_EQ(cc_exp_field->redaction_decision(),
              optimization_guide::proto::
                  REDACTION_DECISION_REDACTED_IS_SENSITIVE_PAYMENT_FIELD);
    EXPECT_EQ(cc_exp_field->field_value(), "");
  } else {
    EXPECT_EQ(
        cc_exp_field->redaction_decision(),
        optimization_guide::proto::REDACTION_DECISION_NO_REDACTION_NECESSARY);
    EXPECT_EQ(cc_exp_field->field_value(), "12/2030");
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillAnnotationsProviderRedactionBrowserTest,
                         testing::Bool());

}  // namespace autofill
