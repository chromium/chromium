// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEST_UTILS_H_
#define CHROME_BROWSER_AI_AI_TEST_UTILS_H_

#include "base/supports_user_data.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
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

    MOCK_METHOD(void,
                OnResponse,
                (blink::mojom::ModelStreamingResponseStatus status,
                 const std::optional<std::string>& text,
                 std::optional<uint64_t> current_tokens),
                (override));

   private:
    mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
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
    MockSupportsUserData* mock_host() { return mock_host_.get(); }
    void ResetMockHost();
    size_t GetAIManagerReceiversSize();

    raw_ptr<MockOptimizationGuideKeyedService>
        mock_optimization_guide_keyed_service_;

   private:
    std::unique_ptr<MockSupportsUserData> mock_host_;
  };

  static std::string GetTypeURLForProto(std::string type_name);
  static const optimization_guide::TokenLimits& GetFakeTokenLimits();
  static const optimization_guide::proto::Any& GetFakeFeatureMetadata();
};

#endif  // CHROME_BROWSER_AI_AI_TEST_UTILS_H_
