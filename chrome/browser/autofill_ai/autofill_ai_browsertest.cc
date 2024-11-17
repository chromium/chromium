// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autofill_ai/chrome_autofill_ai_client.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_ai/core/browser/autofill_ai_features.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/forms_annotations.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/user_annotations/user_annotations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill_ai {

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using DeepQuery = WebContentsInteractionTestUtil::DeepQuery;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);

// Comparator for `DeepQuery` that uses the first sequence entry as comparison.
struct DeepQueryComparator {
  bool operator()(const DeepQuery& left, const DeepQuery& right) const {
    return left[0] < right[0];
  }
};

// Struct declaring test data.
struct TestField {
  std::string label;
  std::string manually_entered_value;
  int64_t expected_entry_id;
  std::string expected_value_after_form_submission;
  std::string expected_value_for_import_and_automatic_filling;
};

// Matches optimization_guide::proto::UserAnnotationsEntry `arg` against
// `TestField` `expected_test_field`.
MATCHER_P(MatchesTestField, expected_test_field, "") {
  return arg.key() == expected_test_field.label &&
         arg.value() == expected_test_field
                            .expected_value_for_import_and_automatic_filling;
}

// Function template to access a member of `TestField`.
template <typename T>
T get_field(const TestField& test_field, T TestField::* member_ptr) {
  return test_field.*member_ptr;
}

// CSS selectors of fields to filled and/or checked for their value.
const DeepQuery kNameFieldQuery = DeepQuery({"input#name"});
const DeepQuery kStreetAddressFieldQuery = DeepQuery({"input#street-address"});
const DeepQuery kPostalCodeFieldQuery = DeepQuery({"input#postal-code"});
const DeepQuery kCreditCardNumberFieldQuery = DeepQuery({"input#cc-number"});

const auto kTestFieldsByQuery =
    base::flat_map<DeepQuery, TestField, DeepQueryComparator>(
        {{DeepQuery({kNameFieldQuery}),
          {.label = "Name",
           .manually_entered_value = "Jane",
           .expected_entry_id = 1,
           .expected_value_for_import_and_automatic_filling = "Jane"}},
         {DeepQuery({kStreetAddressFieldQuery}),
          {.label = "Address",
           .manually_entered_value = "100 Boland Way",
           .expected_entry_id = 2,
           .expected_value_for_import_and_automatic_filling =
               "100 Boland Way"}},
         {DeepQuery({kPostalCodeFieldQuery}),
          {.label = "Postal code",
           // The value is left empty intentionally.
           .manually_entered_value = "",
           .expected_entry_id = 3,
           .expected_value_for_import_and_automatic_filling = ""}},
         {DeepQuery({kCreditCardNumberFieldQuery}),
          {.label = "Credit card number",
           .manually_entered_value = "1234 5678 9012 3456",
           .expected_entry_id = 4,
           .expected_value_for_import_and_automatic_filling = ""}}});

const ui::Accelerator kKeyDown =
    ui::Accelerator(ui::VKEY_DOWN,
                    /*modifiers=*/0,
                    ui::Accelerator::KeyState::RELEASED);
const ui::Accelerator kKeyReturn =
    ui::Accelerator(ui::VKEY_RETURN,
                    /*modifiers=*/0,
                    ui::Accelerator::KeyState::RELEASED);

// Class for faking server responses.
class OptimizationGuideTestServer {
 public:
  enum class ResponseType : uint8_t {
    kServerError = 0,
    kFormsAnnotations = 1,
    kFormsPredictions = 2
  };

  bool InitAndStart() {
    optimization_guide_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {
        GURL(optimization_guide::
                 kOptimizationGuideServiceModelExecutionDefaultURL)
            .host()};
    optimization_guide_server_->SetSSLConfig(cert_config);
    optimization_guide_server_->RegisterRequestHandler(base::BindRepeating(
        &OptimizationGuideTestServer::HandleOptimizationGuideRequest,
        base::Unretained(this)));
    return optimization_guide_server_->Start();
  }

  void SetUpCommandLine(base::CommandLine* cmd) {
    cmd->AppendSwitchASCII(
        optimization_guide::switches::
            kOptimizationGuideServiceModelExecutionURL,
        optimization_guide_server_
            ->GetURL(GURL(optimization_guide::
                              kOptimizationGuideServiceModelExecutionDefaultURL)
                         .host(),
                     "/")
            .spec());
  }

  bool ShutdownAndWaitUntilComplete() {
    return optimization_guide_server_->ShutdownAndWaitUntilComplete();
  }

  void SetExpectedResponseType(ResponseType expected_response_type) {
    expected_response_type_ = expected_response_type;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse>
  HandleOptimizationGuideRequest(const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    optimization_guide::proto::ExecuteRequest execute_request;
    if (!execute_request.ParseFromString(request.content)) {
      response->set_content(
          "Couldn't parse net::test_server::HttpRequest::content as "
          "optimization_guide::proto::ExecuteRequest.");
      response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
      return response;
    }
    switch (expected_response_type_) {
      case ResponseType::kFormsAnnotations: {
        optimization_guide::proto::FormsAnnotationsRequest
            forms_annotations_request;
        if (!forms_annotations_request.ParseFromString(
                execute_request.request_metadata().value())) {
          response->set_content(
              "Couldn't parse optimization_guide::proto::ExecuteRequest's "
              "request_metadata().value() field as "
              "optimization_guide::proto::FormsAnnotationsRequest.");
          response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
          return response;
        }
        std::string serialized_response;
        optimization_guide::proto::ExecuteResponse execute_response =
            BuildFormsAnnotationsResponse(forms_annotations_request);
        execute_response.SerializeToString(&serialized_response);
        response->set_code(net::HTTP_OK);
        response->set_content(serialized_response);
        break;
      }
      case ResponseType::kFormsPredictions: {
        optimization_guide::proto::FormsPredictionsRequest
            forms_predictions_request;
        if (!forms_predictions_request.ParseFromString(
                execute_request.request_metadata().value())) {
          response->set_content(
              "Couldn't parse optimization_guide::proto::ExecuteRequest's "
              "request_metadata().value() field as "
              "optimization_guide::proto::FormsPredictionsRequest.");
          response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
          return response;
        }
        std::string serialized_response;
        optimization_guide::proto::ExecuteResponse execute_response =
            BuildFormsPredictionsResponse(forms_predictions_request);
        execute_response.SerializeToString(&serialized_response);
        response->set_code(net::HTTP_OK);
        response->set_content(serialized_response);
        break;
      }
      default:
        response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    }
    return response;
  }

  static optimization_guide::proto::ExecuteResponse
  BuildFormsAnnotationsResponse(
      const optimization_guide::proto::FormsAnnotationsRequest& request) {
    optimization_guide::proto::FormsAnnotationsResponse response;
    for (const optimization_guide::proto::FormFieldData& field :
         request.form_data().fields()) {
      if (field.field_value().empty()) {
        continue;
      }
      optimization_guide::proto::UserAnnotationsEntry* entry =
          response.add_upserted_entries();
      entry->set_key(field.field_label());
      entry->set_value(field.field_value());
    }
    optimization_guide::proto::ExecuteResponse execute_response;
    optimization_guide::proto::Any* any_metadata =
        execute_response.mutable_response_metadata();
    any_metadata->set_type_url("type.googleapis.com/" + response.GetTypeName());
    response.SerializeToString(any_metadata->mutable_value());
    auto response_data = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::FormsAnnotationsResponse>(*any_metadata);
    EXPECT_TRUE(response_data);
    return execute_response;
  }

  static optimization_guide::proto::ExecuteResponse
  BuildFormsPredictionsResponse(
      const optimization_guide::proto::FormsPredictionsRequest& request) {
    optimization_guide::proto::FormsPredictionsResponse response;
    for (int i = 0; i < request.form_data().fields().size(); ++i) {
      const optimization_guide::proto::FormFieldData& field =
          request.form_data().fields(i);
      auto it = base::ranges::find(
          request.entries(), field.field_label(),
          &optimization_guide::proto::UserAnnotationsEntry::key);
      if (it == request.entries().end()) {
        continue;
      }
      const std::string& field_label = it->key();
      const std::string& field_value = it->value();
      optimization_guide::proto::FilledFormFieldData* filled_field =
          response.mutable_form_data()->add_filled_form_field_data();
      optimization_guide::proto::PredictedValue* predicted_value =
          filled_field->add_predicted_values();
      predicted_value->set_value(field_value);
      filled_field->set_normalized_label(field_label);
      filled_field->set_request_field_index(i);
      optimization_guide::proto::FormFieldData* filled_field_data =
          filled_field->mutable_field_data();
      filled_field_data->set_field_label(field_label);
      filled_field_data->set_field_value(field_value);
    }
    optimization_guide::proto::ExecuteResponse execute_response;
    optimization_guide::proto::Any* any_metadata =
        execute_response.mutable_response_metadata();
    any_metadata->set_type_url("type.googleapis.com/" + response.GetTypeName());
    response.SerializeToString(any_metadata->mutable_value());
    auto response_data = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::FormsPredictionsResponse>(*any_metadata);
    EXPECT_TRUE(response_data);
    return execute_response;
  }

  ResponseType expected_response_type_ = ResponseType::kServerError;
  std::unique_ptr<net::EmbeddedTestServer> optimization_guide_server_;
};

using ResponseType = OptimizationGuideTestServer::ResponseType;

// Base class for setting up the browser test.
class AutofillAiBrowserBaseTest : public InteractiveBrowserTest {
 public:
  AutofillAiBrowserBaseTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{kAutofillAi,
                               {{"skip_allowlist", "true"},
                                {"allowed_hosts_for_form_submissions",
                                 "a.com"}}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/autofill_prediction_improvements");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(optimization_guide_test_server_.InitAndStart());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Started());
    form_page_url_ = embedded_test_server()->GetURL(
        "a.com", "/autofill_prediction_improvements_form.html");
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &AutofillAiBrowserBaseTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    optimization_guide_test_server_.SetUpCommandLine(cmd);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(optimization_guide_test_server_.ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  autofill::ChromeAutofillClient* autofill_client() {
    return autofill::ChromeAutofillClient::FromWebContentsForTesting(
        web_contents());
  }

  autofill::AutofillAiDelegate* GetAutofillAiDelegate() {
    return autofill_client() ? autofill_client()->GetAutofillAiDelegate()
                             : nullptr;
  }

  user_annotations::UserAnnotationsService* GetUserAnnotationsService() {
    return UserAnnotationsServiceFactory::GetForProfile(browser()->profile());
  }

  user_annotations::UserAnnotationsEntries GetAllEntries() {
    base::test::TestFuture<user_annotations::UserAnnotationsEntries> future;
    GetUserAnnotationsService()->RetrieveAllEntries(future.GetCallback());
    return future.Take();
  }

  void EnableSignin() {
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@gmail.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  OptimizationGuideTestServer& optimization_guide_test_server() {
    return optimization_guide_test_server_;
  }

  const GURL& form_page_url() const { return form_page_url_; }

  template <typename... Args>
  InteractiveTestApi::MultiStep NamedSteps(std::string_view name,
                                           Args&&... args) {
    MultiStep result;
    (AddStep(result, std::forward<Args>(args)), ...);
    AddDescription(result, base::StrCat({name, "( %s )"}));
    return result;
  }

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList feature_list_;
  GURL form_page_url_;
  OptimizationGuideTestServer optimization_guide_test_server_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

// Test is disabled on Windows because "`MoveMouseTo()` on Windows may result in
// Windows entering a drag loop that may hang or otherwise impact the test."
// Source:
// https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/test/interaction/README.md#known-issues-and-incompatibilities
#if BUILDFLAG(IS_WIN)
#define MAYBE_AutofillAiBrowserTest DISABLED_AutofillAiBrowserTest
#else
#define MAYBE_AutofillAiBrowserTest AutofillAiBrowserTest
#endif
// Test fixture defining the steps taken in the browser tests. Steps are ordered
// by first occurrence in the tests below. Uses the Kombucha API, see
// https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/test/interaction/README.md
// for documentation.
class MAYBE_AutofillAiBrowserTest : public AutofillAiBrowserBaseTest {
 protected:
  MultiStep Initialize() {
    return NamedSteps(
        "Initialize", EnableAutofillAiPref(), InstrumentTab(kPrimaryTabId),
        WaitForWebContentsReady(kPrimaryTabId), CheckAutofillAiDelegateExists(),
        CheckUserAnnotationsServiceExists());
  }

  MultiStep NavigateToFormPage() {
    return NamedSteps("NavigateToFormPage",
                      NavigateWebContents(kPrimaryTabId, form_page_url()),
                      WaitForWebContentsReady(kPrimaryTabId, form_page_url()),
                      EnsurePresent(kPrimaryTabId, kNameFieldQuery));
  }

  MultiStep ManuallyFillAndSubmitForm() {
    return NamedSteps(
        "ManuallyFillAndSubmitForm", ManuallyFillForm(),
        WaitForFieldsToBeFilledManually(),
        SetExpectedResponseType(ResponseType::kFormsAnnotations), SubmitForm(),
        WaitForWebContentsNavigation(kPrimaryTabId, form_page_url()),
        EnsurePresent(kPrimaryTabId, kNameFieldQuery),
        CheckFieldValuesAfterFormSubmission());
  }

  MultiStep WaitForSaveBubbleAndAccept() {
    return NamedSteps("WaitForSaveBubbleAndAccept",
                      WaitForShow(views::DialogClientView::kOkButtonElementId),
                      PressButton(views::DialogClientView::kOkButtonElementId));
  }

  MultiStep NavigateToAboutBlank() {
    const GURL kAboutBlankUrl = GURL("about:blank");
    return NamedSteps("NavigateToAboutBlank",
                      NavigateWebContents(kPrimaryTabId, kAboutBlankUrl),
                      WaitForWebContentsReady(kPrimaryTabId, kAboutBlankUrl));
  }

  MultiStep ClickOnNameField() {
    return NamedSteps("ClickOnNameField",
                      MoveMouseTo(kPrimaryTabId, kNameFieldQuery),
                      ClickMouse());
  }

  MultiStep WaitForAndAcceptSuggestion(
      ui::ElementIdentifier id,
      ResponseType expected_response_type = ResponseType::kFormsPredictions) {
    return NamedSteps("WaitForSelectAndAcceptSuggestion", WaitForShow(id),
                      DisableAutofillPopupThreshold(),
                      SendAccelerator(kPrimaryTabId, kKeyDown),
                      WaitForCellSelected(id),
                      SetExpectedResponseType(expected_response_type),
                      SendAccelerator(kPrimaryTabId, kKeyReturn));
  }

  MultiStep WaitForFieldsToBeFilledAutomatically() {
    return NamedSteps(
        "WaitForFieldsToBeFilledAutomatically",
        WaitForFieldsToBeFilled(
            &TestField::expected_value_for_import_and_automatic_filling));
  }

  void VerifyDataImportedIntoUserAnnotations() {
    EXPECT_THAT(
        GetAllEntries(),
        UnorderedElementsAre(
            MatchesTestField(kTestFieldsByQuery.at(kNameFieldQuery)),
            MatchesTestField(kTestFieldsByQuery.at(kStreetAddressFieldQuery))));
  }

 private:
  MultiStep EnableAutofillAiPref() {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kPrefWasEnabledState);
    return NamedSteps(
        "EnableAutofillAiPref", Log("Start polling kPrefWasEnabledState."),
        Do([&]() {
          browser()->profile()->GetPrefs()->SetBoolean(
              autofill::prefs::kAutofillPredictionImprovementsEnabled, true);
        }),
        PollState(
            kPrefWasEnabledState,
            [&]() {
              return browser()->profile()->GetPrefs()->GetBoolean(
                  autofill::prefs::kAutofillPredictionImprovementsEnabled);
            }),
        WaitForState(kPrefWasEnabledState, true),
        StopObservingState(kPrefWasEnabledState),
        Log("Stopped polling kPrefWasEnabledState."));
  }

  InteractiveTestApi::StepBuilder CheckAutofillAiDelegateExists() {
    return Check([&]() -> bool { return GetAutofillAiDelegate(); },
                 "Tab has non-null AutofillAiDelegate");
  }

  InteractiveTestApi::StepBuilder CheckUserAnnotationsServiceExists() {
    return Check([&]() -> bool { return GetUserAnnotationsService(); },
                 "Tab has non-null UserAnnotationsService");
  }

  InteractiveTestApi::StepBuilder RemoveAllEntries() {
    return Do([&]() {
      base::test::TestFuture<void> future;
      GetUserAnnotationsService()->RemoveAllEntries(future.GetCallback());
      ASSERT_TRUE(future.Wait())
          << "UserAnnotationsService::RemoveAllEntries() failed.";
    });
  }

  InteractiveTestApi::StepBuilder CheckHasNoEntries() {
    return Check([&]() { return GetAllEntries().empty(); },
                 "UserAnnotationsService has no entries.");
  }

  MultiStep ManuallyFillForm() {
    MultiStep steps;
    for (const auto& [deep_query, test_field] : kTestFieldsByQuery) {
      steps.emplace_back(
          ExecuteJsAt(kPrimaryTabId, deep_query,
                      base::StrCat({"(el) => el.value = \"",
                                    test_field.manually_entered_value, "\""})));
    }
    return steps;
  }

  InteractiveBrowserTestApi::StateChange FieldIsManuallyFilledStateChange(
      const DeepQuery& deep_query,
      const std::string& expected_field_value) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kFieldsAreManuallyFilled);
    InteractiveBrowserTestApi::StateChange state_change;
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kFieldsAreManuallyFilled;
    state_change.test_function =
        base::StrCat({"() => document.querySelector(\"", deep_query[0],
                      "\").value === \"", expected_field_value, "\""});
    return state_change;
  }

  template <typename T>
  MultiStep WaitForFieldsToBeFilled(T TestField::* member_ptr) {
    MultiStep steps;
    for (const auto& [deep_query, test_field] : kTestFieldsByQuery) {
      const std::string expected_field_value =
          get_field(test_field, member_ptr);
      steps.emplace_back(Log(
          base::StrCat({"Start waiting for field \"", deep_query[0],
                        "\" to have value \"", expected_field_value, "\"."})));
      for (auto& step : WaitForStateChange(
               kPrimaryTabId, FieldIsManuallyFilledStateChange(
                                  deep_query, expected_field_value))) {
        steps.emplace_back(std::move(step));
      }
      steps.emplace_back(
          Log("Stopped waiting for field \"", deep_query[0], "\"."));
    }
    return steps;
  }

  MultiStep WaitForFieldsToBeFilledManually() {
    return NamedSteps(
        "WaitForFieldsToBeFilledManually()",
        WaitForFieldsToBeFilled(&TestField::manually_entered_value));
  }

  InteractiveTestApi::StepBuilder SetExpectedResponseType(
      ResponseType expected_response_type) {
    return Do([&, expected_response_type]() {
      optimization_guide_test_server().SetExpectedResponseType(
          expected_response_type);
    });
  }

  InteractiveTestApi::StepBuilder SubmitForm() {
    return ExecuteJs(kPrimaryTabId,
                     R"js(
                () => {
                  document.forms[0].submit();
                }
              )js",
                     ExecuteJsMode::kFireAndForget);
  }

  MultiStep CheckFieldValuesAfterFormSubmission() {
    MultiStep steps;
    for (const auto& [deep_query, test_field] : kTestFieldsByQuery) {
      steps.emplace_back(CheckJsResultAt(
          kPrimaryTabId, deep_query,
          base::StrCat({"(el) => el.value ===\"",
                        test_field.expected_value_after_form_submission,
                        "\""})));
    }
    return steps;
  }

  MultiStep DisableAutofillPopupThreshold() {
    return NamedSteps(
        "DisableAutofillPopupThreshold", Do([&]() {
          if (base::WeakPtr<autofill::AutofillSuggestionController> controller =
                  autofill_client()->suggestion_controller_for_testing()) {
            test_api(static_cast<autofill::AutofillPopupControllerImpl&>(
                         *controller))
                .DisableThreshold(/*disable_threshold=*/true);
          }
        }));
  }

  MultiStep WaitForCellSelected(ui::ElementIdentifier id) {
    using SelectedCellObserver = views::test::PollingViewPropertyObserver<
        std::optional<autofill::PopupRowView::CellType>,
        autofill::PopupRowView>;
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(SelectedCellObserver,
                                        kSelectedCellState);
    return NamedSteps(
        "WaitForCellSelected", Log("Start polling kSelectedCellState."),
        PollViewProperty(kSelectedCellState, id,
                         &autofill::PopupRowView::GetSelectedCell),
        WaitForState(kSelectedCellState,
                     autofill::PopupRowView::CellType::kContent),
        StopObservingState(kSelectedCellState),
        Log("Stopped polling kSelectedCellState."));
  }
};

// Tests the import and fill "happy path" end to end.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillAiBrowserTest,
                       ImportAndFillFormSuccessful) {
  EnableSignin();
  RunTestSequence(Initialize(), NavigateToFormPage(),
                  ManuallyFillAndSubmitForm(), WaitForSaveBubbleAndAccept());
  base::RunLoop().RunUntilIdle();
  VerifyDataImportedIntoUserAnnotations();
  RunTestSequence(
      NavigateToAboutBlank(), NavigateToFormPage(), ClickOnNameField(),
      WaitForAndAcceptSuggestion(
          kAutofillPredictionImprovementsTriggerElementId),
      WaitForAndAcceptSuggestion(kAutofillPredictionImprovementsFillElementId),
      WaitForFieldsToBeFilledAutomatically());
}

// Tests that the error suggestion is shown if filling suggestions cannot be
// retrieved.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillAiBrowserTest,
                       ImportAndFillFormFailsAtRetrievingFillingSuggestions) {
  EnableSignin();
  RunTestSequence(Initialize(), NavigateToFormPage(),
                  ManuallyFillAndSubmitForm(), WaitForSaveBubbleAndAccept());
  base::RunLoop().RunUntilIdle();
  VerifyDataImportedIntoUserAnnotations();
  RunTestSequence(NavigateToAboutBlank(), NavigateToFormPage(),
                  ClickOnNameField(),
                  WaitForAndAcceptSuggestion(
                      kAutofillPredictionImprovementsTriggerElementId,
                      ResponseType::kServerError),
                  WaitForShow(kAutofillPredictionImprovementsErrorElementId));
}

}  // namespace

}  // namespace autofill_ai
