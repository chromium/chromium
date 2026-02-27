// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_test_utils.h"

#include <cstdint>
#include <utility>

#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"

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

void AITestUtils::TestStreamingResponder::OnContextOverflow() {
  context_overflow_run_loop_.Quit();
}

AITestUtils::AITestBase::AITestBase()
    : ChromeRenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
AITestUtils::AITestBase::~AITestBase() = default;

void AITestUtils::AITestBase::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  optimization_guide::FakeModelBroker::Options options{
      .performance_class =
          optimization_guide::OnDeviceModelPerformanceClass::kUnknown};
  fake_broker_ = std::make_unique<optimization_guide::FakeModelBroker>(options);
  optimization_guide::FakeAdaptationAsset::Content content{.config =
                                                               CreateConfig()};
  fake_asset_ = std::make_unique<optimization_guide::FakeAdaptationAsset>(
      std::move(content));
  fake_broker_->UpdateModelAdaptation(*fake_asset_);

  SetupMockOptimizationGuideKeyedService();
  ai_manager_ =
      std::make_unique<AIManager>(main_rfh()->GetBrowserContext(), main_rfh());
}

void AITestUtils::AITestBase::TearDown() {
  mock_optimization_guide_keyed_service_ = nullptr;
  ai_manager_.reset();
  fake_broker_.reset();
  fake_asset_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
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
  ON_CALL(*mock_optimization_guide_keyed_service_,
          GetOnDeviceModelEligibilityAsync(testing::_, testing::_, testing::_))
      .WillByDefault([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
  ON_CALL(*mock_optimization_guide_keyed_service_, CreateModelBrokerClient())
      .WillByDefault([&]() {
        return std::make_unique<optimization_guide::ModelBrokerClient>(
            fake_broker_->BindAndPassRemote(), nullptr);
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
