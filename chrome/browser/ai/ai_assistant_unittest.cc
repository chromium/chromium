// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_assistant.h"

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
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"

using testing::_;
using testing::Test;
using Role = blink::mojom::AIAssistantInitialPromptRole;

namespace {

const uint32_t kTestMaxContextToken = 10u;
const uint32_t kTestInitialPromptsToken = 5u;
const uint32_t kDefaultTopK = 1;
const float kDefaultTemperature = 0.0;

const char kTestPrompt[] = "Test prompt";
const char kExpectedFormattedTestPrompt[] = "User: Test prompt\nModel: ";
const char kTestSystemPrompts[] = "Test system prompt";
const char kExpectedFormattedSystemPrompts[] = "Test system prompt\n";
const char kTestResponse[] = "Test response";

const char kTestInitialPromptsUser1[] = "How are you?";
const char kTestInitialPromptsSystem1[] = "I'm fine, thank you, and you?";
const char kTestInitialPromptsUser2[] = "I'm fine too.";
const char kExpectedFormattedInitialPrompts[] =
    "User: How are you?\nModel: I'm fine, thank you, and you?\nUser: I'm fine "
    "too.\n";
const char kExpectedFormattedSystemPromptAndInitialPrompts[] =
    "Test system prompt\nUser: How are you?\nModel: I'm fine, thank you, and "
    "you?\nUser: I'm fine too.\n";
std::vector<blink::mojom::AIAssistantInitialPromptPtr> GetTestInitialPrompts() {
  auto create_initial_prompt = [](Role role, const char* content) {
    return blink::mojom::AIAssistantInitialPrompt::New(role, content);
  };
  std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts{};
  initial_prompts.push_back(
      create_initial_prompt(Role::kUser, kTestInitialPromptsUser1));
  initial_prompts.push_back(
      create_initial_prompt(Role::kAssistant, kTestInitialPromptsSystem1));
  initial_prompts.push_back(
      create_initial_prompt(Role::kUser, kTestInitialPromptsUser2));
  return initial_prompts;
}

}  // namespace

class AIAssistantTest : public AITestUtils::AITestBase {
 protected:
  // The helper function that creates a `AIAssistant` and executes the prompt.
  void RunPromptTest(
      const std::string& prompt_input,
      blink::mojom::AIAssistantSamplingParamsPtr sampling_params,
      const std::optional<std::string>& system_prompt,
      std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts,
      const std::string& expected_context,
      const std::string& expected_prompt) {
    blink::mojom::AIAssistantSamplingParamsPtr sampling_params_copy;
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
              .WillByDefault([&](const google::protobuf::MessageLite&
                                     request_metadata) {
                EXPECT_THAT(
                    static_cast<const optimization_guide::proto::StringValue*>(
                        &request_metadata)
                        ->value(),
                    expected_context.c_str());
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
                        expected_prompt);
                    callback.Run(CreateExecutionResult(kTestResponse,
                                                       /*is_complete=*/true));
                  });
          return session;
        });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    mojo::Remote<blink::mojom::AIAssistant> mock_session;
    ai_manager->CreateAssistant(
        mock_session.BindNewPipeAndPassReceiver(), std::move(sampling_params),
        system_prompt, std::move(initial_prompts), base::NullCallback());

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

TEST_F(AIAssistantTest, PromptDefaultSession) {
  RunPromptTest(kTestPrompt, /*sampling_params=*/nullptr,
                /*system_prompt=*/std::nullopt, /*initial_prompts=*/{},
                /*expected_context=*/"", kExpectedFormattedTestPrompt);
}

TEST_F(AIAssistantTest, PromptSessionWithSamplingParams) {
  RunPromptTest(kTestPrompt,
                blink::mojom::AIAssistantSamplingParams::New(
                    /*top_k=*/10, /*temperature=*/0.6),
                /*system_prompt=*/std::nullopt, /*initial_prompts=*/{},
                /*expected_context=*/"", kExpectedFormattedTestPrompt);
}

TEST_F(AIAssistantTest, PromptSessionWithSystemPrompt) {
  RunPromptTest(kTestPrompt, /*sampling_params=*/nullptr, kTestSystemPrompts,
                /*initial_prompts=*/{}, kExpectedFormattedSystemPrompts,
                kExpectedFormattedTestPrompt);
}

TEST_F(AIAssistantTest, PromptSessionWithInitialPrompts) {
  RunPromptTest(kTestPrompt, /*sampling_params=*/nullptr,
                /*system_prompt=*/std::nullopt, GetTestInitialPrompts(),
                kExpectedFormattedInitialPrompts, kExpectedFormattedTestPrompt);
}

TEST_F(AIAssistantTest, PromptSessionWithSystemPromptAndInitialPrompts) {
  RunPromptTest(kTestPrompt, /*sampling_params=*/nullptr, kTestSystemPrompts,
                GetTestInitialPrompts(),
                kExpectedFormattedSystemPromptAndInitialPrompts,
                kExpectedFormattedTestPrompt);
}

// Tests `AIAssistant::Context` creation without initial prompts.
TEST(AIAssistantContextCreationTest, CreateContext_WithoutInitialPrompts) {
  AIAssistant::Context context(kTestMaxContextToken, std::nullopt);
  EXPECT_FALSE(context.HasContextItem());
}

// Tests `AIAssistant::Context` creation with valid initial prompts.
TEST(AIAssistantContextCreationTest, CreateContext_WithInitialPrompts_Normal) {
  AIAssistant::Context context(
      kTestMaxContextToken, AIAssistant::Context::ContextItem{
                                "initial prompts\n", kTestInitialPromptsToken});
  EXPECT_TRUE(context.HasContextItem());
}

// Tests `AIAssistant::Context` creation with initial prompts that exceeds the
// max token limit.
TEST(AIAssistantContextCreationTest,
     CreateContext_WithInitialPrompts_Overflow) {
  EXPECT_DEATH_IF_SUPPORTED(
      AIAssistant::Context context(
          kTestMaxContextToken,
          AIAssistant::Context::ContextItem{"long initial prompts\n",
                                            kTestMaxContextToken + 1u}),
      "");
}

// Tests the `AIAssistant::Context` that's initialized with/without any
// initial prompt.
class AIAssistantContextTest : public testing::Test,
                               public testing::WithParamInterface<
                                   /*is_init_with_initial_prompts=*/bool> {
 public:
  bool IsInitializedWithInitialPrompts() { return GetParam(); }

  uint32_t GetMaxContextToken() {
    return IsInitializedWithInitialPrompts()
               ? kTestMaxContextToken - kTestInitialPromptsToken
               : kTestMaxContextToken;
  }

  std::string GetInitialPromptsPrefix() {
    return IsInitializedWithInitialPrompts() ? "initial prompts\n" : "";
  }

  AIAssistant::Context context_{
      kTestMaxContextToken,
      IsInitializedWithInitialPrompts()
          ? std::optional<
                AIAssistant::Context::ContextItem>{{"initial prompts",
                                                    kTestInitialPromptsToken}}
          : std::nullopt};
};

INSTANTIATE_TEST_SUITE_P(All,
                         AIAssistantContextTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithInitialPrompts"
                                             : "WithoutInitialPrompts";
                         });

// Tests `GetContextString()` and `HasContextItem()` when the context is empty.
TEST_P(AIAssistantContextTest, TestContextOperation_Empty) {
  EXPECT_EQ(context_.GetContextString(), GetInitialPromptsPrefix());

  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}

// Tests `GetContextString()` and `HasContextItem()` when some items are added
// to the context.
TEST_P(AIAssistantContextTest, TestContextOperation_NonEmpty) {
  context_.AddContextItem({"test", 1u});
  EXPECT_EQ(context_.GetContextString(), GetInitialPromptsPrefix() + "test");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem({" test again", 2u});
  EXPECT_EQ(context_.GetContextString(),
            GetInitialPromptsPrefix() + "test test again");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_P(AIAssistantContextTest, TestContextOperation_Overflow) {
  context_.AddContextItem({"test", 1u});
  EXPECT_EQ(context_.GetContextString(), GetInitialPromptsPrefix() + "test");
  EXPECT_TRUE(context_.HasContextItem());

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  context_.AddContextItem({"test long token", GetMaxContextToken()});
  EXPECT_EQ(context_.GetContextString(),
            GetInitialPromptsPrefix() + "test long token");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow on
// the first insertion.
TEST_P(AIAssistantContextTest, TestContextOperation_OverflowOnFirstItem) {
  context_.AddContextItem({"test very long token", GetMaxContextToken() + 1u});
  EXPECT_EQ(context_.GetContextString(), GetInitialPromptsPrefix());
  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}
