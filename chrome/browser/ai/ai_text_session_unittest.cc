// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom.h"

using testing::_;
using testing::Test;
using Role = blink::mojom::AIAssistantInitialPromptRole;

namespace {

using optimization_guide::proto::PromptApiRequest;
using optimization_guide::proto::PromptApiRole;

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

std::string GetContextString(AITextSession::Context& ctx) {
  auto msg = ctx.MakeRequest();
  auto* v = static_cast<optimization_guide::proto::StringValue*>(msg.get());
  return v->value();
}

AITextSession::Context::ContextItem SimpleContextItem(std::string text,
                                                      uint32_t size) {
  auto item = AITextSession::Context::ContextItem();
  item.tokens = size;
  auto* prompt = item.prompts.Add();
  prompt->set_role(PromptApiRole::PROMPT_API_ROLE_SYSTEM);
  prompt->set_content(text);
  return item;
}

const char* FormatPromptRole(PromptApiRole role) {
  switch (role) {
    case PromptApiRole::PROMPT_API_ROLE_SYSTEM:
      return "S: ";
    case PromptApiRole::PROMPT_API_ROLE_USER:
      return "U: ";
    case PromptApiRole::PROMPT_API_ROLE_ASSISTANT:
      return "M: ";
    default:
      NOTREACHED();
  }
}

std::string ToString(const PromptApiRequest& request) {
  std::ostringstream oss;
  for (const auto& prompt : request.initial_prompts()) {
    oss << FormatPromptRole(prompt.role()) << prompt.content() << "\n";
  }
  for (const auto& prompt : request.prompt_history()) {
    oss << FormatPromptRole(prompt.role()) << prompt.content() << "\n";
  }
  for (const auto& prompt : request.current_prompts()) {
    oss << FormatPromptRole(prompt.role()) << prompt.content() << "\n";
  }
  if (request.current_prompts_size() > 0) {
    oss << FormatPromptRole(PromptApiRole::PROMPT_API_ROLE_ASSISTANT);
  }
  return oss.str();
}

std::string ToString(const google::protobuf::MessageLite& request_metadata) {
  if (request_metadata.GetTypeName() ==
      "optimization_guide.proto.PromptApiRequest") {
    return ToString(*static_cast<const PromptApiRequest*>(&request_metadata));
  }
  if (request_metadata.GetTypeName() ==
      "optimization_guide.proto.StringValue") {
    return static_cast<const optimization_guide::proto::StringValue*>(
               &request_metadata)
        ->value();
  }
  return "unexpected type";
}

const optimization_guide::proto::Any& GetPromptApiMetadata() {
  static base::NoDestructor<optimization_guide::proto::Any> data([]() {
    optimization_guide::proto::PromptApiMetadata metadata;
    metadata.set_version(1);
    optimization_guide::proto::Any any;
    any.set_type_url("type.googleapis.com/" + metadata.GetTypeName());
    any.set_value(metadata.SerializeAsString());
    return any;
  }());
  return *data;
}

}  // namespace

class AITextSessionTest : public AITestUtils::AITestBase {
 public:
  struct Options {
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params = nullptr;
    std::optional<std::string> system_prompt = std::nullopt;
    std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts;
    std::string prompt_input = kTestPrompt;
    std::string expected_context = "";
    std::string expected_prompt = kExpectedFormattedTestPrompt;
    bool use_prompt_api_proto = false;
  };

 protected:
  // The helper function that creates a `AITextSession` and executes the prompt.
  void RunPromptTest(Options options) {
    blink::mojom::AITextSessionSamplingParamsPtr sampling_params_copy;
    if (options.sampling_params) {
      sampling_params_copy = options.sampling_params->Clone();
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
          ON_CALL(*session, GetOnDeviceFeatureMetadata())
              .WillByDefault(options.use_prompt_api_proto
                                 ? GetPromptApiMetadata
                                 : AITestUtils::GetFakeFeatureMetadata);
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
          ON_CALL(*session, GetContextSizeInTokens(_, _))
              .WillByDefault(
                  [](const google::protobuf::MessageLite& request_metadata,
                     optimization_guide::
                         OptimizationGuideModelSizeInTokenCallback callback) {
                    std::move(callback).Run(ToString(request_metadata).size());
                  });
          ON_CALL(*session, AddContext(_))
              .WillByDefault(
                  [&](const google::protobuf::MessageLite& request_metadata) {
                    EXPECT_THAT(ToString(request_metadata),
                                options.expected_context);
                  });
          EXPECT_CALL(*session, ExecuteModel(_, _))
              .WillOnce(
                  [&](const google::protobuf::MessageLite& request_metadata,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultStreamingCallback
                              callback) {
                    EXPECT_THAT(ToString(request_metadata),
                                options.expected_prompt);
                    callback.Run(CreateExecutionResult(kTestResponse,
                                                       /*is_complete=*/true));
                  });
          return session;
        });

    mojo::Remote<blink::mojom::AIManager> ai_manager = GetAIManagerRemote();
    mojo::Remote<blink::mojom::AITextSession> mock_session;
    ai_manager->CreateTextSession(
        mock_session.BindNewPipeAndPassReceiver(),
        std::move(options.sampling_params), options.system_prompt,
        std::move(options.initial_prompts), base::NullCallback());

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

    mock_session->Prompt(options.prompt_input,
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
  RunPromptTest(AITextSessionTest::Options{
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AITextSessionTest, PromptSessionWithSamplingParams) {
  RunPromptTest(AITextSessionTest::Options{
      .sampling_params = blink::mojom::AITextSessionSamplingParams::New(
          /*top_k=*/10, /*temperature=*/0.6),
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AITextSessionTest, PromptSessionWithSystemPrompt) {
  RunPromptTest(AITextSessionTest::Options{
      .system_prompt = kTestSystemPrompts,
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedSystemPrompts,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AITextSessionTest, PromptSessionWithInitialPrompts) {
  RunPromptTest(AITextSessionTest::Options{
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedInitialPrompts,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AITextSessionTest, PromptSessionWithSystemPromptAndInitialPrompts) {
  RunPromptTest(AITextSessionTest::Options{
      .system_prompt = kTestSystemPrompts,
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedSystemPromptAndInitialPrompts,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AITextSessionTest, PromptSessionWithPromptApiRequests) {
  RunPromptTest(AITextSessionTest::Options{
      .system_prompt = "Test system prompt",
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = "Test prompt",
      .expected_context = ("S: Test system prompt\n"
                           "U: How are you?\n"
                           "M: I'm fine, thank you, and you?\n"
                           "U: I'm fine too.\n"),
      .expected_prompt = "U: Test prompt\nM: ",
      .use_prompt_api_proto = true,
  });
}

// Tests `AITextSession::Context` creation without initial prompts.
TEST(AITextSessionContextCreationTest, CreateContext_WithoutInitialPrompts) {
  AITextSession::Context context(kTestMaxContextToken, {},
                                 /*use_prompt_api_request*/ false);
  EXPECT_FALSE(context.HasContextItem());
}

// Tests `AITextSession::Context` creation with valid initial prompts.
TEST(AITextSessionContextCreationTest,
     CreateContext_WithInitialPrompts_Normal) {
  AITextSession::Context context(
      kTestMaxContextToken,
      SimpleContextItem("initial prompts\n", kTestInitialPromptsToken),
      /*use_prompt_api_request*/ false);
  EXPECT_TRUE(context.HasContextItem());
}

// Tests `AITextSession::Context` creation with initial prompts that exceeds the
// max token limit.
TEST(AITextSessionContextCreationTest,
     CreateContext_WithInitialPrompts_Overflow) {
  EXPECT_DEATH_IF_SUPPORTED(AITextSession::Context context(
                                kTestMaxContextToken,
                                SimpleContextItem("long initial prompts\n",
                                                  kTestMaxContextToken + 1u),
                                /*use_prompt_api_request*/ false),
                            "");
}

// Tests the `AITextSession::Context` that's initialized with/without any
// initial prompt.
class AITextSessionContextTest : public testing::Test,
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

  AITextSession::Context context_{
      kTestMaxContextToken,
      IsInitializedWithInitialPrompts()
          ? SimpleContextItem("initial prompts", kTestInitialPromptsToken)
          : AITextSession::Context::ContextItem(),
      /*use_prompt_api_request*/ false};
};

INSTANTIATE_TEST_SUITE_P(All,
                         AITextSessionContextTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithInitialPrompts"
                                             : "WithoutInitialPrompts";
                         });

// Tests `GetContextString()` and `HasContextItem()` when the context is empty.
TEST_P(AITextSessionContextTest, TestContextOperation_Empty) {
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix());

  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}

// Tests `GetContextString()` and `HasContextItem()` when some items are added
// to the context.
TEST_P(AITextSessionContextTest, TestContextOperation_NonEmpty) {
  context_.AddContextItem(SimpleContextItem("test", 1u));
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix() + "test\n");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem(SimpleContextItem(" test again", 2u));
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "test\n test again\n");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_P(AITextSessionContextTest, TestContextOperation_Overflow) {
  context_.AddContextItem(SimpleContextItem("test", 1u));
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix() + "test\n");
  EXPECT_TRUE(context_.HasContextItem());

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  context_.AddContextItem(
      SimpleContextItem("test long token", GetMaxContextToken()));
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "test long token\n");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow on
// the first insertion.
TEST_P(AITextSessionContextTest, TestContextOperation_OverflowOnFirstItem) {
  context_.AddContextItem(
      SimpleContextItem("test very long token", GetMaxContextToken() + 1u));
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix());
  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}
