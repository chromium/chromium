// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test.pb.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/optimization_guide/core/model_execution/model_execution_manager.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

using base::test::TestMessage;

namespace {

enum class ModelExecutionRemoteResponseType {
  kSuccessful = 0,
  kUnsuccessful = 1,
  kMalformed = 2,
};

TestMessage BuildTestMessage(const std::string& test_message_str) {
  TestMessage test_message;
  test_message.set_test(test_message_str);
  return test_message;
}

proto::ExecuteResponse BuildTestExecuteResponse(const TestMessage& message) {
  proto::ExecuteResponse execute_response;
  proto::Any* any_metadata = execute_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/" + message.GetTypeName());
  message.SerializeToString(any_metadata->mutable_value());
  return execute_response;
}

}  // namespace

class ModelExecutionBrowserTestBase : public InProcessBrowserTest {
 public:
  ModelExecutionBrowserTestBase() = default;
  ~ModelExecutionBrowserTestBase() override = default;

  ModelExecutionBrowserTestBase(const ModelExecutionBrowserTestBase&) = delete;
  ModelExecutionBrowserTestBase& operator=(
      const ModelExecutionBrowserTestBase&) = delete;

  void SetUp() override {
    InitializeFeatureList();
    model_execution_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {
        GURL(kOptimizationGuideServiceModelExecutionDefaultURL).host()};
    model_execution_server_->SetSSLConfig(cert_config);
    model_execution_server_->RegisterRequestHandler(base::BindRepeating(
        &ModelExecutionBrowserTestBase::HandleGetModelExecutionRequest,
        base::Unretained(this)));
    ASSERT_TRUE(model_execution_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ModelExecutionBrowserTestBase::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitchASCII(
        switches::kOptimizationGuideServiceModelExecutionURL,
        model_execution_server_
            ->GetURL(
                GURL(kOptimizationGuideServiceModelExecutionDefaultURL).host(),
                "/")
            .spec());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(model_execution_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void EnableSignin() {
    identity_test_env_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable("user@gmail.com",
                                      signin::ConsentLevel::kSignin);
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  // Executes the model for the feature, waits until the response is received,
  // and returns the response.
  void ExecuteModel(proto::ModelExecutionFeature feature,
                    const TestMessage& request_metadata,
                    Profile* profile = nullptr) {
    if (!profile) {
      profile = browser()->profile();
    }
    base::RunLoop run_loop;
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
    optimization_guide_keyed_service->ExecuteModel(
        feature, request_metadata,
        base::BindOnce(&ModelExecutionBrowserTestBase::OnModelExecutionResponse,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void SetExpectedBearerAccessToken(
      const std::string& expected_bearer_access_token) {
    expected_bearer_access_token_ = expected_bearer_access_token;
  }

  void SetResponseType(ModelExecutionRemoteResponseType response_type) {
    response_type_ = response_type;
  }

 protected:
  void OnModelExecutionResponse(
      base::OnceClosure on_model_execution_closure,
      OptimizationGuideModelExecutionResult result,
      std::unique_ptr<ModelQualityLogEntry> log_entry) {
    if (result.has_value()) {
      model_execution_result_ = base::ok(result.value());
    } else {
      model_execution_result_ = base::unexpected(result.error());
    }
    std::move(on_model_execution_closure).Run();
  }

  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetModelExecutionRequest(const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    // If the request is a GET, it corresponds to a navigation so return a
    // normal response.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));

    // Access token should be set.
    EXPECT_TRUE(base::Contains(request.headers,
                               net::HttpRequestHeaders::kAuthorization));
    EXPECT_EQ(expected_bearer_access_token_,
              request.headers.at(net::HttpRequestHeaders::kAuthorization));

    if (response_type_ == ModelExecutionRemoteResponseType::kSuccessful) {
      std::string serialized_response;
      proto::ExecuteResponse execute_response =
          BuildTestExecuteResponse(BuildTestMessage("foo response"));
      execute_response.SerializeToString(&serialized_response);
      response->set_code(net::HTTP_OK);
      response->set_content(serialized_response);
    } else if (response_type_ ==
               ModelExecutionRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);
    } else if (response_type_ == ModelExecutionRemoteResponseType::kMalformed) {
      response->set_code(net::HTTP_OK);
      response->set_content("Not a proto");
    } else {
      NOTREACHED();
    }

    return std::move(response);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  // Virtualize for testing different feature configurations.
  virtual void InitializeFeatureList() {}

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> model_execution_server_;
  base::HistogramTester histogram_tester_;

  ModelExecutionRemoteResponseType response_type_ =
      ModelExecutionRemoteResponseType::kSuccessful;

  // The last model execution response received.
  absl::optional<OptimizationGuideModelExecutionResult> model_execution_result_;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;

  // The expected authorization header holding the bearer access token.
  std::string expected_bearer_access_token_;
};

class ModelExecutionDisabledBrowserTest : public ModelExecutionBrowserTestBase {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitAndDisableFeature(
        features::kOptimizationGuideModelExecution);
  }
};

IN_PROC_BROWSER_TEST_F(ModelExecutionDisabledBrowserTest,
                       ModelExecutionDisabled) {
  ExecuteModel(proto::MODEL_EXECUTION_FEATURE_COMPOSE, BuildTestMessage("foo"));
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());
}

class ModelExecutionEnabledBrowserTest : public ModelExecutionBrowserTestBase {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOptimizationGuideModelExecution);
  }
};

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionDisabledInIncognito) {
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  ExecuteModel(proto::MODEL_EXECUTION_FEATURE_COMPOSE, BuildTestMessage("foo"),
               otr_browser->profile());
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsNoUserSignIn) {
  ExecuteModel(proto::MODEL_EXECUTION_FEATURE_COMPOSE, BuildTestMessage("foo"));
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kPermissionDenied,
            model_execution_result_->error().error());
  EXPECT_FALSE(model_execution_result_->error().transient());
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionSuccess) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");

  ExecuteModel(proto::MODEL_EXECUTION_FEATURE_COMPOSE, BuildTestMessage("foo"));
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_TRUE(model_execution_result_->has_value());
  EXPECT_EQ(
      "foo response",
      ParsedAnyMetadata<TestMessage>(model_execution_result_->value())->test());
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsForUnsuccessfulResponse) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kUnsuccessful);

  ExecuteModel(proto::MODEL_EXECUTION_FEATURE_COMPOSE, BuildTestMessage("foo"));
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsForMalformedResponse) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kMalformed);

  ExecuteModel(proto::MODEL_EXECUTION_FEATURE_COMPOSE, BuildTestMessage("foo"));
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());
}

}  // namespace optimization_guide
