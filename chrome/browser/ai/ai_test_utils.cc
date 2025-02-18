// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_test_utils.h"

#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"

AITestUtils::MockModelStreamingResponder::MockModelStreamingResponder() =
    default;
AITestUtils::MockModelStreamingResponder::~MockModelStreamingResponder() =
    default;

mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
AITestUtils::MockModelStreamingResponder::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

AITestUtils::MockModelDownloadProgressMonitor::
    MockModelDownloadProgressMonitor() = default;
AITestUtils::MockModelDownloadProgressMonitor::
    ~MockModelDownloadProgressMonitor() = default;

mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
AITestUtils::MockModelDownloadProgressMonitor::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

AITestUtils::MockCreateLanguageModelClient::MockCreateLanguageModelClient() =
    default;
AITestUtils::MockCreateLanguageModelClient::~MockCreateLanguageModelClient() =
    default;

mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
AITestUtils::MockCreateLanguageModelClient::BindNewPipeAndPassRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

AITestUtils::AITestBase::AITestBase() = default;
AITestUtils::AITestBase::~AITestBase() = default;

void AITestUtils::AITestBase::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ai_manager_ = std::make_unique<AIManager>(main_rfh()->GetBrowserContext());
}

void AITestUtils::AITestBase::TearDown() {
  mock_optimization_guide_keyed_service_ = nullptr;
  ai_manager_.reset();
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

blink::mojom::AIManager* AITestUtils::AITestBase::GetAIManagerInterface() {
  return ai_manager_.get();
}

mojo::Remote<blink::mojom::AIManager>
AITestUtils::AITestBase::GetAIManagerRemote() {
  mojo::Remote<blink::mojom::AIManager> ai_manager;
  ai_manager_->AddReceiver(ai_manager.BindNewPipeAndPassReceiver());
  return ai_manager;
}

size_t AITestUtils::AITestBase::GetAIManagerDownloadProgressObserversSize() {
  return ai_manager_->GetDownloadProgressObserversSizeForTesting();
}

size_t AITestUtils::AITestBase::GetAIManagerContextBoundObjectSetSize() {
  return ai_manager_->GetContextBoundObjectSetSizeForTesting();
}

void AITestUtils::AITestBase::MockDownloadProgressUpdate(
    uint64_t downloaded_bytes,
    uint64_t total_bytes) {
  ai_manager_->SendDownloadProgressUpdateForTesting(downloaded_bytes,
                                                    total_bytes);
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

// static
void AITestUtils::CheckWritingAssistanceApiRequest(
    const google::protobuf::MessageLite& request_metadata,
    const std::string& expected_shared_context,
    const std::string& expected_context,
    const optimization_guide::proto::WritingAssistanceApiOptions&
        expected_options,
    const std::string& expected_input) {
  const optimization_guide::proto::WritingAssistanceApiRequest* request =
      static_cast<
          const optimization_guide::proto::WritingAssistanceApiRequest*>(
          &request_metadata);
  EXPECT_EQ(request->shared_context(), expected_shared_context);
  EXPECT_EQ(request->context(), expected_context);
  EXPECT_EQ(request->options().output_tone(), expected_options.output_tone());
  EXPECT_EQ(request->options().output_format(),
            expected_options.output_format());
  EXPECT_EQ(request->options().output_length(),
            expected_options.output_length());
  EXPECT_EQ(request->rewrite_text(), expected_input);
}

// static
void AITestUtils::CheckSummarizeRequest(
    const google::protobuf::MessageLite& request_metadata,
    const std::string& expected_shared_context,
    const std::string& expected_context,
    const optimization_guide::proto::SummarizeOptions& expected_options,
    const std::string& expected_input) {
  const optimization_guide::proto::SummarizeRequest* request =
      static_cast<const optimization_guide::proto::SummarizeRequest*>(
          &request_metadata);
  EXPECT_EQ(request->context(), AISummarizer::CombineContexts(
                                    expected_shared_context, expected_context));
  EXPECT_EQ(request->options().output_type(), expected_options.output_type());
  EXPECT_EQ(request->options().output_format(),
            expected_options.output_format());
  EXPECT_EQ(request->options().output_length(),
            expected_options.output_length());
  EXPECT_EQ(request->article(), expected_input);
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
