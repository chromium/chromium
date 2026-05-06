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
#include "base/test/test_future.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_delegate_impl.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_metrics_helper.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_delegate.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_permission_service.h"
#include "components/password_manager/core/browser/actor_login/test/mock_actor_login_quality_logger.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_embedder_login_request.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
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
              (mojo::PendingReceiver<
                   page_content_annotations::mojom::PageStabilityMonitor>,
               const actor::TaskId&,
               bool),
              (override));
  MOCK_METHOD(void,
              GetCrossDocumentScriptToolResult,
              (const base::UnguessableToken&,
               GetCrossDocumentScriptToolResultCallback),
              (override));

 private:
  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;
};

}  // namespace



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

class ActorLoginSiwgControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ActorLoginSiwgControllerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_sequence_delegate_, RegisterActionSequenceEnded)
        .WillByDefault([](base::OnceCallback<void(bool)> callback) {
          return base::CallbackListSubscription();
        });

    // Navigate to a URL so we have a valid last committed URL.
    NavigateAndCommit(GURL("https://example.com/login"));
  }

  base::WeakPtr<MockActorLoginQualityLogger> mqls_logger() {
    return mock_mqls_logger_.AsWeakPtr();
  }

  void SimulateContinuationLoginResult(content::WebContentsObserver* observer,
                                       bool success) {
    observer->OnFedCmFederatedLogin(success);
  }

  void SimulatePopupOpen(content::WebContentsObserver* observer,
                         content::WebContents* popup_contents) {
    observer->DidOpenRequestedURL(
        popup_contents, /*source_render_frame_host=*/nullptr, GURL(),
        content::Referrer(), WindowOpenDisposition::NEW_POPUP,
        ui::PAGE_TRANSITION_LINK,
        /*started_from_context_menu=*/false, /*renderer_initiated=*/false);
  }

 protected:
  StrictMock<MockActorLoginPermissionService> mock_permission_service_;
  MockActorLoginQualityLogger mock_mqls_logger_;
  testing::NiceMock<MockActionSequenceDelegate> mock_action_sequence_delegate_;
};

TEST_F(ActorLoginSiwgControllerTest, DelegatesClick) {
  base::HistogramTester histogram_tester;
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;

  base::OnceCallback<void(bool)> captured_callback;
  EXPECT_CALL(mock_action_sequence_delegate_, RegisterActionSequenceEnded)
      .WillOnce([&](base::OnceCallback<void(bool)> callback) {
        captured_callback = std::move(callback);
        return base::CallbackListSubscription();
      });

  auto metrics_helper_owned =
      std::make_unique<ActorLoginMetricsHelper>(ukm::kInvalidSourceId);

  Credential credential;
  credential.federation_detail = FederationDetail();

  base::test::TestFuture<bool> post_button_click_login_result_future;
  auto controller = std::make_unique<ActorLoginSiwgController>(
      web_contents(), credential, false, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());
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
  expected_details.set_button_click_succeeded(true);

  EXPECT_CALL(
      mock_mqls_logger_,
      AddAttemptLoginDetails(EqualsAttemptLoginDetails(expected_details)));

  controller->StartFederatedLogin(std::move(metrics_helper_owned));

  start_run_loop.Run();

  // The attempt by the controller is complete, but it is not destroyed until
  // the action sequence is complete.

  // The result still needs to be reported.
  base::RunLoop outcome_run_loop;
  EXPECT_CALL(mock_action_sequence_delegate_,
              OnFederatedLoginOutcome(LoginStatusResult::kSuccessFederated))
      .WillOnce(base::test::RunClosure(outcome_run_loop.QuitClosure()));

  task_environment()->AdvanceClock(base::Milliseconds(kAttemptLoginTimeMs));

  // Simulate the action sequence ending with success.
  std::move(captured_callback).Run(true);

  // Manually trigger the federated login completion callback.
  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);
  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kSuccess);

  outcome_run_loop.Run();

  EXPECT_TRUE(post_button_click_login_result_future.Get());

  // Simulate the action sequence ending, at which point the delegate would
  // destroy its SIWG controller.
  controller.reset();

  histogram_tester.ExpectUniqueSample("Actor.Login.Federated.LoginResult",
                                      ActorLoginFederatedLoginResult::kSuccess,
                                      1);
}

TEST_F(ActorLoginSiwgControllerTest, StoresPermissionOnSuccess) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
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
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(), base::DoNothing());

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
  EXPECT_CALL(mock_action_sequence_delegate_,
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

TEST_F(ActorLoginSiwgControllerTest, DoesNotStorePermissionOnFailure) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  auto metrics_helper_owned =
      std::make_unique<ActorLoginMetricsHelper>(ukm::kInvalidSourceId);
  Credential credential;
  credential.federation_detail = FederationDetail();

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

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
  EXPECT_CALL(mock_action_sequence_delegate_,
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

  EXPECT_FALSE(post_button_click_login_result_future.Get());
}

TEST_F(ActorLoginSiwgControllerTest, Continuation_Success_StoresPermission) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
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

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

  EXPECT_CALL(
      finished_callback,
      Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(
      mock_action_sequence_delegate_,
      OnFederatedLoginOutcome(LoginStatusResult::kErrorFederatedContinuation));

  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);

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

  SimulateContinuationLoginResult(&controller, /*success=*/true);
  EXPECT_TRUE(post_button_click_login_result_future.Get());
}

TEST_F(ActorLoginSiwgControllerTest, Continuation_Failed_NoPermissionStored) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
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

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

  EXPECT_CALL(
      finished_callback,
      Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(
      mock_action_sequence_delegate_,
      OnFederatedLoginOutcome(LoginStatusResult::kErrorFederatedContinuation));

  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);

  EXPECT_CALL(mock_permission_service_, GrantPermission).Times(0);

  SimulateContinuationLoginResult(&controller, /*success=*/false);
  EXPECT_FALSE(post_button_click_login_result_future.Get());
}

TEST_F(ActorLoginSiwgControllerTest,
       Continuation_ShouldStoreFalse_NoPermissionStored) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
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

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/false, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

  EXPECT_CALL(
      finished_callback,
      Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(
      mock_action_sequence_delegate_,
      OnFederatedLoginOutcome(LoginStatusResult::kErrorFederatedContinuation));

  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);

  EXPECT_CALL(mock_permission_service_, GrantPermission).Times(0);

  SimulateContinuationLoginResult(&controller, /*success=*/true);
  EXPECT_TRUE(post_button_click_login_result_future.Get());
}

TEST_F(ActorLoginSiwgControllerTest,
       NotInContinuationFlow_FedCmLogin_NoPermissionStored) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
  StrictMock<MockActionSequenceDelegate> action_sequence_delegate;

  Credential credential;
  credential.username = u"test@gmail.com";
  credential.request_origin = url::Origin::Create(GURL("https://example.com"));
  FederationDetail fed_detail;
  fed_detail.idp_origin =
      url::Origin::Create(GURL("https://accounts.google.com"));
  fed_detail.account_id = "12345";
  credential.federation_detail = fed_detail;

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

  EXPECT_CALL(mock_permission_service_, GrantPermission).Times(0);

  SimulateContinuationLoginResult(&controller, /*success=*/true);
  // RunUntilIdle because there is no signal to wait for when not in a
  // continuation flow.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(post_button_click_login_result_future.IsReady());
}

TEST_F(ActorLoginSiwgControllerTest,
       ContinuationInPopup_PopupDestroyed_NoPermission) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
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

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

  EXPECT_CALL(
      finished_callback,
      Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(
      mock_action_sequence_delegate_,
      OnFederatedLoginOutcome(LoginStatusResult::kErrorFederatedContinuation));

  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);

  // Simulate opening a popup.
  std::unique_ptr<content::WebContents> popup_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr);

  SimulatePopupOpen(&controller, popup_contents.get());

  EXPECT_CALL(mock_permission_service_, GrantPermission).Times(0);

  // Destroy the popup. This should trigger `WebContentsDestroyed` in
  // PopupObserver.
  popup_contents.reset();

  // Wait for the posted task to run.
  EXPECT_FALSE(post_button_click_login_result_future.Get());
}

TEST_F(ActorLoginSiwgControllerTest,
       ContinuationInPopup_Success_StoresPermission) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
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

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

  // Simulate opening a popup.
  std::unique_ptr<content::WebContents> popup_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr);

  SimulatePopupOpen(&controller, popup_contents.get());

  EXPECT_CALL(
      finished_callback,
      Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(
      mock_action_sequence_delegate_,
      OnFederatedLoginOutcome(LoginStatusResult::kErrorFederatedContinuation));

  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);

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

  controller.SimulateContinuationInPopupForTesting(true);
  EXPECT_TRUE(post_button_click_login_result_future.Get());
}

TEST_F(ActorLoginSiwgControllerTest,
       ContinuationInPopup_Failed_NoPermissionStored) {
  base::MockCallback<LoginStatusResultOrErrorReply> finished_callback;
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

  base::test::TestFuture<bool> post_button_click_login_result_future;
  ActorLoginSiwgController controller(
      web_contents(), credential,
      /*should_store_permission=*/true, mock_permission_service_,
      finished_callback.Get(), mock_action_sequence_delegate_.GetWeakPtr(),
      mqls_logger(), base::TimeTicks::Now(),
      post_button_click_login_result_future.GetCallback());

  // Simulate opening a popup.
  std::unique_ptr<content::WebContents> popup_contents =
      content::WebContentsTester::CreateTestWebContents(browser_context(),
                                                        nullptr);

  SimulatePopupOpen(&controller, popup_contents.get());

  EXPECT_CALL(
      finished_callback,
      Run(base::test::ValueIs(LoginStatusResult::kRequiresButtonClick)));

  controller.StartFederatedLogin(std::move(metrics_helper_owned));

  auto* request =
      content::webid::FederatedEmbedderLoginRequest::Get(web_contents());
  ASSERT_TRUE(request);

  EXPECT_CALL(
      mock_action_sequence_delegate_,
      OnFederatedLoginOutcome(LoginStatusResult::kErrorFederatedContinuation));

  request->OnFederatedResultReceived(
      content::webid::FederatedLoginResult::kContinuation);

  EXPECT_CALL(mock_permission_service_, GrantPermission).Times(0);

  controller.SimulateContinuationInPopupForTesting(false);
  EXPECT_FALSE(post_button_click_login_result_future.Get());
}

}  // namespace actor_login
