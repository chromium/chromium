// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl_test_api.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/forms_annotations.pb.h"
#include "components/optimization_guide/proto/features/forms_predictions.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/interaction/interactive_views_test.h"
#include "ui/views/interaction/view_focus_observer.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill_prediction_improvements {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabId);

const WebContentsInteractionTestUtil::DeepQuery kNameFieldQuery =
    WebContentsInteractionTestUtil::DeepQuery({"input#name"});
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
    switch (expected_response_type_) {
      case ResponseType::kFormsAnnotations: {
        std::string serialized_response;
        optimization_guide::proto::ExecuteResponse execute_response =
            BuildFormsAnnotationsResponse();
        execute_response.SerializeToString(&serialized_response);
        response->set_code(net::HTTP_OK);
        response->set_content(serialized_response);
      } break;
      case ResponseType::kFormsPredictions: {
        std::string serialized_response;
        optimization_guide::proto::ExecuteResponse execute_response =
            BuildFormsPredictionsResponse();
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
  BuildFormsAnnotationsResponse() {
    optimization_guide::proto::FormsAnnotationsResponse response;
    optimization_guide::proto::UserAnnotationsEntry* entry =
        response.add_upserted_entries();
    entry->set_entry_id(1);
    entry->set_key("Name");
    entry->set_value("Jane");
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
  BuildFormsPredictionsResponse() {
    optimization_guide::proto::FormsPredictionsResponse response;
    optimization_guide::proto::FilledFormFieldData* filled_field =
        response.mutable_form_data()->add_filled_form_field_data();
    optimization_guide::proto::PredictedValue* predicted_value =
        filled_field->add_predicted_values();
    predicted_value->set_value("Jane");
    filled_field->set_normalized_label("Name");
    optimization_guide::proto::FormFieldData* filled_field_data =
        filled_field->mutable_field_data();
    filled_field_data->set_field_label("Name");
    filled_field_data->set_field_value("Jane");
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
class AutofillPredictionImprovementsBrowserBaseTest
    : public InteractiveBrowserTest {
 public:
  AutofillPredictionImprovementsBrowserBaseTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{user_annotations::kUserAnnotations,
                               {{"allowed_hosts_for_form_submissions",
                                 "a.com"}}},
                              {kAutofillPredictionImprovements,
                               {{"skip_allowlist", "true"}}},
                              {autofill::features::
                                   kAutofillEnableImprovedPredictionParser,
                               {}}},
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
                &AutofillPredictionImprovementsBrowserBaseTest::
                    OnWillCreateBrowserContextServices,
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

  autofill::AutofillPredictionImprovementsDelegate*
  GetAutofillPredictionImprovementsDelegate() {
    return autofill_client()
               ? autofill_client()->GetAutofillPredictionImprovementsDelegate()
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
#define MAYBE_AutofillPredictionImprovementsBrowserTest \
  DISABLED_AutofillPredictionImprovementsBrowserTest
#else
#define MAYBE_AutofillPredictionImprovementsBrowserTest \
  AutofillPredictionImprovementsBrowserTest
#endif
// Test fixture defining the steps taken in the browser tests. Steps are ordered
// by first occurrence in the tests below. Uses the Kombucha API, see
// https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/test/interaction/README.md
// for documentation.
class MAYBE_AutofillPredictionImprovementsBrowserTest
    : public AutofillPredictionImprovementsBrowserBaseTest {
 protected:
  MultiStep Initialize() {
    return NamedSteps("Initialize", EnableAutofillPredictionImprovementsPref(),
                      InstrumentTab(kPrimaryTabId),
                      WaitForWebContentsReady(kPrimaryTabId),
                      CheckPredictionImprovementsDelegateExists(),
                      CheckUserAnnotationsServiceExists(), RemoveAllEntries(),
                      CheckHasNoEntries());
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
        Log("Start waiting for NameFieldIsFilledStateChange()."),
        WaitForStateChange(kPrimaryTabId, NameFieldIsFilledStateChange()),
        Log("Stopped waiting for NameFieldIsFilledStateChange()."),
        SetExpectedResponseType(ResponseType::kFormsAnnotations), SubmitForm(),
        WaitForWebContentsNavigation(kPrimaryTabId, form_page_url()),
        EnsurePresent(kPrimaryTabId, kNameFieldQuery), CheckNameFieldIsEmpty());
  }

  MultiStep WaitForSaveBubbleAndAccept() {
    return NamedSteps("WaitForSaveBubbleAndAccept",
                      WaitForShow(views::DialogClientView::kOkButtonElementId),
                      PressButton(views::DialogClientView::kOkButtonElementId));
  }

  MultiStep WaitForFormImportedToUserAnnotations() {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kFormWasImportedToUserAnnotationsState);
    return NamedSteps(
        "WaitForFormImportedToUserAnnotations",
        Log("Start polling kFormWasImportedToUserAnnotationsState."),
        PollState(kFormWasImportedToUserAnnotationsState,
                  [&]() { return !GetAllEntries().empty(); }),
        WaitForState(kFormWasImportedToUserAnnotationsState, true),
        StopObservingState(kFormWasImportedToUserAnnotationsState),
        Log("Stopped polling kFormWasImportedToUserAnnotationsState."));
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

  MultiStep WaitForNameFieldToBeFilled() {
    return NamedSteps(
        "WaitForNameFieldToBeFilled",
        WaitForStateChange(kPrimaryTabId, NameFieldIsFilledStateChange()));
  }

 private:
  MultiStep EnableAutofillPredictionImprovementsPref() {
    DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                        kPrefWasEnabledState);
    return NamedSteps(
        "EnableAutofillPredictionImprovementsPref",
        Log("Start polling kPrefWasEnabledState."), Do([&]() {
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

  InteractiveTestApi::StepBuilder CheckPredictionImprovementsDelegateExists() {
    return Check(
        [&]() -> bool { return GetAutofillPredictionImprovementsDelegate(); },
        "Tab has non-null AutofillPredictionImprovementsDelegate");
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

  InteractiveTestApi::StepBuilder ManuallyFillForm() {
    return ExecuteJs(kPrimaryTabId,
                     R"js(
                () => {
                  const name_field = document.getElementById("name");
                  name_field.value = "Jane";
                }
              )js");
  }

  InteractiveBrowserTestApi::StateChange NameFieldIsFilledStateChange() {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kNameFieldHasValue);
    InteractiveBrowserTestApi::StateChange state_change;
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kNameFieldHasValue;
    state_change.test_function = R"js(
                () => {
                  const name_field = document.getElementById("name");
                  return name_field.value === "Jane";
                }
              )js";
    return state_change;
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

  InteractiveTestApi::StepBuilder CheckNameFieldIsEmpty() {
    return CheckJsResultAt(kPrimaryTabId, kNameFieldQuery,
                           R"js((el) => el.value === "")js");
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
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillPredictionImprovementsBrowserTest,
                       ImportAndFillFormSuccessful) {
  EnableSignin();
  RunTestSequence(
      Initialize(), NavigateToFormPage(), ManuallyFillAndSubmitForm(),
      WaitForSaveBubbleAndAccept(), WaitForFormImportedToUserAnnotations(),
      NavigateToAboutBlank(), NavigateToFormPage(), ClickOnNameField(),
      WaitForAndAcceptSuggestion(
          kAutofillPredictionImprovementsTriggerElementId),
      WaitForAndAcceptSuggestion(kAutofillPredictionImprovementsFillElementId),
      WaitForNameFieldToBeFilled());
}

// Tests that the error suggestion is shown if filling suggestions cannot be
// retrieved.
IN_PROC_BROWSER_TEST_F(MAYBE_AutofillPredictionImprovementsBrowserTest,
                       ImportAndFillFormFailsAtRetrievingFillingSuggestions) {
  EnableSignin();
  RunTestSequence(
      Initialize(), NavigateToFormPage(), ManuallyFillAndSubmitForm(),
      WaitForSaveBubbleAndAccept(), WaitForFormImportedToUserAnnotations(),
      NavigateToAboutBlank(), NavigateToFormPage(), ClickOnNameField(),
      WaitForAndAcceptSuggestion(
          kAutofillPredictionImprovementsTriggerElementId,
          ResponseType::kServerError),
      WaitForShow(kAutofillPredictionImprovementsErrorElementId));
}

}  // namespace

}  // namespace autofill_prediction_improvements
