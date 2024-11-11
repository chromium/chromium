// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEST_UTILS_H_
#define CHROME_BROWSER_AI_AI_TEST_UTILS_H_

#include "base/supports_user_data.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

class AITestUtils {
 public:
  class MockSupportsUserData : public base::SupportsUserData {};

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
                (blink::mojom::ModelStreamingResponseStatus status),
                (override));
    MOCK_METHOD(void,
                OnCompletion,
                (blink::mojom::ModelExecutionContextInfoPtr context_info),
                (override));

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

  class MockCreateAssistantClient
      : public blink::mojom::AIManagerCreateAssistantClient {
   public:
    MockCreateAssistantClient();
    ~MockCreateAssistantClient() override;
    MockCreateAssistantClient(const MockCreateAssistantClient&) = delete;
    MockCreateAssistantClient& operator=(const MockCreateAssistantClient&) =
        delete;

    mojo::PendingRemote<blink::mojom::AIManagerCreateAssistantClient>
    BindNewPipeAndPassRemote();

    MOCK_METHOD(void,
                OnResult,
                (mojo::PendingRemote<blink::mojom::AIAssistant> assistant,
                 blink::mojom::AIAssistantInfoPtr info),
                (override));

   private:
    mojo::Receiver<blink::mojom::AIManagerCreateAssistantClient> receiver_{
        this};
  };

  class AITestBase : public ChromeRenderViewHostTestHarness {
   public:
    AITestBase();
    ~AITestBase() override;

    void SetUp() override;
    void TearDown() override;

   protected:
    void SetupMockOptimizationGuideKeyedService();
    void SetupNullOptimizationGuideKeyedService();

    mojo::Remote<blink::mojom::AIManager> GetAIManagerRemote();
    MockSupportsUserData& mock_host() { return *mock_host_.get(); }
    void ResetMockHost();
    size_t GetAIManagerReceiversSize();
    size_t GetAIManagerDownloadProgressObserversSize();
    void MockDownloadProgressUpdate(uint64_t downloaded_bytes,
                                    uint64_t total_bytes);

    raw_ptr<MockOptimizationGuideKeyedService>
        mock_optimization_guide_keyed_service_;

   private:
    AIManagerKeyedService* GetAIManager();
    std::unique_ptr<MockSupportsUserData> mock_host_;
  };

  static const optimization_guide::TokenLimits& GetFakeTokenLimits();
  static const optimization_guide::proto::Any& GetFakeFeatureMetadata();
};

#endif  // CHROME_BROWSER_AI_AI_TEST_UTILS_H_
