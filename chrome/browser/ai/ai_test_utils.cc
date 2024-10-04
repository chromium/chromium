// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_test_utils.h"

#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"

AITestUtils::MockModelStreamingResponder::MockModelStreamingResponder() =
    default;
AITestUtils::MockModelStreamingResponder::~MockModelStreamingResponder() =
    default;

mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
AITestUtils::MockModelStreamingResponder::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

AITestUtils::MockCreateAssistantClient::MockCreateAssistantClient() = default;
AITestUtils::MockCreateAssistantClient::~MockCreateAssistantClient() = default;

mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient>
AITestUtils::MockCreateAssistantClient::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

AITestUtils::AITestBase::AITestBase() = default;
AITestUtils::AITestBase::~AITestBase() = default;

void AITestUtils::AITestBase::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  mock_host_ = std::make_unique<MockSupportsUserData>();
}

void AITestUtils::AITestBase::TearDown() {
  mock_optimization_guide_keyed_service_ = nullptr;
  mock_host_.reset();
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
}

void AITestUtils::AITestBase::SetupNullOptimizationGuideKeyedService() {
  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));
}

mojo::Remote<blink::mojom::AIManager>
AITestUtils::AITestBase::GetAIManagerRemote() {
  AIManagerKeyedService* ai_manager_keyed_service =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          main_rfh()->GetBrowserContext());
  mojo::Remote<blink::mojom::AIManager> ai_manager;
  ai_manager_keyed_service->AddReceiver(ai_manager.BindNewPipeAndPassReceiver(),
                                        mock_host_.get());
  return ai_manager;
}

size_t AITestUtils::AITestBase::GetAIManagerReceiversSize() {
  AIManagerKeyedService* ai_manager_keyed_service =
      AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
          main_rfh()->GetBrowserContext());

  return ai_manager_keyed_service->GetReceiversSizeForTesting();
}

void AITestUtils::AITestBase::ResetMockHost() {
  mock_host_.reset();
}

// static
std::string AITestUtils::GetTypeURLForProto(std::string type_name) {
  return "type.googleapis.com/" + type_name;
}

// static
const optimization_guide::TokenLimits& AITestUtils::GetFakeTokenLimits() {
  static const optimization_guide::TokenLimits limits{
      .max_tokens = 4096,
      .max_context_tokens = 2048,
      .max_execute_tokens = 1024,
      .max_output_tokens = 1024,
  };
  return limits;
}

// static
const optimization_guide::proto::Any& AITestUtils::GetFakeFeatureMetadata() {
  static base::NoDestructor<optimization_guide::proto::Any> data;
  return *data;
}
