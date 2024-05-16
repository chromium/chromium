// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/model_validator_keyed_service.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

class ModelExecutionValidationBrowserTestBase : public InProcessBrowserTest {
 public:
  ModelExecutionValidationBrowserTestBase() = default;
  ~ModelExecutionValidationBrowserTestBase() override = default;

  ModelExecutionValidationBrowserTestBase(
      const ModelExecutionValidationBrowserTestBase&) = delete;
  ModelExecutionValidationBrowserTestBase& operator=(
      const ModelExecutionValidationBrowserTestBase&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOptimizationGuideModelExecution);

    model_execution_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {
        GURL(kOptimizationGuideServiceModelExecutionDefaultURL).host()};
    model_execution_server_->SetSSLConfig(cert_config);
    model_execution_server_->RegisterRequestHandler(
        base::BindRepeating(&ModelExecutionValidationBrowserTestBase::
                                HandleGetModelExecutionRequest,
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
                base::BindRepeating(&ModelExecutionValidationBrowserTestBase::
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

  void EnableServerModelExecutionFailure() {
    should_server_fail_model_execution_ = true;
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetModelExecutionRequest(const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));
    EXPECT_TRUE(base::Contains(request.headers,
                               net::HttpRequestHeaders::kAuthorization));

    if (should_server_fail_model_execution_) {
      response->set_code(net::HTTP_NOT_FOUND);
      return std::move(response);
    }

    proto::StringValue string_response;
    string_response.set_value("test_response");
    proto::ExecuteResponse execute_response;
    proto::Any* any_metadata = execute_response.mutable_response_metadata();
    any_metadata->set_type_url("type.googleapis.com/" +
                               string_response.GetTypeName());
    string_response.SerializeToString(any_metadata->mutable_value());

    std::string serialized_response;
    execute_response.SerializeToString(&serialized_response);
    response->set_code(net::HTTP_OK);
    response->set_content(serialized_response);
    return std::move(response);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> model_execution_server_;
  base::HistogramTester histogram_tester_;

  bool should_server_fail_model_execution_ = false;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

class ModelExecutionValidationBrowserTest
    : public ModelExecutionValidationBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    ModelExecutionValidationBrowserTestBase::SetUpCommandLine(cmd);
    cmd->AppendSwitch(switches::kDebugLoggingEnabled);
    cmd->AppendSwitchASCII(switches::kModelExecutionValidate, "test_request");
  }
};

// TODO(b/318433299, crbug.com/1520214): Flaky on linux-chromeos, Win and Mac.
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_ModelExecutionSuccess DISABLED_ModelExecutionSuccess
#else
#define MAYBE_ModelExecutionSuccess ModelExecutionSuccess
#endif
IN_PROC_BROWSER_TEST_F(ModelExecutionValidationBrowserTest,
                       MAYBE_ModelExecutionSuccess) {
  EnableSignin();
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "OptimizationGuide.ModelExecution.Result.Test", 1);

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Test", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus.Test",
      FetcherRequestStatus::kSuccess, 1);
}

// TODO(b/318433299, crbug.com/1520214): Flaky on linux-chromeos and win
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_ModelExecutionFailsServerFailure \
  DISABLED_ModelExecutionFailsServerFailure
#else
#define MAYBE_ModelExecutionFailsServerFailure ModelExecutionFailsServerFailure
#endif
IN_PROC_BROWSER_TEST_F(ModelExecutionValidationBrowserTest,
                       MAYBE_ModelExecutionFailsServerFailure) {
  EnableServerModelExecutionFailure();
  EnableSignin();
  RetryForHistogramUntilCountReached(
      &histogram_tester_, "OptimizationGuide.ModelExecution.Result.Test", 1);

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.Result.Test", false, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutionFetcher.RequestStatus.Test",
      FetcherRequestStatus::kResponseError, 1);
}

}  // namespace optimization_guide
