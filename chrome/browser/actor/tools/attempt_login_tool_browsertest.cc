// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

using actor::webui::mojom::UserGrantedPermissionDuration;
using base::test::TestFuture;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace actor {

namespace {

webui::mojom::SelectCredentialDialogResponsePtr
MakeSelectCredentialDialogResponse(
    TaskId task_id,
    std::optional<actor_login::Credential::Id> selected_credential_id,
    UserGrantedPermissionDuration permission_duration,
    std::optional<webui::mojom::SelectCredentialDialogErrorReason>
        error_reason = std::nullopt) {
  auto response = webui::mojom::SelectCredentialDialogResponse::New();
  response->task_id = task_id.value();
  if (selected_credential_id.has_value()) {
    response->selected_credential_id = selected_credential_id->value();
    response->permission_duration = permission_duration;
  }
  response->error_reason = error_reason;
  return response;
}

const SkBitmap GenerateSquareBitmap(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(size, size, kOpaque_SkAlphaType));
  bitmap.eraseColor(color);
  bitmap.setImmutable();
  return bitmap;
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(Profile* profile) : ExecutionEngine(profile) {}
  ~MockExecutionEngine() override = default;

  // Type alias to get around the comma in the flat_map template. MOCK_METHOD
  // breaks up the argument using commas.
  using IconMap = base::flat_map<std::string, gfx::Image>;

  MOCK_METHOD(void,
              PromptToSelectCredential,
              (const std::vector<actor_login::Credential>&,
               const IconMap&,
               ToolDelegate::CredentialSelectedCallback),
              (override));
  MOCK_METHOD(actor_login::ActorLoginService&,
              GetActorLoginService,
              (),
              (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
};

class ActorAttemptLoginToolTest : public ActorToolsTest {
 public:
  ActorAttemptLoginToolTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{password_manager::features::kActorLogin,
                              password_manager::features::
                                  kActorLoginQualityLogs,
                              actor::kGlicEnableAutoLoginDialogs,
                              actor::kGlicEnableAutoLoginPersistedPermissions},
        /*disabled_features=*/{});
  }

  ~ActorAttemptLoginToolTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    ASSERT_TRUE(embedded_test_server()->Start());

    ON_CALL(mock_execution_engine(), GetActorLoginService())
        .WillByDefault(ReturnRef(mock_login_service_));

    // Returns the first credential by default.
    ON_CALL(mock_execution_engine(), PromptToSelectCredential)
        .WillByDefault(
            [this](const std::vector<actor_login::Credential>& credentials,
                   const MockExecutionEngine::IconMap&,
                   ToolDelegate::CredentialSelectedCallback callback) {
              std::move(callback).Run(MakeSelectCredentialDialogResponse(
                  actor_task().id(), credentials[0].id,
                  webui::mojom::UserGrantedPermissionDuration::kOneTime));
            });

    ON_CALL(mock_execution_engine(), GetFaviconService())
        .WillByDefault(Return(nullptr));

    OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile())
        ->SetModelQualityLogsUploaderServiceForTesting(
            std::make_unique<
                optimization_guide::TestModelQualityLogsUploaderService>(
                g_browser_process->local_state()));
  }

  std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      Profile* profile) override {
    return std::make_unique<::testing::NiceMock<MockExecutionEngine>>(profile);
  }

  MockActorLoginService& mock_login_service() { return mock_login_service_; }

  MockExecutionEngine& mock_execution_engine() {
    return static_cast<MockExecutionEngine&>(execution_engine());
  }

  optimization_guide::TestModelQualityLogsUploaderService*
  test_mqls_uploader() {
    return static_cast<
        optimization_guide::TestModelQualityLogsUploaderService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile())
            ->GetModelQualityLogsUploaderService());
  }

  const std::vector<
      std::unique_ptr<optimization_guide::proto::LogAiDataRequest>>&
  uploaded_logs() {
    return test_mqls_uploader()->uploaded_logs();
  }

 private:
  MockActorLoginService mock_login_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, Basic) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_TRUE(RequiresPageStabilization(*result.Get<2>().back().result));

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username", last_credential_used->username);
  ASSERT_EQ(uploaded_logs().size(), 1u);
  EXPECT_EQ(
      uploaded_logs()[0]->actor_login().quality().permission_picked(),
      optimization_guide::proto::ActorLoginQuality_PermissionOption_ALLOW_ONCE);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, NoCredentials) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       MultipleCredentialsSelectFirst) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const bool immediately_available_to_login = true;
  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url, immediately_available_to_login),
      MakeTestCredential(u"username2", url, immediately_available_to_login)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username1", last_credential_used->username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       MultipleCredentialsSelectSecond) {
  ON_CALL(mock_execution_engine(), PromptToSelectCredential)
      .WillByDefault(
          [this](const std::vector<actor_login::Credential>& credentials,
                 const MockExecutionEngine::IconMap&,
                 ToolDelegate::CredentialSelectedCallback callback) {
            std::move(callback).Run(MakeSelectCredentialDialogResponse(
                actor_task().id(), credentials[1].id,
                UserGrantedPermissionDuration::kOneTime));
          });

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const bool immediately_available_to_login = true;
  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url, immediately_available_to_login),
      MakeTestCredential(u"username2", url, immediately_available_to_login)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username2", last_credential_used->username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, NoAvailableCredentials) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/false));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       MultipleCredentialsOnlyOneAvailable) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url,
                         /*immediately_available_to_login=*/false),
      MakeTestCredential(u"username2", url,
                         /*immediately_available_to_login=*/true),
      MakeTestCredential(u"username2", url,
                         /*immediately_available_to_login=*/false)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username2", last_credential_used->username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, OnlyUsernameFilled) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredentials(
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_TRUE(RequiresPageStabilization(*result.Get<2>().back().result));
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, OnlyPasswordFilled) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredentials(
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_TRUE(RequiresPageStabilization(*result.Get<2>().back().result));
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, NoSigninForm) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kErrorNoSigninForm);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kLoginNotLoginPage);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       InvalidCredentialForGivenUrl) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kErrorInvalidCredential);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       FillingNotAllowedForGivenUrl) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      base::unexpected(actor_login::ActorLoginError::kFillingNotAllowed));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kLoginFillingNotAllowed);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, FailedAttemptLogin) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      base::unexpected(actor_login::ActorLoginError::kServiceBusy));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kLoginTooManyRequests);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, CredentialSaved) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const bool immediately_available_to_login = true;
  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url, immediately_available_to_login)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // The user selects the first credential, which is cached.
  EXPECT_CALL(mock_execution_engine(), PromptToSelectCredential)
      .WillOnce([this](const std::vector<actor_login::Credential>& credentials,
                       const MockExecutionEngine::IconMap&,
                       ToolDelegate::CredentialSelectedCallback callback) {
        auto response = MakeSelectCredentialDialogResponse(
            actor_task().id(), credentials[0].id,
            UserGrantedPermissionDuration::kAlwaysAllow);
        std::move(callback).Run(std::move(response));
      });
  std::unique_ptr<ToolRequest> action1 = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result1;
  actor_task().Act(ToRequestList(action1), result1.GetCallback());
  ExpectOkResult(result1);
  ASSERT_TRUE(mock_login_service().last_credential_used().has_value());
  EXPECT_EQ(u"username1",
            mock_login_service().last_credential_used()->username);
  EXPECT_TRUE(mock_login_service().last_permission_was_permanent());

  // The second time, the user should not be prompted. Note that we don't need
  // to set another expectation on `PromptToSelectCredential` because the
  // WillOnce() above.
  std::unique_ptr<ToolRequest> action2 = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result2;
  actor_task().Act(ToRequestList(action2), result2.GetCallback());
  ExpectOkResult(result2);
  ASSERT_TRUE(mock_login_service().last_credential_used().has_value());
  EXPECT_EQ(u"username1",
            mock_login_service().last_credential_used()->username);
  EXPECT_TRUE(mock_login_service().last_permission_was_permanent());
  // There are two tool calls, so expect 2 logs
  ASSERT_EQ(uploaded_logs().size(), 2u);
  EXPECT_EQ(uploaded_logs()[0]->actor_login().quality().permission_picked(),
            optimization_guide::proto::
                ActorLoginQuality_PermissionOption_ALWAYS_ALLOW);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, UsePersistedCredential) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  actor_login::Credential persisted_cred = MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true);
  persisted_cred.has_persistent_permission = true;

  mock_login_service().SetCredentials(std::vector{persisted_cred});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // Since we'll use a credential that we already have permission for, we should
  // skip the confirmation prompt and use the credential automatically.
  EXPECT_CALL(mock_execution_engine(), PromptToSelectCredential).Times(0);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  ASSERT_TRUE(mock_login_service().last_credential_used().has_value());
  EXPECT_EQ(u"username", mock_login_service().last_credential_used()->username);
  EXPECT_TRUE(mock_login_service().last_permission_was_permanent());
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, SavedCredentialNotUsed) {
  const GURL blank_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), blank_url));

  GURL blank_origin = blank_url.GetWithEmptyPath();

  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", blank_url.GetWithEmptyPath(),
                                     /*immediately_available_to_login=*/true)};
  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // The user selects the first credential, which is cached.
  EXPECT_CALL(mock_execution_engine(), PromptToSelectCredential)
      .WillOnce([this](const std::vector<actor_login::Credential>& credentials,
                       const MockExecutionEngine::IconMap&,
                       ToolDelegate::CredentialSelectedCallback callback) {
        auto response = MakeSelectCredentialDialogResponse(
            actor_task().id(), credentials[0].id,
            UserGrantedPermissionDuration::kAlwaysAllow);
        std::move(callback).Run(std::move(response));
      });
  std::unique_ptr<ToolRequest> action1 = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result1;
  actor_task().Act(ToRequestList(action1), result1.GetCallback());
  ExpectOkResult(result1);
  ASSERT_TRUE(mock_login_service().last_credential_used().has_value());
  EXPECT_EQ(u"username1",
            mock_login_service().last_credential_used()->username);
  EXPECT_TRUE(mock_login_service().last_permission_was_permanent());

  const GURL link_url = embedded_https_test_server().GetURL(
      "subdomain.example.com", "/actor/link.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), link_url));
  mock_login_service().SetCredentials(std::vector<actor_login::Credential>{
      MakeTestCredential(u"username2", link_url.GetWithEmptyPath(),
                         /*immediately_available_to_login=*/true)});
  // The second time, the user is prompted again because the page's origin is
  // subdomain.example.com. The previously cached (and selected) credential is
  // for example.com.
  EXPECT_CALL(mock_execution_engine(), PromptToSelectCredential)
      .WillOnce([this](const std::vector<actor_login::Credential>& credentials,
                       const MockExecutionEngine::IconMap&,
                       ToolDelegate::CredentialSelectedCallback callback) {
        auto response = MakeSelectCredentialDialogResponse(
            actor_task().id(), credentials[0].id,
            UserGrantedPermissionDuration::kOneTime);
        std::move(callback).Run(std::move(response));
      });

  std::unique_ptr<ToolRequest> action2 = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result2;
  actor_task().Act(ToRequestList(action2), result2.GetCallback());
  ExpectOkResult(result2);
  ASSERT_TRUE(mock_login_service().last_credential_used().has_value());
  EXPECT_EQ(u"username2",
            mock_login_service().last_credential_used()->username);
  EXPECT_FALSE(mock_login_service().last_permission_was_permanent());
}

// If a navigation occurs during credential selection, do not proceed with the
// login attempt and return an error instead.
IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       NavigationWhileRequestingCredential) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  const GURL url2 = embedded_https_test_server().GetURL("other.example.com",
                                                        "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  base::test::TestFuture<base::OnceClosure> select_creds;
  EXPECT_CALL(mock_execution_engine(), PromptToSelectCredential)
      .WillOnce([this, &select_creds](
                    const std::vector<actor_login::Credential>& credentials,
                    const MockExecutionEngine::IconMap&,
                    ToolDelegate::CredentialSelectedCallback callback) {
        select_creds.SetValue(base::BindOnce(
            std::move(callback), MakeSelectCredentialDialogResponse(
                                     actor_task().id(), credentials[0].id,
                                     UserGrantedPermissionDuration::kOneTime)));
      });

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());

  ASSERT_TRUE(select_creds.Wait());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url2));

  select_creds.Take().Run();
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginPageChangedDuringSelection);
}

class ActorAttemptLoginToolTestWithFaviconService
    : public ActorAttemptLoginToolTest {
 public:
  ActorAttemptLoginToolTestWithFaviconService() = default;
  ~ActorAttemptLoginToolTestWithFaviconService() override = default;

  void SetUpOnMainThread() override {
    ActorAttemptLoginToolTest::SetUpOnMainThread();
    ON_CALL(mock_execution_engine(), GetFaviconService())
        .WillByDefault(Return(&mock_favicon_service_));

    // Empty favicon by default.
    ON_CALL(mock_favicon_service_, GetFaviconImageForPageURL)
        .WillByDefault([](const GURL& page_url,
                          favicon_base::FaviconImageCallback callback,
                          base::CancelableTaskTracker* tracker) {
          favicon_base::FaviconImageResult result;
          result.image = gfx::Image();
          std::move(callback).Run(std::move(result));
          return static_cast<base::CancelableTaskTracker::TaskId>(1);
        });
  }

  favicon::MockFaviconService& mock_favicon_service() {
    return mock_favicon_service_;
  }

 private:
  favicon::MockFaviconService mock_favicon_service_;
};

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService, NoService) {
  ON_CALL(mock_execution_engine(), GetFaviconService())
      .WillByDefault(Return(nullptr));
  EXPECT_CALL(mock_favicon_service(), GetFaviconImageForPageURL).Times(0);

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(mock_execution_engine(),
              PromptToSelectCredential(/*credentials=*/credentials,
                                       /*icons=*/MockExecutionEngine::IconMap{},
                                       /*callback=*/_));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username1", last_credential_used->username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService,
                       EmptyFavicons) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  GURL origin = url.GetWithEmptyPath();
  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(mock_execution_engine(),
              PromptToSelectCredential(/*credentials=*/credentials,
                                       /*icons=*/MockExecutionEngine::IconMap{},
                                       /*callback=*/_));
  EXPECT_CALL(mock_favicon_service(), GetFaviconImageForPageURL(origin, _, _));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username1", last_credential_used->username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService,
                       OneFavicon) {
  const SkBitmap bitmap = GenerateSquareBitmap(/*size=*/10, SK_ColorRED);
  auto image = gfx::Image::CreateFrom1xBitmap(bitmap);
  ON_CALL(mock_favicon_service(), GetFaviconImageForPageURL)
      .WillByDefault([&](const GURL& page_url,
                         favicon_base::FaviconImageCallback callback,
                         base::CancelableTaskTracker* tracker) {
        favicon_base::FaviconImageResult result;
        result.image = image;
        std::move(callback).Run(std::move(result));
        return static_cast<base::CancelableTaskTracker::TaskId>(1);
      });

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  GURL origin = url.GetWithEmptyPath();
  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(
      mock_execution_engine(),
      PromptToSelectCredential(
          /*credentials=*/credentials,
          /*icons=*/MockExecutionEngine::IconMap{{origin.spec(), image}},
          /*callback=*/_));
  EXPECT_CALL(mock_favicon_service(), GetFaviconImageForPageURL(origin, _, _));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username1", last_credential_used->username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService,
                       TwoFavicons) {
  auto blank_icon = gfx::Image::CreateFrom1xBitmap(
      GenerateSquareBitmap(/*size=*/10, SK_ColorWHITE));
  auto link_icon = gfx::Image::CreateFrom1xBitmap(
      GenerateSquareBitmap(/*size=*/15, SK_ColorBLUE));

  const GURL blank_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), blank_url));
  const GURL link_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/link.html");

  ON_CALL(mock_favicon_service(), GetFaviconImageForPageURL)
      .WillByDefault([&](const GURL& page_url,
                         favicon_base::FaviconImageCallback callback,
                         base::CancelableTaskTracker* tracker) {
        favicon_base::FaviconImageResult result;
        if (page_url == blank_url.GetWithEmptyPath()) {
          result.image = blank_icon;
        } else if (page_url == link_url.GetWithEmptyPath()) {
          result.image = link_icon;
        }
        std::move(callback).Run(std::move(result));
        return static_cast<base::CancelableTaskTracker::TaskId>(1);
      });

  GURL blank_origin = blank_url.GetWithEmptyPath();
  GURL link_origin = link_url.GetWithEmptyPath();
  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", blank_url,
                                     /*immediately_available_to_login=*/true),
                  MakeTestCredential(u"username2", link_url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(
      mock_execution_engine(),
      PromptToSelectCredential(
          /*credentials=*/credentials,
          /*icons=*/
          MockExecutionEngine::IconMap{{blank_origin.spec(), blank_icon},
                                       {link_origin.spec(), link_icon}},
          /*callback=*/_));
  EXPECT_CALL(mock_favicon_service(),
              GetFaviconImageForPageURL(blank_origin, _, _));
  EXPECT_CALL(mock_favicon_service(),
              GetFaviconImageForPageURL(link_origin, _, _));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username1", last_credential_used->username);
}

}  // namespace
}  // namespace actor
