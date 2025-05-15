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

mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
AITestUtils::FakeMonitor::BindNewPipeAndPassRemote() {
  return mock_monitor_.BindNewPipeAndPassRemote();
}

void AITestUtils::FakeMonitor::ExpectReceivedUpdate(
    uint64_t expected_downloaded_bytes,
    uint64_t expected_total_bytes) {
  base::RunLoop download_progress_run_loop;
  EXPECT_CALL(mock_monitor_, OnDownloadProgressUpdate(testing::_, testing::_))
      .WillOnce(
          testing::Invoke([&](uint64_t downloaded_bytes, uint64_t total_bytes) {
            EXPECT_EQ(downloaded_bytes, expected_downloaded_bytes);
            EXPECT_EQ(total_bytes, expected_total_bytes);
            download_progress_run_loop.Quit();
          }));
  download_progress_run_loop.Run();
}

void AITestUtils::FakeMonitor::ExpectReceivedNormalizedUpdate(
    uint64_t expected_downloaded_bytes,
    uint64_t expected_total_bytes) {
  ExpectReceivedUpdate(AIUtils::NormalizeModelDownloadProgress(
                           expected_downloaded_bytes, expected_total_bytes),
                       AIUtils::kNormalizedDownloadProgressMax);
}

void AITestUtils::FakeMonitor::ExpectNoUpdate() {
  EXPECT_CALL(mock_monitor_, OnDownloadProgressUpdate(testing::_, testing::_))
      .Times(0);
}

AITestUtils::FakeComponent::FakeComponent(std::string id, uint64_t total_bytes)
    : id_(std::move(id)), total_bytes_(total_bytes) {}

component_updater::CrxUpdateItem AITestUtils::FakeComponent::CreateUpdateItem(
    update_client::ComponentState state,
    uint64_t downloaded_bytes) const {
  component_updater::CrxUpdateItem update_item;
  update_item.state = state;
  update_item.id = id_;
  update_item.downloaded_bytes = downloaded_bytes;
  update_item.total_bytes = total_bytes_;
  return update_item;
}

AITestUtils::MockComponentUpdateService::MockComponentUpdateService() = default;
AITestUtils::MockComponentUpdateService::~MockComponentUpdateService() =
    default;

void AITestUtils::MockComponentUpdateService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AITestUtils::MockComponentUpdateService::RemoveObserver(
    Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AITestUtils::MockComponentUpdateService::SendUpdate(
    const component_updater::CrxUpdateItem& item) {
  for (Observer& observer : observer_list_) {
    observer.OnEvent(item);
  }
}

AITestUtils::AITestBase::AITestBase() = default;
AITestUtils::AITestBase::~AITestBase() = default;

void AITestUtils::AITestBase::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  ai_manager_ = std::make_unique<AIManager>(
      main_rfh()->GetBrowserContext(), &component_update_service_, main_rfh());
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
  ON_CALL(*mock_optimization_guide_keyed_service_,
          GetOnDeviceModelEligibilityAsync(testing::_, testing::_, testing::_))
      .WillByDefault([](auto feature, auto capabilities, auto callback) {
        std::move(callback).Run(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
      });
}

void AITestUtils::AITestBase::SetupNullOptimizationGuideKeyedService() {
  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(), base::BindRepeating(
                     [](content::BrowserContext* context)
                         -> std::unique_ptr<KeyedService> { return nullptr; }));
}

void AITestUtils::AITestBase::SetupMockSession() {
  ON_CALL(*mock_optimization_guide_keyed_service_,
          StartSession(testing::_, testing::_))
      .WillByDefault([&] {
        return std::make_unique<
            testing::NiceMock<optimization_guide::MockSession>>(&session_);
      });
  ON_CALL(session_, GetExecutionInputSizeInTokens(testing::_, testing::_))
      .WillByDefault(
          [&](optimization_guide::MultimodalMessageReadView request_metadata,
              optimization_guide::OptimizationGuideModelSizeInTokenCallback
                  callback) {
            std::move(callback).Run(
                blink::mojom::kWritingAssistanceMaxInputTokenSize);
          });
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
