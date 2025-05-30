// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEST_UTILS_H_
#define CHROME_BROWSER_AI_AI_TEST_UTILS_H_

#include "base/supports_user_data.h"
#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/update_client/crx_update_item.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

class AITestUtils {
 public:
  class MockModelStreamingResponder
      : public blink::mojom::ModelStreamingResponder {
   public:
    MockModelStreamingResponder();
    ~MockModelStreamingResponder() override;
    MockModelStreamingResponder(const MockModelStreamingResponder&) = delete;
    MockModelStreamingResponder& operator=(const MockModelStreamingResponder&) =
        delete;

    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
    BindNewPipeAndPassRemote();

    MOCK_METHOD(void, OnStreaming, (const std::string& text), (override));
    MOCK_METHOD(void,
                OnError,
                (blink::mojom::ModelStreamingResponseStatus status,
                 blink::mojom::QuotaErrorInfoPtr quota_error_info),
                (override));
    MOCK_METHOD(void,
                OnCompletion,
                (blink::mojom::ModelExecutionContextInfoPtr context_info),
                (override));
    MOCK_METHOD(void, OnQuotaOverflow, (), (override));

   private:
    mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
  };

  class MockModelDownloadProgressMonitor
      : public blink::mojom::ModelDownloadProgressObserver {
   public:
    MockModelDownloadProgressMonitor();
    ~MockModelDownloadProgressMonitor() override;
    MockModelDownloadProgressMonitor(const MockModelDownloadProgressMonitor&) =
        delete;
    MockModelDownloadProgressMonitor& operator=(
        const MockModelDownloadProgressMonitor&) = delete;

    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
    BindNewPipeAndPassRemote();

    // `blink::mojom::ModelDownloadProgressObserver` implementation.
    MOCK_METHOD(void,
                OnDownloadProgressUpdate,
                (uint64_t downloaded_bytes, uint64_t total_bytes),
                (override));

   private:
    mojo::Receiver<blink::mojom::ModelDownloadProgressObserver> receiver_{this};
  };

  class MockCreateLanguageModelClient
      : public blink::mojom::AIManagerCreateLanguageModelClient {
   public:
    MockCreateLanguageModelClient();
    ~MockCreateLanguageModelClient() override;
    MockCreateLanguageModelClient(const MockCreateLanguageModelClient&) =
        delete;
    MockCreateLanguageModelClient& operator=(
        const MockCreateLanguageModelClient&) = delete;

    mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
    BindNewPipeAndPassRemote();

    MOCK_METHOD(
        void,
        OnResult,
        (mojo::PendingRemote<blink::mojom::AILanguageModel> language_model,
         blink::mojom::AILanguageModelInstanceInfoPtr info),
        (override));

    MOCK_METHOD(void,
                OnError,
                (blink::mojom::AIManagerCreateClientError error,
                 blink::mojom::QuotaErrorInfoPtr quota_error_info),
                (override));

   private:
    mojo::Receiver<blink::mojom::AIManagerCreateLanguageModelClient> receiver_{
        this};
  };

  class FakeMonitor {
   public:
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
    BindNewPipeAndPassRemote();

    void ExpectReceivedUpdate(uint64_t expected_downloaded_bytes,
                              uint64_t expected_total_bytes);

    // Same as `ExpectReceivedUpdate` except it normalizes
    // `expected_downloaded_bytes` and `expected_total_bytes`.
    void ExpectReceivedNormalizedUpdate(uint64_t expected_downloaded_bytes,
                                        uint64_t expected_total_bytes);

    void ExpectNoUpdate();

   private:
    AITestUtils::MockModelDownloadProgressMonitor mock_monitor_;
  };

  class FakeComponent {
   public:
    FakeComponent(std::string id, uint64_t total_bytes);

    component_updater::CrxUpdateItem CreateUpdateItem(
        update_client::ComponentState state,
        uint64_t downloaded_bytes) const;

    const std::string& id() { return id_; }
    uint64_t total_bytes() { return total_bytes_; }

   private:
    std::string id_;
    uint64_t total_bytes_;
  };

  class MockComponentUpdateService
      : public component_updater::MockComponentUpdateService {
   public:
    MockComponentUpdateService();
    ~MockComponentUpdateService() override;

    void AddObserver(Observer* observer) override;

    void RemoveObserver(Observer* observer) override;

    void SendUpdate(const component_updater::CrxUpdateItem& item);

    // Not copyable or movable.
    MockComponentUpdateService(const MockComponentUpdateService&) = delete;
    MockComponentUpdateService& operator=(const MockComponentUpdateService&) =
        delete;

   private:
    base::ObserverList<Observer>::Unchecked observer_list_;
  };

  class AITestBase : public ChromeRenderViewHostTestHarness {
   public:
    AITestBase();
    ~AITestBase() override;

    void SetUp() override;
    void TearDown() override;

   protected:
    virtual void SetupMockOptimizationGuideKeyedService();
    virtual void SetupNullOptimizationGuideKeyedService();

    // Optimization guide keyed service should be set up before calling this
    // method.
    void SetupMockSession();

    blink::mojom::AIManager* GetAIManagerInterface();
    mojo::Remote<blink::mojom::AIManager> GetAIManagerRemote();
    size_t GetAIManagerContextBoundObjectSetSize();
    size_t GetAIManagerDownloadProgressObserversSize();

    raw_ptr<MockOptimizationGuideKeyedService>
        mock_optimization_guide_keyed_service_;
    testing::NiceMock<optimization_guide::MockSession> session_;
    AITestUtils::MockComponentUpdateService component_update_service_;

    std::unique_ptr<AIManager> ai_manager_;
  };

  static const optimization_guide::TokenLimits& GetFakeTokenLimits();
  static const optimization_guide::proto::Any& GetFakeFeatureMetadata();

  // Converts string language codes to AILanguageCode mojo struct.
  static std::vector<blink::mojom::AILanguageCodePtr> ToMojoLanguageCodes(
      const std::vector<std::string>& language_codes);
};

#endif  // CHROME_BROWSER_AI_AI_TEST_UTILS_H_
