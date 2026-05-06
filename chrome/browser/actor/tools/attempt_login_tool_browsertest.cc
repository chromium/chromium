// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/ui_test_utils.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

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

void PostFederatedLoginResumeAfterClick(
    std::unique_ptr<ExecutionEngineStateWaiter>& state_waiter,
    Profile* profile,
    tabs::TabInterface* tab,
    content::webid::FederatedLoginResult fed_result,
    MockActorLoginService::FederatedLoginResumeCallback resume_callback) {
  auto* actor_service = actor::ActorKeyedService::Get(profile);
  actor::ActorTask* task = actor_service->GetTaskFromTab(*tab);
  // ExecutionEngineStateWaiter is notified before the state change, so we need
  // to asynchronously resume.
  auto async_resume = base::BindOnce(
      [](MockActorLoginService::FederatedLoginResumeCallback resume_callback,
         content::webid::FederatedLoginResult fed_result) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(resume_callback), fed_result));
      },
      std::move(resume_callback), fed_result);
  // Wait until the next tool invoke (for the click).
  state_waiter = std::make_unique<ExecutionEngineStateWaiter>(
      std::move(async_resume), task->GetExecutionEngine(),
      ExecutionEngine::State::kToolInvoke);
}

std::unique_ptr<KeyedService> CreateMockAffiliationService(
    content::BrowserContext* context) {
  auto service = std::make_unique<
      testing::NiceMock<affiliations::MockAffiliationService>>();
  ON_CALL(*service, GetAffiliationsAndBranding(_, _))
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<1>(
          std::vector<affiliations::Facet>(), /*success=*/true));
  return service;
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(ActorTask& task) : ExecutionEngine(task) {}
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
        /*enabled_features=*/
        {password_manager::features::kActorLogin,
         password_manager::features::kActorLoginQualityLogs,
         features::kGlicActor},
        // TODO(crbug.com/480920277): Remove the FedCM flag once the prototyping
        // is complete.
        /*disabled_features=*/{kGlicCrossOriginNavigationGating,
                               features::kFedCmEmbedderInitiatedLogin});
  }

  ~ActorAttemptLoginToolTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    ActorToolsTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AffiliationServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(&CreateMockAffiliationService));
                }));
  }

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

  static std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      ActorTask& task) {
    return std::make_unique<::testing::NiceMock<MockExecutionEngine>>(task);
  }

  MockActorLoginService& mock_login_service() { return mock_login_service_; }

  MockExecutionEngine& mock_execution_engine() {
    return static_cast<MockExecutionEngine&>(execution_engine());
  }

  affiliations::MockAffiliationService* mock_affiliation_service() {
    return static_cast<affiliations::MockAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(GetProfile()));
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
  base::CallbackListSubscription create_services_subscription_;
  ScopedExecutionEngineFactory mock_execution_engine_factory_{
      base::BindRepeating(ActorAttemptLoginToolTest::CreateExecutionEngine)};
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
  EXPECT_TRUE(RequiresPageStabilization(*result.Get().back().result));

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

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       NoCredentialsAvailableForLogin) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/false));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kErrorNoSigninForm);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  // If there are saved credentials, but none is available for login it means
  // that there is no login form on the page.
  ExpectErrorResult(result, mojom::ActionResultCode::kLoginNotLoginPage);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       MultipleCredentialsOnlyOneAvailable) {
  if (base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)) {
    // This behaviour does not apply when federated credentials are supported
    // where we allow password selection regardless of availability.
    GTEST_SKIP();
  }

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
  EXPECT_TRUE(RequiresPageStabilization(*result.Get().back().result));
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
  EXPECT_TRUE(RequiresPageStabilization(*result.Get().back().result));
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       UsesStrongAffiliationsForAutoSelect) {
  const GURL url_a =
      embedded_https_test_server().GetURL("a.com", "/actor/blank.html");
  const GURL url_b =
      embedded_https_test_server().GetURL("b.com", "/actor/blank.html");

  // User logs into Site A
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_a));
  // Site A and B are strongly affiliated.
  affiliations::FacetURI facet_a =
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          url_a.GetWithEmptyPath().spec());
  affiliations::FacetURI facet_b =
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          url_b.GetWithEmptyPath().spec());
  std::vector<affiliations::Facet> facets = {affiliations::Facet(facet_a),
                                             affiliations::Facet(facet_b)};
  EXPECT_CALL(*mock_affiliation_service(),
              GetAffiliationsAndBranding(facet_a, _))
      .WillOnce(base::test::RunOnceCallback<1>(facets, /*success=*/true));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username_shared", url_a, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  {
    std::unique_ptr<ToolRequest> action =
        MakeAttemptLoginRequest(*active_tab());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  ASSERT_TRUE(mock_login_service().last_credential_used().has_value());
  actor_login::Credential::Id first_login_cred_id =
      mock_login_service().last_credential_used()->id;

  // User attempts login on Site B
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_b));

  EXPECT_CALL(mock_execution_engine(), PromptToSelectCredential).Times(0);

  {
    std::unique_ptr<ToolRequest> action =
        MakeAttemptLoginRequest(*active_tab());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  ASSERT_TRUE(mock_login_service().last_credential_used().has_value());
  EXPECT_EQ(first_login_cred_id,
            mock_login_service().last_credential_used()->id);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       UsesStrongAffiliationsIgnoresAndroidFacets) {
  const GURL source_url =
      embedded_https_test_server().GetURL("a.com", "/actor/blank.html");
  const GURL web_affiliate_url =
      embedded_https_test_server().GetURL("b.com", "/actor/blank.html");
  const std::string android_facet_spec = "android://hash@com.example.android";

  affiliations::Facet web_facet(
      affiliations::FacetURI::FromPotentiallyInvalidSpec(
          web_affiliate_url.GetWithEmptyPath().spec()));
  affiliations::Facet android_facet(
      affiliations::FacetURI::FromPotentiallyInvalidSpec(android_facet_spec));

  EXPECT_CALL(*mock_affiliation_service(),
              GetAffiliationsAndBranding(
                  affiliations::FacetURI::FromPotentiallyInvalidSpec(
                      source_url.GetWithEmptyPath().spec()),
                  _))
      .WillOnce(base::test::RunOnceCallback<1>(
          std::vector<affiliations::Facet>{web_facet, android_facet},
          /*success=*/true));

  // Navigate to source URL
  ASSERT_TRUE(content::NavigateToURL(web_contents(), source_url));
  mock_login_service().SetCredential(MakeTestCredential(
      u"username", source_url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // Run the tool and check that the map of affiliations does not include
  // android facets.
  {
    std::unique_ptr<ToolRequest> action =
        MakeAttemptLoginRequest(*active_tab());
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  const auto& affiliated_map =
      mock_execution_engine().GetAffiliatedOriginMapForTesting();

  url::Origin web_origin = url::Origin::Create(web_affiliate_url);
  url::Origin source_origin = url::Origin::Create(source_url);

  ASSERT_TRUE(affiliated_map.contains(web_origin));
  EXPECT_EQ(affiliated_map.at(web_origin), source_origin);

  url::Origin android_origin = url::Origin::Create(GURL(android_facet_spec));
  EXPECT_FALSE(affiliated_map.contains(android_origin));
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

class ActorAttemptLoginToolFederatedTest : public ActorAttemptLoginToolTest {
 public:
  ActorAttemptLoginToolFederatedTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFedCmEmbedderInitiatedLogin);
  }

  ~ActorAttemptLoginToolFederatedTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// One of the federated tests intentionally triggers the observation timeout. We
// shorten that timeout so that the test completes in a reasonable amount of
// time.
class ActorAttemptLoginToolFederatedShortDelayTest
    : public ActorAttemptLoginToolFederatedTest {
 public:
  ActorAttemptLoginToolFederatedShortDelayTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicActor,
          {{features::kActorObservationDelayTimeout.name, "1s"}}}},
        {});
  }

  ~ActorAttemptLoginToolFederatedShortDelayTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolFederatedTest,
                       FederatedLoginClicksProviderButton) {
  const GURL idp_url = GURL("https://accounts.google.com");
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  const GURL signin_success_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(
      MakeTestCredentialFederated(u"username", idp_url));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kRequiresButtonClick);
  std::unique_ptr<ExecutionEngineStateWaiter> state_waiter = nullptr;
  mock_login_service().SetFederatedLoginDelay(base::BindOnce(
      &PostFederatedLoginResumeAfterClick, std::ref(state_waiter), GetProfile(),
      active_tab(), content::webid::FederatedLoginResult::kSuccess));

  std::optional<int> password_button_id =
      content::GetDOMNodeId(*main_frame(), "#submit-button");
  ASSERT_TRUE(password_button_id);
  std::optional<int> provider_button_id =
      content::GetDOMNodeId(*main_frame(), "#provider-button");
  ASSERT_TRUE(provider_button_id);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequestByNodeIds(
      *active_tab(), password_button_id, provider_button_id);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& action_results = result.Get();
  // Although we have multiple tools invoked internally, we should not expose
  // this to the caller.
  EXPECT_EQ(1, action_results.size());

  EXPECT_EQ(signin_success_url, web_contents()->GetLastCommittedURL());
  EXPECT_TRUE(mock_login_service().last_sequence_succeeded());
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolFederatedTest,
                       PasswordLoginClicksSubmitButton) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  const GURL signin_success_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::optional<int> password_button_id =
      content::GetDOMNodeId(*main_frame(), "#submit-button");
  ASSERT_TRUE(password_button_id);
  std::optional<int> provider_button_id =
      content::GetDOMNodeId(*main_frame(), "#provider-button");
  ASSERT_TRUE(provider_button_id);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequestByNodeIds(
      *active_tab(), password_button_id, provider_button_id);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& action_results = result.Get();
  // Although we have multiple tools invoked internally, we should not expose
  // this to the caller.
  EXPECT_EQ(1, action_results.size());

  GURL::Replacements replacements;
  replacements.ClearQuery();
  EXPECT_EQ(
      signin_success_url.ReplaceComponents(replacements),
      web_contents()->GetLastCommittedURL().ReplaceComponents(replacements));
  EXPECT_TRUE(mock_login_service().last_sequence_succeeded());
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolFederatedTest,
                       FederatedLoginProviderErrorAfterClick) {
  const GURL idp_url = GURL("https://accounts.google.com");
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  const GURL signin_success_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(
      MakeTestCredentialFederated(u"username", idp_url));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kRequiresButtonClick);
  std::unique_ptr<ExecutionEngineStateWaiter> state_waiter = nullptr;
  mock_login_service().SetFederatedLoginDelay(base::BindOnce(
      &PostFederatedLoginResumeAfterClick, std::ref(state_waiter), GetProfile(),
      active_tab(), content::webid::FederatedLoginResult::kIdpReturnedError));

  std::optional<int> password_button_id =
      content::GetDOMNodeId(*main_frame(), "#submit-button");
  ASSERT_TRUE(password_button_id);
  std::optional<int> provider_button_id =
      content::GetDOMNodeId(*main_frame(), "#provider-button");
  ASSERT_TRUE(provider_button_id);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequestByNodeIds(
      *active_tab(), password_button_id, provider_button_id);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginFederatedIdpReturnedError);

  const auto& action_results = result.Get();
  // Although we have multiple tools invoked internally, we should not expose
  // this to the caller.
  EXPECT_EQ(1, action_results.size());

  EXPECT_FALSE(mock_login_service().last_sequence_succeeded());
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolFederatedShortDelayTest,
                       FederatedLoginTimeoutDuringObservationDelay) {
  const GURL idp_url = GURL("https://accounts.google.com");
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  const GURL signin_success_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(
      MakeTestCredentialFederated(u"username", idp_url));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kRequiresButtonClick);
  // Intentionally never resume. We expect the observation delay logic to time
  // us out.
  mock_login_service().SetFederatedLoginDelay(base::DoNothing());

  std::optional<int> password_button_id =
      content::GetDOMNodeId(*main_frame(), "#submit-button");
  ASSERT_TRUE(password_button_id);
  std::optional<int> provider_button_id =
      content::GetDOMNodeId(*main_frame(), "#provider-button");
  ASSERT_TRUE(provider_button_id);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequestByNodeIds(
      *active_tab(), password_button_id, provider_button_id);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kLoginFederatedTimeout);

  const auto& action_results = result.Get();
  // Although we have multiple tools invoked internally, we should not expose
  // this to the caller.
  EXPECT_EQ(1, action_results.size());

  EXPECT_FALSE(mock_login_service().last_sequence_succeeded());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolFederatedTest,
                       FederatedLoginClicksProviderButtonWithPopup) {
  const GURL idp_url = GURL("https://accounts.google.com");
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  const GURL signin_success_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(
      MakeTestCredentialFederated(u"username", idp_url));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kRequiresButtonClick);

  std::optional<int> password_button_id =
      content::GetDOMNodeId(*main_frame(), "#submit-button");
  ASSERT_TRUE(password_button_id);
  std::optional<int> provider_popup_button_id =
      content::GetDOMNodeId(*main_frame(), "#provider-popup-button");
  ASSERT_TRUE(provider_popup_button_id);

  // We need to perform a useless action on the tab, in order to get the actor
  // overlay to show. Otherwise, the WebContentsAddedObserver below will confuse
  // it for the popup triggered by the attempt login action.
  std::unique_ptr<ToolRequest> click_on_nothing_action =
      MakeClickRequest(*active_tab(), gfx::Point(1, 1));
  ActResultFuture click_result;
  actor_task().Act(ToRequestList(click_on_nothing_action),
                   click_result.GetCallback());
  ExpectOkResult(click_result);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequestByNodeIds(
      *active_tab(), password_button_id, provider_popup_button_id);

  tabs::TabInterface* original_tab = active_tab();

  // Open a new unrelated tab in the foreground.
  const GURL other_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), other_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::WebContentsAddedObserver web_contents_added_observer;

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  const auto& action_results = result.Get();
  // Although we have multiple tools invoked internally, we should not expose
  // this to the caller.
  EXPECT_EQ(1, action_results.size());

  content::WebContents* new_contents =
      web_contents_added_observer.GetWebContents();
  ASSERT_TRUE(new_contents);

  bool platform_supports_programmatic_window_activation = true;
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    platform_supports_programmatic_window_activation = false;
  }
#endif
  if (platform_supports_programmatic_window_activation) {
    // Since a different tab was in the foreground, the popup should not have
    // been focused.
    EXPECT_FALSE(new_contents->GetRenderWidgetHostView()->HasFocus());
  }

  content::TestNavigationObserver navigation_observer(
      original_tab->GetContents());
  EXPECT_TRUE(content::ExecJs(new_contents,
                              "window.opener.postMessage('signin-complete');"));
  navigation_observer.Wait();

  EXPECT_EQ(signin_success_url, navigation_observer.last_navigation_url());
  EXPECT_TRUE(mock_login_service().last_sequence_succeeded());
}
#endif

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolFederatedTest,
                       FederatedLoginFailedButtonClick) {
  WaitTool::SetNoDelayForTesting();

  const GURL idp_url = GURL("https://accounts.google.com");
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  const GURL signin_success_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(
      MakeTestCredentialFederated(u"username", idp_url));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kRequiresButtonClick);

  std::optional<int> password_button_id =
      content::GetDOMNodeId(*main_frame(), "#submit-button");
  ASSERT_TRUE(password_button_id);
  std::optional<int> provider_button_id =
      content::GetDOMNodeId(*main_frame(), "#provider-button");
  ASSERT_TRUE(provider_button_id);

  ASSERT_TRUE(content::ExecJs(
      main_frame(),
      "document.getElementById('provider-button').disabled = true;"));

  std::unique_ptr<ToolRequest> login_action = MakeAttemptLoginRequestByNodeIds(
      *active_tab(), password_button_id, provider_button_id);

  // This helps confirm that the click tool is sequenced correctly. The click
  // should fail before this, so we should not see a second action that
  // succeeded.
  std::unique_ptr<ToolRequest> other_action = MakeWaitRequest();

  ActResultFuture result;
  actor_task().Act(ToRequestList(login_action, other_action),
                   result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementDisabled);

  const auto& action_results = result.Get();
  // The caller should see the failed click as the attempt login action failing.
  EXPECT_EQ(1, action_results.size());

  EXPECT_FALSE(mock_login_service().last_sequence_succeeded());
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolFederatedTest,
                       FederatedLoginButtonNotProvided) {
  const GURL idp_url = GURL("https://accounts.google.com");
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/sign_in_page.html");
  const GURL signin_success_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(
      MakeTestCredentialFederated(u"username", idp_url));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kRequiresButtonClick);

  std::optional<int> password_button_id =
      content::GetDOMNodeId(*main_frame(), "#submit-button");
  ASSERT_TRUE(password_button_id);

  // Intentionally do not identify the provider button.
  std::optional<int> provider_button_id = std::nullopt;

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequestByNodeIds(
      *active_tab(), password_button_id, provider_button_id);

  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);

  EXPECT_FALSE(mock_login_service().last_sequence_succeeded());
}

}  // namespace
}  // namespace actor
