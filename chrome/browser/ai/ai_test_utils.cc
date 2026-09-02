// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_test_utils.h"

#include <cstdint>
#include <utility>

#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/model_execution/optimization_guide_global_state.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/feature_configs.pb.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/navigation_simulator.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace {

using ::optimization_guide::proto::BaseModelRecipe;
using ::optimization_guide::proto::PromptApiFeatureConfig;
using ::optimization_guide::proto::SolutionConfig;
using ::optimization_guide::proto::SummarizerFeatureConfig;
using ::optimization_guide::proto::WritingAssistanceApiFeatureConfig;

}  // namespace



AITestUtils::TestStreamingResponder::TestStreamingResponder() = default;
AITestUtils::TestStreamingResponder::~TestStreamingResponder() = default;

mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
AITestUtils::TestStreamingResponder::BindRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

bool AITestUtils::TestStreamingResponder::WaitForCompletion() {
  run_loop_.Run();
  return !error_status_.has_value();
}

bool AITestUtils::TestStreamingResponder::WaitForToolCalls() {
  tool_calls_run_loop_.Run();
  return !tool_calls_.empty();
}

void AITestUtils::TestStreamingResponder::WaitForContextOverflow() {
  context_overflow_run_loop_.Run();
}

void AITestUtils::TestStreamingResponder::OnError(
    blink::mojom::ModelStreamingResponseStatus status,
    blink::mojom::QuotaErrorInfoPtr quota_error_info) {
  error_status_ = status;
  quota_error_info_ = std::move(quota_error_info);
  run_loop_.Quit();
}

void AITestUtils::TestStreamingResponder::OnStreaming(const std::string& text) {
  responses_.push_back(text);
}

void AITestUtils::TestStreamingResponder::OnCompletion(
    blink::mojom::ModelExecutionContextInfoPtr context_info) {
  if (context_info) {
    current_tokens_ = context_info->current_tokens;
  }
  run_loop_.Quit();
}

void AITestUtils::TestStreamingResponder::OnToolCalls(
    std::vector<blink::mojom::ToolCallPtr> tool_calls) {
  tool_calls_ = std::move(tool_calls);
  tool_calls_run_loop_.Quit();
}

void AITestUtils::TestStreamingResponder::OnContextOverflow() {
  context_overflow_run_loop_.Quit();
}

AITestUtils::AITestBase::AITestBase()
    : ChromeRenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
#if BUILDFLAG(IS_ANDROID)
  scoped_feature_list_.InitAndDisableFeature(
      optimization_guide::kOptimizationGuideManifestBroker);
#endif
}
AITestUtils::AITestBase::~AITestBase() = default;

#if BUILDFLAG(IS_ANDROID)
void AITestUtils::AITestBase::SetupBroker() {
  fake_broker_ = std::make_unique<optimization_guide::FakeModelBrokerAndroid>(
      optimization_guide::FakeModelBrokerAndroid::Options{});
  fake_broker_->java_helper().settings().SetDefaultStatusCheckResult(
      on_device_model::ModelDownloaderAndroid::ModelStatus::kDownloadable);
  auto asset = std::make_unique<optimization_guide::FakeAdaptationAsset>(
      optimization_guide::FakeAdaptationAsset::Content{
          .config = CreateSolution().feature(),
      });
  fake_broker_->UpdateModelAdaptation(*asset);
  fake_assets_.push_back(std::move(asset));
}

void AITestUtils::AITestBase::SetSolutionConfig(
    SolutionConfig solution_config) {
  auto asset = std::make_unique<optimization_guide::FakeAdaptationAsset>(
      optimization_guide::FakeAdaptationAsset::Content{
          .config = std::move(*solution_config.mutable_feature()),
      });
  fake_broker_->UpdateModelAdaptation(*asset);
  fake_assets_.push_back(std::move(asset));
}
#else
namespace {

std::string GetUseCaseForSolutionConfig(const SolutionConfig& solution_config) {
  auto feature = optimization_guide::ToOnDeviceFeature(
      solution_config.feature().feature());
  CHECK(feature.has_value());
  return optimization_guide::ToUseCaseName(*feature);
}

void SetupScenario(
    optimization_guide::TestManifestAssetManagerComponentState& component_state,
    SolutionConfig solution_config) {
  std::string use_case = GetUseCaseForSolutionConfig(solution_config);

  // Explicit BaseModelRecipeArgs and empty FakeBaseModelAsset::Content are
  // needed: ScenarioBuilder::AddBaseModel(name) defaults to 100 max_tokens and
  // non-empty cache weights (1015, 1016, 1017), which causes FakeOnDeviceModel
  // to emit dummy cache weight response chunks that break response assertions.
  constexpr uint32_t kDefaultMaxTokens = 8096;
  auto builder = optimization_guide::ScenarioBuilder(component_state);
  builder
      .AddBaseModel(
          "base",
          optimization_guide::BaseModelRecipeArgs(
              BaseModelRecipe::BACKEND_TYPE_GPU,
              BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY, {},
              kDefaultMaxTokens),
          optimization_guide::FakeBaseModelAsset::Content{}, "1.0.0.0")
      .AddSafetyModel("safety")
      .AddSafeSolution(use_case, "base", "safety", std::move(solution_config));

  if (use_case == "prompt_api") {
    PromptApiFeatureConfig prompt_api_cfg;
    prompt_api_cfg.set_default_use_case("prompt_api");
    builder.SetFeatureConfig("prompt_api",
                             optimization_guide::AnyWrapProto(prompt_api_cfg));
  } else if (use_case == "writing_assistance_api") {
    WritingAssistanceApiFeatureConfig writer_cfg;
    writer_cfg.set_default_use_case("writing_assistance_api");
    builder.SetFeatureConfig("writing_assistance_api",
                             optimization_guide::AnyWrapProto(writer_cfg));
  } else if (use_case == "summarizer_api") {
    SummarizerFeatureConfig summarizer_cfg;
    summarizer_cfg.set_default_use_case("summarizer_api");
    builder.SetFeatureConfig("summarizer_api",
                             optimization_guide::AnyWrapProto(summarizer_cfg));
  }

  builder.Finish();
}

}  // namespace

void AITestUtils::AITestBase::SetupBroker() {
  fake_broker_ = std::make_unique<optimization_guide::FakeManifestBroker>();
  SetupScenario(fake_broker_->component_state(), CreateSolution());
  fake_broker_->settings().performance_class =
      on_device_model::mojom::PerformanceClass::kHigh;
  fake_broker_->Startup();
}

void AITestUtils::AITestBase::SetSolutionConfig(
    SolutionConfig solution_config) {
  SetupScenario(fake_broker_->component_state(), std::move(solution_config));
  fake_broker_->SimulateShutdown();
  fake_broker_->Startup();
  fake_broker_->settings().performance_class =
      on_device_model::mojom::PerformanceClass::kHigh;
  ai_manager_ =
      std::make_unique<AIManager>(main_rfh()->GetBrowserContext(), main_rfh());
}
#endif  // !BUILDFLAG(IS_ANDROID)

void AITestUtils::AITestBase::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  SetupBroker();

  SetupMockOptimizationGuideKeyedService();
  ai_manager_ =
      std::make_unique<AIManager>(main_rfh()->GetBrowserContext(), main_rfh());
}

void AITestUtils::AITestBase::TearDown() {
  mock_optimization_guide_keyed_service_ = nullptr;
  ai_manager_.reset();
  fake_broker_.reset();
#if BUILDFLAG(IS_ANDROID)
  fake_assets_.clear();
#endif
  ChromeRenderViewHostTestHarness::TearDown();
}

void AITestUtils::AITestBase::SetModelInputContextLimit(
    uint32_t max_input_tokens) {
  auto solution_config = CreateSolution();
  solution_config.mutable_feature()
      ->mutable_input_config()
      ->set_max_execute_tokens(max_input_tokens);
  SetSolutionConfig(std::move(solution_config));
}

void AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService() {
  mock_optimization_guide_keyed_service_ =
      static_cast<MockOptimizationGuideKeyedService*>(
          OptimizationGuideKeyedServiceFactory::GetInstance()
              ->SetTestingFactoryAndUse(
                  profile(),
                  base::BindRepeating([](content::BrowserContext* context)
                                          -> std::unique_ptr<KeyedService> {
                    return std::make_unique<
                        testing::NiceMock<MockOptimizationGuideKeyedService>>();
                  })));
  ON_CALL(*mock_optimization_guide_keyed_service_, CreateModelBrokerClient())
      .WillByDefault([&]() {
#if BUILDFLAG(IS_ANDROID)
        return std::make_unique<optimization_guide::ModelBrokerClient>(
            fake_broker_->BindAndPassRemote(), nullptr);
#else
        return std::make_unique<optimization_guide::ModelBrokerClient>(
            fake_broker_->state().BindAndPassRemoteBroker(), nullptr);
#endif
      });
}

void AITestUtils::AITestBase::SetupNullOptimizationGuideKeyedService() {
  mock_optimization_guide_keyed_service_ = nullptr;
  ai_manager_.reset();

  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));
  ai_manager_ =
      std::make_unique<AIManager>(main_rfh()->GetBrowserContext(), main_rfh());
}

blink::mojom::AIManager* AITestUtils::AITestBase::GetAIManagerInterface() {
  return ai_manager_.get();
}

mojo::Remote<blink::mojom::AIManager>
AITestUtils::AITestBase::GetAIManagerRemote() {
  mojo::Remote<blink::mojom::AIManager> ai_manager;
  ai_manager_->AddReceiver(ai_manager.BindNewPipeAndPassReceiver());
  return ai_manager;
}

size_t AITestUtils::AITestBase::GetAIManagerContextBoundObjectSetSize() {
  return ai_manager_->GetContextBoundObjectSetSizeForTesting();
}

void AITestUtils::AITestBase::DisablePolicy(
    network::mojom::PermissionsPolicyFeature feature) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.com"), main_rfh());
  navigation->SetPermissionsPolicyHeader(
      {{feature, /*allowed_origins=*/{}, /*self_if_matches=*/std::nullopt,
        /*matches_all_origins=*/false, /*matches_opaque_src=*/false}});
  navigation->Commit();

  // Re-create AIManager as it's bound to the RFH.
  ai_manager_ = std::make_unique<AIManager>(
      navigation->GetFinalRenderFrameHost()->GetBrowserContext(),
      navigation->GetFinalRenderFrameHost());
}

void AITestUtils::AITestBase::InstallBaseModel() {
#if BUILDFLAG(IS_ANDROID)
  fake_broker_->InstallBaseModel();
#else
  auto asset = std::make_unique<optimization_guide::FakeBaseModelAsset>();
  asset->set_version("1.0.0.0");
  fake_broker_->component_state().UpdateBaseModel("base_key", std::move(asset));
#endif  // BUILDFLAG(IS_ANDROID)
}

void AITestUtils::AITestBase::UnInstallBaseModel() {
#if BUILDFLAG(IS_ANDROID)
  fake_broker_->UnInstallBaseModel();
#else
  fake_broker_->component_state().Uninstall("base_key");
#endif  // BUILDFLAG(IS_ANDROID)
}

void AITestUtils::AITestBase::SetSizeInTokens(uint32_t size) {
#if BUILDFLAG(IS_ANDROID)
  fake_broker_->java_helper().settings().SetSizeInTokens(size);
#else
  fake_broker_->settings().set_size_in_tokens(size);
#endif
}

void AITestUtils::AITestBase::SetExecuteResult(
    const std::vector<std::string>& result) {
#if BUILDFLAG(IS_ANDROID)
  fake_broker_->java_helper().settings().SetExecuteResult(result);
#else
  fake_broker_->settings().set_execute_result(result);
#endif
}

void AITestUtils::AITestBase::SetBuiltInAIAPIsEnterprisePolicy(bool allowed) {
  profile()->GetPrefs()->SetBoolean(policy::policy_prefs::kBuiltInAIAPIsEnabled,
                                    allowed);
}

void AITestUtils::AITestBase::SetGenAILocalEnterprisePolicy(bool allowed) {
  g_browser_process->local_state()->SetInteger(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      allowed ? 0 : 1);
}

void AITestUtils::AITestBase::SetOnDeviceAiUserSetting(bool allowed) {
  g_browser_process->local_state()->SetBoolean(
      optimization_guide::model_execution::prefs::localstate::
          kOnDeviceAiUserSettingsEnabled,
      allowed);
}

// static
std::vector<blink::mojom::AILanguageCodePtr> AITestUtils::ToMojoLanguageCodes(
    const std::vector<std::string>& language_codes) {
  std::vector<blink::mojom::AILanguageCodePtr> result;
  result.reserve(language_codes.size());
  std::ranges::transform(
      language_codes, std::back_inserter(result),
      [](const std::string& language_code) {
        return blink::mojom::AILanguageCode::New(language_code);
      });
  return result;
}
