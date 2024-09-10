// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"

using testing::_;
using testing::Test;

namespace {

const uint32_t kTestMaxContextToken = 10u;
const uint32_t kTestSystemPromptToken = 5u;
const uint32_t kDefaultTopK = 1;
const float kDefaultTemperature = 0.0;

const char kTestPrompt[] = "Test prompt";
const char kTestSystemPrompt[] = "Test system prompt";
const char kTestResponse[] = "Test response";

}  // namespace

class AITextSessionTest : public AITestUtils::AITestBase {
 protected:
  // The helper function that creates a `AITextSession` and executes the prompt.
  void RunPromptTest(
      const std::string& prompt_input,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      const std::optional<std::string>& system_prompt) {
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params_copy;
    if (sampling_params) {
      sampling_params_copy = sampling_params->Clone();
    }

    // Set up mock service.
    SetupMockOptimizationGuideKeyedService();
    EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature,
                      const std::optional<
                          optimization_guide::SessionConfigParams>&
                          config_params) {
          auto session = std::make_unique<
              testing::NiceMock<optimization_guide::MockSession>>();
          if (sampling_params_copy) {
            EXPECT_EQ(config_params->sampling_params->top_k,
                      sampling_params_copy->top_k);
            EXPECT_EQ(config_params->sampling_params->temperature,
                      sampling_params_copy->temperature);
          }

          ON_CALL(*session, GetTokenLimits())
              .WillByDefault(AITestUtils::GetFakeTokenLimits);
          ON_CALL(*session, GetSamplingParams()).WillByDefault([]() {
            // We don't need to use these value, so just mock it with defaults.
            return optimization_guide::SamplingParams{
                /*top_k=*/kDefaultTopK,
                /*temperature=*/kDefaultTemperature};
          });
          ON_CALL(*session, GetSizeInTokens(_, _))
              .WillByDefault(
                  [](const std::string& text,
                     optimization_guide::
                         OptimizationGuideModelSizeInTokenCallback callback) {
                    std::move(callback).Run(text.size());
                  });

          ON_CALL(*session, AddContext(_))
              .WillByDefault([&system_prompt](
                                 const google::protobuf::MessageLite&
                                     request_metadata) {
                EXPECT_TRUE(system_prompt.has_value());
                EXPECT_THAT(
                    static_cast<const optimization_guide::proto::StringValue*>(
                        &request_metadata)
                        ->value(),
                    base::StringPrintf(
                        AITextSession::GetSystemPromptFormatForTesting()
                            .c_str(),
                        system_prompt->c_str()));
              });
          EXPECT_CALL(*session, ExecuteModel(_, _))
              .WillOnce(
                  [&](const google::protobuf::MessageLite& request_metadata,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultStreamingCallback
                              callback) {
                    EXPECT_THAT(
                        static_cast<
                            const optimization_guide::proto::StringValue*>(
                            &request_metadata)
                            ->value(),
                        base::StringPrintf(
                            AITextSession::GetPromptFormatForTesting().c_str(),
                            kTestPrompt));
                    callback.Run(CreateExecutionResult(kTestResponse,
                                                       /*is_complete=*/true));
                  });
          return session;
        });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    mojo::Remote<blink::mojom::AITextSession> mock_session;
    ai_manager->CreateTextSession(mock_session.BindNewPipeAndPassReceiver(),
                                  std::move(sampling_params), system_prompt,
                                  base::NullCallback());

    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop run_loop;
    // This is run twice because the response is returned together with
    // `is_complete` set to true.
    EXPECT_CALL(mock_responder, OnResponse(_, _, _))
        .WillOnce([&](blink::mojom::ModelStreamingResponseStatus status,
                      const std::optional<std::string>& text,
                      std::optional<uint64_t> current_tokens) {
          EXPECT_THAT(text, kTestResponse);
          EXPECT_EQ(status,
                    blink::mojom::ModelStreamingResponseStatus::kOngoing);
        })
        .WillOnce([&](blink::mojom::ModelStreamingResponseStatus status,
                      const std::optional<std::string>& text,
                      std::optional<uint64_t> current_tokens) {
          EXPECT_EQ(status,
                    blink::mojom::ModelStreamingResponseStatus::kComplete);
          run_loop.Quit();
        });

    mock_session->Prompt(prompt_input,
                         mock_responder.BindNewPipeAndPassRemote());
    run_loop.Run();
  }

 private:
  optimization_guide::OptimizationGuideModelStreamingExecutionResult
  CreateExecutionResult(const std::string& output, bool is_complete) {
    optimization_guide::proto::StringValue response;
    response.set_value(output);
    std::string serialized_metadata;
    response.SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any;
    any.set_value(serialized_metadata);
    any.set_type_url(AITestUtils::GetTypeURLForProto(response.GetTypeName()));
    return optimization_guide::OptimizationGuideModelStreamingExecutionResult(
        optimization_guide::StreamingResponse{
            .response = any,
            .is_complete = is_complete,
        },
        /*provided_by_on_device=*/true);
  }

  std::unique_ptr<AITestUtils::MockSupportsUserData> mock_host_;
};

TEST_F(AITextSessionTest, PromptDefaultSession) {
  RunPromptTest(kTestPrompt, /*sampling_params=*/nullptr,
                /*system_prompt=*/std::nullopt);
}

TEST_F(AITextSessionTest, PromptSessionWithSamplingParams) {
  RunPromptTest(kTestPrompt,
                blink::mojom::AITextSessionSamplingParams::New(
                    /*top_k=*/10, /*temperature=*/0.6),
                /*system_prompt=*/std::nullopt);
}

TEST_F(AITextSessionTest, PromptSessionWithSystemPrompt) {
  RunPromptTest(kTestPrompt, /*sampling_params=*/nullptr, kTestSystemPrompt);
}

// Tests `AITextSession::Context` creation without system prompt.
TEST(AITextSessionContextCreationTest, CreateContext_WithoutSystemPrompt) {
  AITextSession::Context context(kTestMaxContextToken, std::nullopt);
  EXPECT_FALSE(context.HasContextItem());
}

// Tests `AITextSession::Context` creation with valid system prompt.
TEST(AITextSessionContextCreationTest, CreateContext_WithSystemPrompt_Normal) {
  AITextSession::Context context(
      kTestMaxContextToken, AITextSession::Context::ContextItem{
                                "system prompt\n", kTestSystemPromptToken});
  EXPECT_TRUE(context.HasContextItem());
}

// Tests `AITextSession::Context` creation with system prompt that exceeds the
// max token limit.
TEST(AITextSessionContextCreationTest,
     CreateContext_WithSystemPrompt_Overflow) {
  EXPECT_DEATH_IF_SUPPORTED(
      AITextSession::Context context(
          kTestMaxContextToken,
          AITextSession::Context::ContextItem{"long system prompt\n",
                                              kTestMaxContextToken + 1u}),
      "");
}

// Tests the `AITextSession::Context` that's initialized with/without any system
// prompt.
class AITextSessionContextTest
    : public testing::Test,
      public testing::WithParamInterface</*is_init_with_system_prompt=*/bool> {
 public:
  bool IsInitializedWithSystemPrompt() { return GetParam(); }

  uint32_t GetMaxContextToken() {
    return IsInitializedWithSystemPrompt()
               ? kTestMaxContextToken - kTestSystemPromptToken
               : kTestMaxContextToken;
  }

  std::string GetSystemPromptPrefix() {
    return IsInitializedWithSystemPrompt() ? "system prompt\n" : "";
  }

  AITextSession::Context context_{
      kTestMaxContextToken,
      IsInitializedWithSystemPrompt()
          ? std::optional<
                AITextSession::Context::ContextItem>{{"system prompt",
                                                      kTestSystemPromptToken}}
          : std::nullopt};
};

INSTANTIATE_TEST_SUITE_P(All,
                         AITextSessionContextTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSystemPrompt"
                                             : "WithoutSystemPrompt";
                         });

// Tests `GetContextString()` and `HasContextItem()` when the context is empty.
TEST_P(AITextSessionContextTest, TestContextOperation_Empty) {
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix());

  if (IsInitializedWithSystemPrompt()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}

// Tests `GetContextString()` and `HasContextItem()` when some items are added
// to the context.
TEST_P(AITextSessionContextTest, TestContextOperation_NonEmpty) {
  context_.AddContextItem({"test", 1u});
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix() + "test");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem({" test again", 2u});
  EXPECT_EQ(context_.GetContextString(),
            GetSystemPromptPrefix() + "test test again");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_P(AITextSessionContextTest, TestContextOperation_Overflow) {
  context_.AddContextItem({"test", 1u});
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix() + "test");
  EXPECT_TRUE(context_.HasContextItem());

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  context_.AddContextItem({"test long token", GetMaxContextToken()});
  EXPECT_EQ(context_.GetContextString(),
            GetSystemPromptPrefix() + "test long token");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow on
// the first insertion.
TEST_P(AITextSessionContextTest, TestContextOperation_OverflowOnFirstItem) {
  context_.AddContextItem({"test very long token", GetMaxContextToken() + 1u});
  EXPECT_EQ(context_.GetContextString(), GetSystemPromptPrefix());
  if (IsInitializedWithSystemPrompt()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}
