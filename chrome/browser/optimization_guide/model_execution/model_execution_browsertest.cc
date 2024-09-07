// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test.pb.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_manager.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/tflite/buildflags.h"

namespace optimization_guide {

namespace {

enum class ModelExecutionRemoteResponseType {
  kSuccessful = 0,
  kUnsuccessful = 1,
  kMalformed = 2,
  kErrorFiltered = 3,
  kUnsupportedLanguage = 4,
};

proto::ExecuteResponse BuildComposeResponse(const std::string& output) {
  proto::ComposeResponse compose_response;
  compose_response.set_output(output);
  proto::ExecuteResponse execute_response;
  proto::Any* any_metadata = execute_response.mutable_response_metadata();
  any_metadata->set_type_url("type.googleapis.com/" +
                             compose_response.GetTypeName());
  compose_response.SerializeToString(any_metadata->mutable_value());
  auto response_data = ParsedAnyMetadata<proto::ComposeResponse>(*any_metadata);
  EXPECT_TRUE(response_data);
  return execute_response;
}

proto::ExecuteResponse BuildTestErrorExecuteResponse(
    const proto::ErrorState& state) {
  proto::ExecuteResponse execute_response;
  execute_response.mutable_error_response()->set_error_state(state);
  return execute_response;
}

class ScopedSetMetricsConsent {
 public:
  // Enables or disables metrics consent based off of |consent|.
  explicit ScopedSetMetricsConsent(bool consent) : consent_(consent) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &consent_);
  }

  ScopedSetMetricsConsent(const ScopedSetMetricsConsent&) = delete;
  ScopedSetMetricsConsent& operator=(const ScopedSetMetricsConsent&) = delete;

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;
};

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

    // Start ModelQualityLogsUploaderService to upload the model quality logs on
    // receiving it from model execution.
    model_quality_logs_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    model_quality_logs_server_->SetSSLConfig(cert_config);
    model_quality_logs_server_->RegisterRequestHandler(base::BindRepeating(
        &ModelExecutionBrowserTestBase::HandleGetModelQualityLogsUploadRequest,
        base::Unretained(this)));

    ASSERT_TRUE(model_execution_server_->Start());
    ASSERT_TRUE(model_quality_logs_server_->Start());
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
    cmd->AppendSwitchASCII(
        switches::kModelQualityServiceURL,
        model_quality_logs_server_
            ->GetURL(
                GURL(kOptimizationGuideServiceModelQualtiyDefaultURL).host(),
                "/")
            .spec());
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(model_execution_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(model_quality_logs_server_->ShutdownAndWaitUntilComplete());
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

  bool IsSignedIn() const {
    return identity_test_env_adaptor_->identity_test_env()
        ->identity_manager()
        ->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  }

  OptimizationGuideKeyedService* GetOptimizationGuideKeyedService(
      Profile* profile = nullptr) {
    if (!profile) {
      profile = browser()->profile();
    }
    return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  }

  // Executes the model for the feature, waits until the response is received,
  // and returns the response.
  void ExecuteModel(UserVisibleFeatureKey feature,
                    const google::protobuf::MessageLite& request_metadata,
                    Profile* profile = nullptr) {
    if (!profile) {
      profile = browser()->profile();
    }
    base::RunLoop run_loop;
    GetOptimizationGuideKeyedService(profile)->ExecuteModel(
        ToModelBasedCapabilityKey(feature), request_metadata,
        base::BindOnce(&ModelExecutionBrowserTestBase::OnModelExecutionResponse,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  bool CanCreateOnDeviceSession(
      ModelBasedCapabilityKey feature,
      OnDeviceModelEligibilityReason* on_device_model_eligibility_reason) {
    return GetOptimizationGuideKeyedService()->CanCreateOnDeviceSession(
        feature, on_device_model_eligibility_reason);
  }

  void SetExpectedBearerAccessToken(
      const std::string& expected_bearer_access_token) {
    expected_bearer_access_token_ = expected_bearer_access_token;
  }

  void SetResponseType(ModelExecutionRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

 protected:
  void OnModelExecutionResponse(
      base::OnceClosure on_model_execution_closure,
      OptimizationGuideModelExecutionResult result,
      std::unique_ptr<ModelQualityLogEntry> log_entry) {
    if (result.has_value() ||
        result.error().error() == OptimizationGuideModelExecutionError::
                                      ModelExecutionError::kFiltered ||
        result.error().error() ==
            OptimizationGuideModelExecutionError::ModelExecutionError::
                kUnsupportedLanguage) {
      EXPECT_TRUE(log_entry);
      proto::LogAiDataRequest* log_ai_data_request =
          log_entry.get()->log_ai_data_request();
      EXPECT_NE(log_ai_data_request, nullptr);
      EXPECT_EQ(log_ai_data_request->feature_case(),
                proto::LogAiDataRequest::FeatureCase::kCompose);
      EXPECT_TRUE(log_ai_data_request->has_compose());
      EXPECT_TRUE(log_ai_data_request->mutable_compose()->has_request());
    }

    if (result.has_value()) {
      EXPECT_TRUE(log_entry.get()
                      ->log_ai_data_request()
                      ->mutable_compose()
                      ->has_response());
      model_execution_result_ = base::ok(result.value());
    } else {
      model_execution_result_ = base::unexpected(result.error());
    }
    ModelQualityLogEntry::Upload(std::move(log_entry));
    std::move(on_model_execution_closure).Run();
  }

  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetModelExecutionRequest(const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
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
          BuildComposeResponse("foo response");
      execute_response.SerializeToString(&serialized_response);
      response->set_code(net::HTTP_OK);
      response->set_content(serialized_response);
    } else if (response_type_ ==
               ModelExecutionRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);
    } else if (response_type_ == ModelExecutionRemoteResponseType::kMalformed) {
      response->set_code(net::HTTP_OK);
      response->set_content("Not a proto");
    } else if (response_type_ ==
               ModelExecutionRemoteResponseType::kErrorFiltered) {
      std::string serialized_response;
      proto::ExecuteResponse execute_response = BuildTestErrorExecuteResponse(
          proto::ErrorState::ERROR_STATE_FILTERED);
      execute_response.SerializeToString(&serialized_response);
      response->set_code(net::HTTP_OK);
      response->set_content(serialized_response);
    } else if (response_type_ ==
               ModelExecutionRemoteResponseType::kUnsupportedLanguage) {
      std::string serialized_response;
      proto::ExecuteResponse execute_response = BuildTestErrorExecuteResponse(
          proto::ErrorState::ERROR_STATE_UNSUPPORTED_LANGUAGE);
      execute_response.SerializeToString(&serialized_response);
      response->set_code(net::HTTP_OK);
      response->set_content(serialized_response);
    } else {
      NOTREACHED_IN_MIGRATION();
    }

    return std::move(response);
  }

  std::unique_ptr<net::test_server::HttpResponse>
  HandleGetModelQualityLogsUploadRequest(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));

    // Access token should be set.
    EXPECT_TRUE(base::Contains(request.headers,
                               net::HttpRequestHeaders::kAuthorization));
    EXPECT_EQ(expected_bearer_access_token_,
              request.headers.at(net::HttpRequestHeaders::kAuthorization));

    std::string serialized_response;
    response->set_code(net::HTTP_OK);
    response->set_content(serialized_response);

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
  std::unique_ptr<net::EmbeddedTestServer> model_quality_logs_server_;
  base::HistogramTester histogram_tester_;

  ModelExecutionRemoteResponseType response_type_ =
      ModelExecutionRemoteResponseType::kSuccessful;

  // The last model execution response received.
  std::optional<OptimizationGuideModelExecutionResult> model_execution_result_;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;

  std::optional<ScopedSetMetricsConsent> scoped_metrics_consent_;

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
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());
}

IN_PROC_BROWSER_TEST_F(ModelExecutionDisabledBrowserTest,
                       CanCreateOnDeviceSessionExecutionDisabled) {
  OnDeviceModelEligibilityReason on_device_model_eligibility_reason;
  EXPECT_FALSE(CanCreateOnDeviceSession(ModelBasedCapabilityKey::kCompose,
                                        &on_device_model_eligibility_reason));
  EXPECT_EQ(on_device_model_eligibility_reason,
            OnDeviceModelEligibilityReason::kFeatureNotEnabled);
}

IN_PROC_BROWSER_TEST_F(
    ModelExecutionDisabledBrowserTest,
    CanCreateOnDeviceSessionExecutionDisabledNullDebugReason) {
  EXPECT_FALSE(
      CanCreateOnDeviceSession(ModelBasedCapabilityKey::kCompose,
                               /*on_device_model_eligibility_reason=*/nullptr));
}

class ModelExecutionEnabledOnDeviceDisabledBrowserTest
    : public ModelExecutionBrowserTestBase {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationGuideModelExecution,
         features::kModelQualityLogging},
        {features::kOptimizationGuideOnDeviceModel});
  }
};

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledOnDeviceDisabledBrowserTest,
                       CanCreateOnDeviceSessionOnDeviceDisabled) {
  OnDeviceModelEligibilityReason on_device_model_eligibility_reason;
  EXPECT_FALSE(CanCreateOnDeviceSession(ModelBasedCapabilityKey::kCompose,
                                        &on_device_model_eligibility_reason));
  EXPECT_EQ(on_device_model_eligibility_reason,
            OnDeviceModelEligibilityReason::kFeatureNotEnabled);
}

IN_PROC_BROWSER_TEST_F(
    ModelExecutionEnabledOnDeviceDisabledBrowserTest,
    CanCreateOnDeviceSessionExecutionDisabledNullDebugReason) {
  EXPECT_FALSE(
      CanCreateOnDeviceSession(ModelBasedCapabilityKey::kCompose,
                               /*on_device_model_eligibility_reason=*/nullptr));
}

class ModelExecutionEnabledBrowserTest : public ModelExecutionBrowserTestBase {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationGuideModelExecution,
         features::kModelQualityLogging,
         features::kOptimizationGuideOnDeviceModel},
        {});
  }

  OptimizationGuideKeyedService* GetOptGuideKeyedService() {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(
        browser()->profile());
  }

  bool IsSettingVisible(UserVisibleFeatureKey feature) {
    return GetOptGuideKeyedService()->IsSettingVisible(feature);
  }

  bool ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey feature) {
    return GetOptGuideKeyedService()
        ->model_execution_features_controller_
        ->ShouldFeatureBeCurrentlyEnabledForUser(feature);
  }

  bool ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase feature) {
    const MqlsFeatureMetadata* metadata =
        MqlsFeatureRegistry::GetInstance().GetFeature(feature);
    return GetOptGuideKeyedService()
        ->model_execution_features_controller_
        ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata);
  }
};

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionDisabledInIncognito) {
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request,
               otr_browser->profile());
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());

  // The logs shouldn't be uploaded because model execution is disabled for
  // incognito and we wouldn't be receiving any log entry.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus", 0);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsNoUserSignIn) {
  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kPermissionDenied,
            model_execution_result_->error().error());
  EXPECT_FALSE(model_execution_result_->error().transient());

  // The logs shouldn't be uploaded because model execution is denied without
  // user signin, also model quality logs.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus", 0);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionSuccess) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_TRUE(model_execution_result_->has_value());
  auto response = ParsedAnyMetadata<proto::ComposeResponse>(
      model_execution_result_->value());
  EXPECT_EQ("foo response", response->output());

  // The logs shouldn't be uploaded because there is no metrics consent.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kMetricsReportingDisabled, 1);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsForUnsuccessfulResponse) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kUnsuccessful);

  // Enable metrics consent for logging.
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());

  // The logs shouldn't be uploaded when model execution fails for unsuccessful
  // response.
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus", 0);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsForMalformedResponse) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kMalformed);

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kGenericFailure,
            model_execution_result_->error().error());
  EXPECT_TRUE(model_execution_result_->error().transient());
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsForErrorFilteredResponse) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kErrorFiltered);

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered,
      model_execution_result_->error().error());
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       ModelExecutionFailsForUnsupportedLanguageResponse) {
  EnableSignin();
  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kCompose),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kUnsupportedLanguage);

  // Enable metrics consent for logging.
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_FALSE(model_execution_result_->has_value());
  EXPECT_EQ(OptimizationGuideModelExecutionError::ModelExecutionError::
                kUnsupportedLanguage,
            model_execution_result_->error().error());

  // There should be no error-status reports about log uploading (we don't try
  // to upload logs in this test, so there's no success report either).
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      0);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnabledBrowserTest,
                       CanCreateOnDeviceSessionNoModelAvailable) {
  OnDeviceModelEligibilityReason on_device_model_eligibility_reason;
  EXPECT_FALSE(CanCreateOnDeviceSession(ModelBasedCapabilityKey::kCompose,
                                        &on_device_model_eligibility_reason));
  EXPECT_EQ(on_device_model_eligibility_reason,
            OnDeviceModelEligibilityReason::kModelNotAvailable);
}

IN_PROC_BROWSER_TEST_F(
    ModelExecutionEnabledBrowserTest,
    CanCreateOnDeviceSessionExecutionDisabledNullDebugReason) {
  EXPECT_FALSE(
      CanCreateOnDeviceSession(ModelBasedCapabilityKey::kCompose,
                               /*on_device_model_eligibility_reason=*/nullptr));
}

class ModelExecutionInternalsPageBrowserTest
    : public ModelExecutionEnabledBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* cmd) override {
    ModelExecutionEnabledBrowserTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch(switches::kDebugLoggingEnabled);
  }
  void CheckInternalsLog(std::string_view message) {
    auto* logger =
        GetOptimizationGuideKeyedService()->GetOptimizationGuideLogger();
    EXPECT_THAT(logger->recent_log_messages_,
                testing::Contains(testing::Field(
                    &OptimizationGuideLogger::LogMessage::message,
                    testing::HasSubstr(message))));
  }
};

IN_PROC_BROWSER_TEST_F(ModelExecutionInternalsPageBrowserTest,
                       LoggedInInternalsPage) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("foo");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_TRUE(model_execution_result_->has_value());
  CheckInternalsLog("ExecuteModel");
  // CheckInternalsLog("TabOrganization Request");
  CheckInternalsLog("OnModelExecutionResponse");
}

class ModelExecutionEnabledBrowserTestWithExplicitBrowserSignin
    : public ModelExecutionEnabledBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        {::switches::kExplicitBrowserSigninUIOnDesktop},
        {features::internal::kTabOrganizationGraduated});
  }
};

IN_PROC_BROWSER_TEST_F(
    ModelExecutionEnabledBrowserTestWithExplicitBrowserSignin,
    PRE_EnableFeatureViaPref) {
  EnableSignin();
  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kTabOrganization),
      static_cast<int>(prefs::FeatureOptInState::kDisabled));

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup.Compose", false,
      1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "TabOrganization",
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "WallpaperSearch",
      false, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtSettingsChange.Compose",
      0);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtSettingsChange."
      "TabOrganization",
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtSettingsChange."
      "WallpaperSearch",
      true, 1);
}

IN_PROC_BROWSER_TEST_F(
    ModelExecutionEnabledBrowserTestWithExplicitBrowserSignin,
    EnableFeatureViaPref) {
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsSignedIn());
#endif
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup.Compose", false,
      1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "TabOrganization",
      false, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtStartup."
      "WallpaperSearch",
      false, 1);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtSettingsChange.Compose",
      0);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtSettingsChange."
      "TabOrganization",
      0);
  histogram_tester_.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.FeatureEnabledAtSettingsChange."
      "WallpaperSearch",
      0);
}

IN_PROC_BROWSER_TEST_F(
    ModelExecutionEnabledBrowserTestWithExplicitBrowserSignin,
    PRE_HistorySearchRecordsSyntheticFieldTrial) {
  EnableSignin();
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kHistorySearch));

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kHistorySearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      "SyntheticModelExecutionFeatureHistorySearch", "Disabled"));
}

IN_PROC_BROWSER_TEST_F(
    ModelExecutionEnabledBrowserTestWithExplicitBrowserSignin,
    HistorySearchRecordsSyntheticFieldTrial) {
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsSignedIn());
#endif
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kHistorySearch));
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      "SyntheticModelExecutionFeatureHistorySearch", "Enabled"));
}

class ModelExecutionComposeLoggingDisabledTest
    : public ModelExecutionEnabledBrowserTest {
 public:
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kModelQualityLogging, {}}},
        {features::kComposeMqlsLogging});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ModelExecutionComposeLoggingDisabledTest,
                       LoggingForFeatureNotEnabled) {
  EnableSignin();
  SetExpectedBearerAccessToken("Bearer access_token");

  // Enable metrics consent for logging.
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request);
  EXPECT_TRUE(model_execution_result_.has_value());
  EXPECT_TRUE(model_execution_result_->has_value());
  auto response = ParsedAnyMetadata<proto::ComposeResponse>(
      model_execution_result_->value());
  EXPECT_EQ("foo response", response->output());

  // The logs shouldn't be uploaded because the feature is not enabled for
  // logging.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kLoggingNotEnabled, 1);
}

class ModelExecutionNewFeaturesEnabledAutomaticallyTest
    : public ModelExecutionEnabledBrowserTest {
 public:
  void InitializeFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kOptimizationGuideModelExecution, {}},
        {features::internal::kTabOrganizationSettingsVisibility, {}}};
    std::vector<base::test::FeatureRef> disabled_features = {
        features::internal::kTabOrganizationGraduated,
        features::internal::kComposeGraduated};

    std::string test_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    // Make the new feature visible in the second start of the test.
    if (!base::StartsWith(test_name, "PRE_")) {
      enabled_features.push_back(
          {features::internal::kComposeSettingsVisibility, {}});
      enabled_features.push_back(
          {features::internal::kHistorySearchSettingsVisibility,
           {{"enable_feature_when_main_toggle_on", "false"}}});
    }
    enabled_features.push_back(
        {::switches::kExplicitBrowserSigninUIOnDesktop, {}});

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
};

IN_PROC_BROWSER_TEST_F(ModelExecutionNewFeaturesEnabledAutomaticallyTest,
                       PRE_NewFeaturesEnabledWhenMainToggleEnabled) {
  EnableSignin();
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kHistorySearch));

  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kModelExecutionMainToggleSettingState,
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));
  EXPECT_FALSE(
      ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey::kCompose));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kHistorySearch));
}

IN_PROC_BROWSER_TEST_F(ModelExecutionNewFeaturesEnabledAutomaticallyTest,
                       NewFeaturesEnabledWhenMainToggleEnabled) {
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(IsSignedIn());
#endif
  // The new feature should have got enabled since main toggle was on.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(
      ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey::kCompose));
  // The feature with automatic enabling disallowed should be visible but not
  // enabled.
#if BUILDFLAG(BUILD_TFLITE_WITH_XNNPACK)
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kHistorySearch));
#else
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kHistorySearch));
#endif
  EXPECT_FALSE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kHistorySearch));
}

#if !BUILDFLAG(IS_ANDROID)

class ModelExecutionEnterprisePolicyBrowserTest
    : public ModelExecutionEnabledBrowserTest {
 public:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    ModelExecutionEnabledBrowserTest::SetUp();
  }

  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kOptimizationGuideModelExecution,
         features::kModelQualityLogging,
         features::internal::kTabOrganizationSettingsVisibility,
         features::internal::kWallpaperSearchSettingsVisibility},
        /*disabled_features=*/
        {features::internal::kComposeGraduated,
         features::internal::kComposeSettingsVisibility,
         features::internal::kTabOrganizationGraduated,
         features::internal::kWallpaperSearchGraduated});
  }

 protected:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(ModelExecutionEnterprisePolicyBrowserTest,
                       DisableThenEnable) {
  EnableSignin();

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kTabOrganization),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  // Default policy value allows the feature.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kTabOrganization));
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kCompose));

  // Disable via the enterprise policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kTabOrganizerSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kDisable)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kCompose));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kTabOrganization));

  // Enable via the enterprise policy.
  policies.Set(
      policy::key::kTabOrganizerSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kTabOrganization),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kTabOrganization));
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kCompose));
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnterprisePolicyBrowserTest,
                       DisableThenEnableCompose) {
  EnableSignin();

  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kUnsupportedLanguage);

  // Enable metrics consent for logging.
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kCompose),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  // Default policy value allows the feature.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_TRUE(
      ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey::kCompose));

  // Disable via the enterprise policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kHelpMeWriteSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kDisable)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_FALSE(
      ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey::kCompose));

  proto::ComposeRequest request_1;
  request_1.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request_1);

  // As the feature is fully disabled by enterprise policy, logs should also be
  // disabled.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      optimization_guide::ModelQualityLogsUploadStatus::
          kFeatureNotEnabledForUser,
      1);

  // Enable via the enterprise policy and check upload.
  policies.Set(
      policy::key::kHelpMeWriteSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kCompose),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_TRUE(
      ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey::kCompose));

  proto::ComposeRequest request_2;
  request_2.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request_2);

  // No new blocked logs samples should have been recorded.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      optimization_guide::ModelQualityLogsUploadStatus::
          kFeatureNotEnabledForUser,
      1);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnterprisePolicyBrowserTest,
                       EnableComposeWithoutLogging) {
  EnableSignin();

  SetExpectedBearerAccessToken("Bearer access_token");
  SetResponseType(ModelExecutionRemoteResponseType::kUnsupportedLanguage);

  // Enable metrics consent for logging.
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kCompose),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  // Enable without logging via the enterprise policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kHelpMeWriteSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kAllowWithoutLogging)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_TRUE(
      ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey::kCompose));

  proto::ComposeRequest request_1;
  request_1.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request_1);

  // The logs should be disabled via enterprise policy.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kDisabledDueToEnterprisePolicy, 1);

  // Enable via the enterprise policy and check upload.
  policies.Set(
      policy::key::kHelpMeWriteSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kCompose),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_TRUE(
      ShouldFeatureBeCurrentlyEnabledForUser(UserVisibleFeatureKey::kCompose));

  proto::ComposeRequest request_2;
  request_2.mutable_generate_params()->set_user_input("a user typed this");
  ExecuteModel(UserVisibleFeatureKey::kCompose, request_2);

  // No new blocked logs samples should have been recorded.
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      optimization_guide::ModelQualityLogsUploadStatus::
          kDisabledDueToEnterprisePolicy,
      1);
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnterprisePolicyBrowserTest,
                       DisableThenEnableWallpaperSearch) {
  EnableSignin();

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  // Default policy value allows the feature.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  // Disable via the enterprise policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kCreateThemesSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kDisable)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  // Enable via the enterprise policy.
  policies.Set(
      policy::key::kCreateThemesSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));
}

IN_PROC_BROWSER_TEST_F(ModelExecutionEnterprisePolicyBrowserTest,
                       EnableTabOrganizationWithoutLogging) {
  EnableSignin();

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kTabOrganization),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  // EnableWithoutLogging via the enterprise policy.
  policy::PolicyMap policies;
  policies.Set(policy::key::kTabOrganizerSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kAllowWithoutLogging)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kTabOrganization),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
  EXPECT_TRUE(ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kTabOrganization));
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
  EXPECT_FALSE(ShouldFeatureBeCurrentlyAllowedForLogging(
      proto::LogAiDataRequest::FeatureCase::kCompose));
}

#endif  //  !BUILDFLAG(IS_ANDROID)

}  // namespace optimization_guide
