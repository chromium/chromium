// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEST_UTILS_H_
#define CHROME_BROWSER_AI_AI_TEST_UTILS_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ai/ai_manager.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/model_execution/test/mock_on_device_capability.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/update_client/crx_update_item.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

class AITestUtils {
 public:
  class TestStreamingResponder : public blink::mojom::ModelStreamingResponder {
   public:
    TestStreamingResponder();
    ~TestStreamingResponder() override;

    mojo::PendingRemote<blink::mojom::ModelStreamingResponder> BindRemote();

    // Returns true on successful completion and false on error.
    bool WaitForCompletion();

    void WaitForContextOverflow();

    blink::mojom::ModelStreamingResponseStatus error_status() const {
      EXPECT_TRUE(error_status_.has_value());
      return *error_status_;
    }

    blink::mojom::QuotaErrorInfo quota_error_info() const {
      return *quota_error_info_;
    }

    const std::vector<std::string> responses() const { return responses_; }
    const std::vector<std::string> responses_without_last() const {
      EXPECT_TRUE(responses_.size() > 1);
      EXPECT_EQ(responses_.back(), "");
      return std::vector<std::string>(responses_.begin(), responses_.end() - 1);
    }

    uint64_t current_tokens() const { return current_tokens_; }

   private:
    // blink::mojom::ModelStreamingResponder:
    void OnError(blink::mojom::ModelStreamingResponseStatus status,
                 blink::mojom::QuotaErrorInfoPtr quota_error_info) override;
    void OnStreaming(const std::string& text) override;
    void OnCompletion(
        blink::mojom::ModelExecutionContextInfoPtr context_info) override;
    void OnContextOverflow() override;

    std::optional<blink::mojom::ModelStreamingResponseStatus> error_status_;
    blink::mojom::QuotaErrorInfoPtr quota_error_info_;
    std::vector<std::string> responses_;
    uint64_t current_tokens_ = 0;
    base::RunLoop run_loop_;
    base::RunLoop context_overflow_run_loop_;
    mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
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

    virtual optimization_guide::proto::OnDeviceModelExecutionFeatureConfig
    CreateConfig() = 0;

    blink::mojom::AIManager* GetAIManagerInterface();
    mojo::Remote<blink::mojom::AIManager> GetAIManagerRemote();
    size_t GetAIManagerContextBoundObjectSetSize();

    raw_ptr<MockOptimizationGuideKeyedService>
        mock_optimization_guide_keyed_service_;
    std::unique_ptr<optimization_guide::FakeModelBroker> fake_broker_;
    std::unique_ptr<optimization_guide::FakeAdaptationAsset> fake_asset_;

    std::unique_ptr<AIManager> ai_manager_;
  };

  // Converts string language codes to AILanguageCode mojo struct.
  static std::vector<blink::mojom::AILanguageCodePtr> ToMojoLanguageCodes(
      const std::vector<std::string>& language_codes);
};

#endif  // CHROME_BROWSER_AI_AI_TEST_UTILS_H_
