// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_siwg_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_autofill_agent.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_metrics_helper.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_permission_service.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_quality_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

namespace actor_login {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::StrictMock;
using ::testing::WithArg;

class MockActionSequenceDelegate : public ActionSequenceDelegate {
 public:
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActionSequenceEnded,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, OnFederatedLoginOutcome, (LoginStatusResult), (override));

  base::WeakPtr<ActionSequenceDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockActionSequenceDelegate> weak_ptr_factory_{this};
};

class MockChromeRenderFrame : public chrome::mojom::ChromeRenderFrame {
 public:
  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this, mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>(
                  std::move(handle)));
  }

  MOCK_METHOD(void,
              SetWindowFeatures,
              (blink::mojom::WindowFeaturesPtr),
              (override));
  MOCK_METHOD(void, RequestReloadImageForContextNode, (), (override));
  MOCK_METHOD(void,
              RequestBitmapForContextNode,
              (RequestBitmapForContextNodeCallback),
              (override));
  MOCK_METHOD(void,
              RequestBitmapForContextNodeWithBoundsHint,
              (RequestBitmapForContextNodeWithBoundsHintCallback),
              (override));
  MOCK_METHOD(void,
              RequestBoundsHintForAllImages,
              (RequestBoundsHintForAllImagesCallback),
              (override));
  MOCK_METHOD(void,
              RequestImageForContextNode,
              (int32_t,
               const gfx::Size&,
               chrome::mojom::ImageFormat,
               int32_t,
               RequestImageForContextNodeCallback),
              (override));
  MOCK_METHOD(void,
              ExecuteWebUIJavaScript,
              (const std::u16string&),
              (override));
  MOCK_METHOD(void, GetMediaFeedURL, (GetMediaFeedURLCallback), (override));
  MOCK_METHOD(void, LoadBlockedPlugins, (const std::string&), (override));
  MOCK_METHOD(void, SetShouldDeferMediaLoad, (bool), (override));
  MOCK_METHOD(void,
              InitializeTool,
              (actor::mojom::ToolInvocationPtr, InitializeToolCallback),
              (override));
  MOCK_METHOD(void,
              ExecuteTool,
              (const actor::TaskId&, ExecuteToolCallback),
              (override));
  MOCK_METHOD(void,
              InvokeTool,
              (actor::mojom::ToolInvocationPtr, InvokeToolCallback),
              (override));
  MOCK_METHOD(void, CancelTool, (const actor::TaskId&), (override));
  MOCK_METHOD(void,
              StartActorJournal,
              (mojo::PendingAssociatedRemote<actor::mojom::JournalClient>),
              (override));
  MOCK_METHOD(void,
              CreatePageStabilityMonitor,
              (mojo::PendingReceiver<actor::mojom::PageStabilityMonitor>,
               const actor::TaskId&,
               bool),
              (override));
  MOCK_METHOD(void,
              GetCrossDocumentScriptToolResult,
              (GetCrossDocumentScriptToolResultCallback),
              (override));

 private:
  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;
};

}  // namespace

class ActorLoginSiwgControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    feature_list_.InitAndDisableFeature(
        password_manager::features::kActorLoginFederatedClickFromActor);

    // Navigate to a URL so we have a valid last committed URL.
    NavigateAndCommit(GURL("https://example.com/login"));

    mock_autofill_agent_.BindForTesting(web_contents()->GetPrimaryMainFrame());

    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        chrome::mojom::ChromeRenderFrame::Name_,
        base::BindRepeating(&MockChromeRenderFrame::BindPendingReceiver,
                            base::Unretained(&mock_chrome_render_frame_)));
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
  autofill::TestAutofillDriverInjector<autofill::ContentAutofillDriver>
      autofill_driver_injector_;
  StrictMock<autofill::MockAutofillAgent> mock_autofill_agent_;
  StrictMock<MockChromeRenderFrame> mock_chrome_render_frame_;
  StrictMock<MockActorLoginPermissionService> mock_permission_service_;

  static void SaveCallback(
      optimization_guide::OnAIPageContentDone* last_callback,
      content::WebContents* web_contents,
      blink::mojom::AIPageContentOptionsPtr options,
      optimization_guide::OnAIPageContentDone callback) {
    *last_callback = std::move(callback);
  }
};

TEST_F(ActorLoginSiwgControllerTest, ButtonFound_ClickSucceeded) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  optimization_guide::OnAIPageContentDone page_content_callback;

  auto metrics_helper_owned =
      std::make_unique<ActorLoginMetricsHelper>(ukm::kInvalidSourceId);

  Credential credential;
  credential.federation_detail = FederationDetail();

  ActorLoginSiwgController controller(
      web_contents(), credential,
      base::BindRepeating(&ActorLoginSiwgControllerTest::SaveCallback,
                          &page_content_callback),
      /*should_store_permission=*/false, mock_permission_service_,
      finished_callback.Get(), /*action_sequence_delegate=*/nullptr,
      /*mqls_logger=*/nullptr,
      /*attempt_login_tool_start_time=*/base::TimeTicks::Now());

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  // 1. Simulate Page Content Received with a SiwG button.
  optimization_guide::proto::AnnotatedPageContent page_content;
  auto* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_common_ancestor_dom_node_id(1);
  root->mutable_content_attributes()
      ->mutable_interaction_info()
      ->add_clickability_reasons(
          optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL);
  auto* child = root->add_children_nodes();
  child->mutable_content_attributes()->set_common_ancestor_dom_node_id(2);

  // 2. Expect FindPotentialSiwgButtons on the agent.
  EXPECT_CALL(mock_autofill_agent_, FindPotentialSiwgButtons)
      .WillOnce(WithArg<0>(
          [&](autofill::mojom::AutofillAgent::FindPotentialSiwgButtonsCallback
                  callback) {
            std::vector<autofill::mojom::SiwgButtonDataPtr> buttons;
            auto button = autofill::mojom::SiwgButtonData::New();
            button->dom_node_id = 2;  // Matches child node
            button->text = u"Sign in with Google";
            buttons.push_back(std::move(button));
            std::move(callback).Run(std::move(buttons));
          }));

  // 3. Expect InvokeTool (Click) on the frame.
  EXPECT_CALL(mock_chrome_render_frame_, InvokeTool)
      .WillOnce(WithArg<1>(
          [&](chrome::mojom::ChromeRenderFrame::InvokeToolCallback callback) {
            auto result = actor::mojom::ActionResult::New();
            result->code = actor::mojom::ActionResultCode::kOk;
            std::move(callback).Run(std::move(result));

            // Manually trigger the federated login completion callback.
            auto* request = content::webid::FederatedEmbedderLoginRequest::Get(
                web_contents());
            ASSERT_TRUE(request);
            request->OnFederatedResultReceived(
                content::webid::FederatedLoginResult::kSuccess);
          }));

  // 4. Verify Success callback.
  EXPECT_CALL(finished_callback,
              Run(base::test::ValueIs(LoginStatusResult::kSuccessFederated)))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  // Trigger the flow manually from OnPageContentReceived since capturing relies
  // on services we didn't mock.
  optimization_guide::AIPageContentResult result;
  result.proto = std::move(page_content);
  std::move(page_content_callback).Run(std::move(result));

  run_loop.Run();

  histogram_tester.ExpectUniqueSample("Actor.Login.Federated.LoginResult",
                                      ActorLoginFederatedLoginResult::kSuccess,
                                      1);
}

TEST_F(ActorLoginSiwgControllerTest, ButtonFound_ClickFailed) {
  base::RunLoop run_loop;
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  optimization_guide::OnAIPageContentDone page_content_callback;
  Credential credential;
  credential.federation_detail = FederationDetail();

  ActorLoginSiwgController controller(
      web_contents(), credential,
      base::BindRepeating(&ActorLoginSiwgControllerTest::SaveCallback,
                          &page_content_callback),
      /*should_store_permission=*/false, mock_permission_service_,
      finished_callback.Get(), /*action_sequence_delegate=*/nullptr,
      /*mqls_logger=*/nullptr,
      /*attempt_login_tool_start_time=*/base::TimeTicks::Now());

  controller.StartFederatedLogin(/*metrics_helper=*/nullptr);

  // 1. Simulate Page Content Received with a SiwG button.
  optimization_guide::proto::AnnotatedPageContent page_content;
  auto* root = page_content.mutable_root_node();
  root->mutable_content_attributes()->set_common_ancestor_dom_node_id(1);
  root->mutable_content_attributes()
      ->mutable_interaction_info()
      ->add_clickability_reasons(
          optimization_guide::proto::CLICKABILITY_REASON_CLICKABLE_CONTROL);
  auto* child = root->add_children_nodes();
  child->mutable_content_attributes()->set_common_ancestor_dom_node_id(2);

  // 2. Expect FindPotentialSiwgButtons.
  EXPECT_CALL(mock_autofill_agent_, FindPotentialSiwgButtons)
      .WillOnce(WithArg<0>(
          [&](autofill::mojom::AutofillAgent::FindPotentialSiwgButtonsCallback
                  callback) {
            std::vector<autofill::mojom::SiwgButtonDataPtr> buttons;
            auto button = autofill::mojom::SiwgButtonData::New();
            button->dom_node_id = 2;
            button->text = u"Sign in with Google";
            buttons.push_back(std::move(button));
            std::move(callback).Run(std::move(buttons));
          }));

  // 3. Expect InvokeTool and simulate failure.
  EXPECT_CALL(mock_chrome_render_frame_, InvokeTool)
      .WillOnce(WithArg<1>(
          [&](chrome::mojom::ChromeRenderFrame::InvokeToolCallback callback) {
            auto result = actor::mojom::ActionResult::New();
            result->code = actor::mojom::ActionResultCode::kElementDisabled;
            std::move(callback).Run(std::move(result));
          }));

  // 4. Verify Failure callback.
  EXPECT_CALL(finished_callback,
              Run(base::test::ErrorIs(ActorLoginError::kFillingNotAllowed)))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  optimization_guide::AIPageContentResult result;
  result.proto = std::move(page_content);
  std::move(page_content_callback).Run(std::move(result));

  run_loop.Run();
}

TEST_F(ActorLoginSiwgControllerTest, NoButtonsFound) {
  base::RunLoop run_loop;
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  optimization_guide::OnAIPageContentDone page_content_callback;
  Credential credential;
  credential.federation_detail = FederationDetail();

  ActorLoginSiwgController controller(
      web_contents(), credential,
      base::BindRepeating(&ActorLoginSiwgControllerTest::SaveCallback,
                          &page_content_callback),
      /*should_store_permission=*/false, mock_permission_service_,
      finished_callback.Get(), /*action_sequence_delegate=*/nullptr,
      /*mqls_logger=*/nullptr,
      /*attempt_login_tool_start_time=*/base::TimeTicks::Now());

  controller.StartFederatedLogin(/*metrics_helper=*/nullptr);

  // No buttons in the page content.
  optimization_guide::proto::AnnotatedPageContent page_content;

  // Expect FindPotentialSiwgButtons but return empty.
  EXPECT_CALL(mock_autofill_agent_, FindPotentialSiwgButtons)
      .WillOnce(WithArg<0>(
          [&](autofill::mojom::AutofillAgent::FindPotentialSiwgButtonsCallback
                  callback) {
            std::vector<autofill::mojom::SiwgButtonDataPtr> buttons;
            std::move(callback).Run(std::move(buttons));
          }));

  // Do NOT expect InvokeTool.
  EXPECT_CALL(mock_chrome_render_frame_, InvokeTool).Times(0);

  EXPECT_CALL(finished_callback,
              Run(base::test::ErrorIs(ActorLoginError::kFillingNotAllowed)))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  optimization_guide::AIPageContentResult result;
  result.proto = std::move(page_content);
  std::move(page_content_callback).Run(std::move(result));

  run_loop.Run();
}

using AttemptLoginDetails =
    optimization_guide::proto::ActorLoginQuality_AttemptLoginDetails;

testing::Matcher<AttemptLoginDetails> EqualsAttemptLoginDetails(
    const AttemptLoginDetails& expected) {
  return testing::AllOf(
      testing::Property("outcome", &AttemptLoginDetails::outcome,
                        expected.outcome()),
      testing::Property("attempt_login_time_ms",
                        &AttemptLoginDetails::attempt_login_time_ms,
                        testing::Ge(expected.attempt_login_time_ms())),
      testing::Property("button_click_required",
                        &AttemptLoginDetails::button_click_required,
                        expected.button_click_required()),
      testing::Property("button_click_succeeded",
                        &AttemptLoginDetails::button_click_succeeded,
                        expected.button_click_succeeded()));
}

class ActorLoginSiwgControllerDelegateClickTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ActorLoginSiwgControllerDelegateClickTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    feature_list_.InitAndEnableFeature(
        password_manager::features::kActorLoginFederatedClickFromActor);

    // Navigate to a URL so we have a valid last committed URL.
    NavigateAndCommit(GURL("https://example.com/login"));
  }

  base::WeakPtr<MockActorLoginQualityLogger> mqls_logger() {
    return mock_mqls_logger_.AsWeakPtr();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  StrictMock<MockActorLoginPermissionService> mock_permission_service_;
  MockActorLoginQualityLogger mock_mqls_logger_;
};

TEST_F(ActorLoginSiwgControllerDelegateClickTest, DelegatesClick) {
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  StrictMock<MockActionSequenceDelegate> action_sequence_delegate;

  auto metrics_helper_owned =
      std::make_unique<ActorLoginMetricsHelper>(ukm::kInvalidSourceId);

  Credential credential;
  credential.federation_detail = FederationDetail();

  auto controller = std::make_unique<ActorLoginSiwgController>(
      web_contents(), credential, false, mock_permission_service_,
      finished_callback.Get(), action_sequence_delegate.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now());
  base::RunLoop start_run_loop;
  EXPECT_CALL(finished_callback,
              Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)))
      .WillOnce(base::test::RunClosure(start_run_loop.QuitClosure()));

  const int kAttemptLoginTimeMs = 50;
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_SUCCESS);
  expected_details.set_attempt_login_time_ms(kAttemptLoginTimeMs);
  expected_details.set_button_click_required(true);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));

  controller->StartFederatedLogin(std::move(metrics_helper_owned));

  start_run_loop.Run();

  // The attempt by the controller is complete, but it is not destroyed until
  // the action sequence is complete.

  // The result still needs to be reported.
  base::RunLoop outcome_run_loop;
  EXPECT_CALL(action_sequence_delegate,
              OnFederatedLoginOutcome(LoginStatusResult::kSuccessFederated))
      .WillOnce(base::test::RunClosure(outcome_run_loop.QuitClosure()));

  task_environment()->AdvanceClock(base::Milliseconds(kAttemptLoginTimeMs));

  // Manually trigger the federated login completion callback.
  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kSuccess);

  outcome_run_loop.Run();

  // Simulate the action sequence ending, at which point the delegate would
  // destroy its SIWG controller.
  controller.reset();

  histogram_tester.ExpectUniqueSample("Actor.Login.Federated.LoginResult",
                                      ActorLoginFederatedLoginResult::kSuccess,
                                      1);
}

TEST_F(ActorLoginSiwgControllerDelegateClickTest, StoresPermissionOnSuccess) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  StrictMock<MockActionSequenceDelegate> action_sequence_delegate;
  auto metrics_helper_owned =
      std::make_unique<ActorLoginMetricsHelper>(ukm::kInvalidSourceId);

  Credential credential;
  credential.username = u"test@gmail.com";
  credential.request_origin = url::Origin::Create(GURL("https://example.com"));
  FederationDetail fed_detail;
  fed_detail.idp_origin =
      url::Origin::Create(GURL("https://accounts.google.com"));
  fed_detail.account_id = "12345";
  credential.federation_detail = fed_detail;

  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), action_sequence_delegate.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now());

  const int kAttemptLoginTimeMs = 50;
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_SUCCESS);
  expected_details.set_attempt_login_time_ms(kAttemptLoginTimeMs);
  expected_details.set_button_click_required(true);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));

  base::RunLoop login_run_loop;
  EXPECT_CALL(finished_callback,
              Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)))
      .WillOnce(base::test::RunClosure(login_run_loop.QuitClosure()));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  login_run_loop.Run();

  FederatedPermission expected_permission;
  expected_permission.idp_origin = fed_detail.idp_origin;
  expected_permission.rp_embedder_origin =
      url::Origin::Create(GURL("https://example.com"));
  expected_permission.rp_requester_origin =
      url::Origin::Create(GURL("https://example.com"));
  expected_permission.chosen_account_id = "12345";
  expected_permission.chosen_account_email = "test@gmail.com";
  EXPECT_CALL(mock_permission_service_,
              GrantPermission(testing::Eq(expected_permission), _));

  base::RunLoop outcome_run_loop;
  EXPECT_CALL(action_sequence_delegate,
              OnFederatedLoginOutcome(LoginStatusResult::kSuccessFederated))
      .WillOnce(base::test::RunClosure(outcome_run_loop.QuitClosure()));

  task_environment()->AdvanceClock(base::Milliseconds(kAttemptLoginTimeMs));

  // Manually trigger the federated login completion callback.
  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kSuccess);

  outcome_run_loop.Run();
}

TEST_F(ActorLoginSiwgControllerDelegateClickTest,
       DoesNotStorePermissionOnFailure) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  StrictMock<MockActionSequenceDelegate> action_sequence_delegate;
  auto metrics_helper_owned =
      std::make_unique<ActorLoginMetricsHelper>(ukm::kInvalidSourceId);

  Credential credential;
  credential.federation_detail = FederationDetail();

  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), action_sequence_delegate.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now());

  const int kAttemptLoginTimeMs = 50;
  AttemptLoginDetails expected_details;
  expected_details.set_outcome(
      optimization_guide::proto::
          ActorLoginQuality_AttemptLoginDetails_AttemptLoginOutcome_FEDERATED_IDP_RETURNED_ERROR);
  expected_details.set_attempt_login_time_ms(kAttemptLoginTimeMs);
  expected_details.set_button_click_required(true);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));

  base::RunLoop start_run_loop;
  EXPECT_CALL(finished_callback,
              Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)))
      .WillOnce(base::test::RunClosure(start_run_loop.QuitClosure()));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  start_run_loop.Run();

  base::RunLoop outcome_run_loop;
  EXPECT_CALL(action_sequence_delegate,
              OnFederatedLoginOutcome(
                  LoginStatusResult::kErrorFederatedIdpReturnedError))
      .WillOnce(base::test::RunClosure(outcome_run_loop.QuitClosure()));
  EXPECT_CALL(mock_permission_service_, GrantPermission).Times(0);

  task_environment()->AdvanceClock(base::Milliseconds(kAttemptLoginTimeMs));

  // Manually trigger the federated login completion callback with FAILURE.
  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kIdpReturnedError);

  outcome_run_loop.Run();
}

}  // namespace actor_login
