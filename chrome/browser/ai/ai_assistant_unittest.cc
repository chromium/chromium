// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_assistant.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_assistant.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"

using testing::_;
using testing::Test;
using Role = blink::mojom::AIAssistantInitialPromptRole;

namespace {

using optimization_guide::proto::PromptApiRequest;
using optimization_guide::proto::PromptApiRole;

const uint32_t kTestMaxContextToken = 10u;
const uint32_t kTestInitialPromptsToken = 5u;
const uint32_t kDefaultTopK = 1u;
const uint32_t kOverrideMaxTopK = 5u;
const float kDefaultTemperature = 0.0;

const std::string kTestPrompt = "Test prompt";
const std::string kExpectedFormattedTestPrompt = "User: Test prompt\nModel: ";
const std::string kTestSystemPrompts = "Test system prompt";
const std::string kExpectedFormattedSystemPrompts = "Test system prompt\n";
const std::string kTestResponse = "Test response";

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

std::string GetContextString(AIAssistant::Context& ctx) {
  auto msg = ctx.MakeRequest();
  auto* v = static_cast<optimization_guide::proto::StringValue*>(msg.get());
  return v->value();
}

AIAssistant::Context::ContextItem SimpleContextItem(std::string text,
                                                    uint32_t size) {
  auto item = AIAssistant::Context::ContextItem();
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

class AIAssistantTest : public AITestUtils::AITestBase {
 public:
  struct Options {
    blink::mojom::AIAssistantSamplingParamsPtr sampling_params = nullptr;
    std::optional<std::string> system_prompt = std::nullopt;
    std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts;
    std::string prompt_input = kTestPrompt;
    std::string expected_context = "";
    std::string expected_cloned_context =
        kExpectedFormattedTestPrompt + kTestResponse + "\n";
    std::string expected_prompt = kExpectedFormattedTestPrompt;
    bool use_prompt_api_proto = false;
  };

  void SetUp() override {
    AITestUtils::AITestBase::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
            features::kAIAssistantOverrideConfiguration,
            {{"max_top_k", base::NumberToString(kOverrideMaxTopK)}})},
        {});
  }

 protected:
  // The helper function that creates a `AIAssistant` and executes the prompt.
  void RunPromptTest(Options options) {
    blink::mojom::AIAssistantSamplingParamsPtr sampling_params_copy;
    if (options.sampling_params) {
      sampling_params_copy = options.sampling_params->Clone();
    }

    // Set up mock service.
    SetupMockOptimizationGuideKeyedService();
    EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        // It will run twice, the first time for the session creation, and the
        // second time for the session cloning.
        .Times(2)
        .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature,
                      const std::optional<
                          optimization_guide::SessionConfigParams>&
                          config_params) {
          auto session = std::make_unique<
              testing::NiceMock<optimization_guide::MockSession>>();
          if (sampling_params_copy) {
            EXPECT_EQ(config_params->sampling_params->top_k,
                      std::min(kOverrideMaxTopK, sampling_params_copy->top_k));
            EXPECT_EQ(config_params->sampling_params->temperature,
                      sampling_params_copy->temperature);
          }

          SetUpMockSession(*session, options.use_prompt_api_proto);

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
        })
        .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature,
                      const std::optional<
                          optimization_guide::SessionConfigParams>&
                          config_params) {
          auto session = std::make_unique<
              testing::NiceMock<optimization_guide::MockSession>>();

          SetUpMockSession(*session, options.use_prompt_api_proto);

          ON_CALL(*session, AddContext(_))
              .WillByDefault(
                  [&](const google::protobuf::MessageLite& request_metadata) {
                    EXPECT_THAT(ToString(request_metadata),
                                options.expected_cloned_context);
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

    // Test session creation.
    mojo::Remote<blink::mojom::AIAssistant> mock_session;
    AITestUtils::MockCreateAssistantClient mock_create_assistant_client;
    base::RunLoop creation_run_loop;
    EXPECT_CALL(mock_create_assistant_client, OnResult(_, _))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<blink::mojom::AIAssistant> assistant,
                blink::mojom::AIAssistantInfoPtr info) {
              EXPECT_TRUE(assistant);
              mock_session =
                  mojo::Remote<blink::mojom::AIAssistant>(std::move(assistant));
              creation_run_loop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();

    mock_remote->CreateAssistant(
        mock_create_assistant_client.BindNewPipeAndPassRemote(),
        blink::mojom::AIAssistantCreateOptions::New(
            std::move(options.sampling_params), options.system_prompt,
            std::move(options.initial_prompts)));
    creation_run_loop.Run();

    AITestUtils::MockModelStreamingResponder mock_responder;

    TestPromptCall(mock_session, options.prompt_input);

    // Test session cloning.
    mojo::Remote<blink::mojom::AIAssistant> mock_cloned_session;
    AITestUtils::MockCreateAssistantClient mock_clone_assistant_client;
    base::RunLoop clone_run_loop;
    EXPECT_CALL(mock_clone_assistant_client, OnResult(_, _))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<blink::mojom::AIAssistant> assistant,
                blink::mojom::AIAssistantInfoPtr info) {
              EXPECT_TRUE(assistant);
              mock_cloned_session =
                  mojo::Remote<blink::mojom::AIAssistant>(std::move(assistant));
              clone_run_loop.Quit();
            }));

    mock_session->Fork(mock_clone_assistant_client.BindNewPipeAndPassRemote());
    clone_run_loop.Run();

    TestPromptCall(mock_cloned_session, options.prompt_input);
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

  void SetUpMockSession(
      testing::NiceMock<optimization_guide::MockSession>& session,
      bool use_prompt_api_proto) {
    ON_CALL(session, GetTokenLimits())
        .WillByDefault(AITestUtils::GetFakeTokenLimits);
    ON_CALL(session, GetOnDeviceFeatureMetadata())
        .WillByDefault(use_prompt_api_proto
                           ? GetPromptApiMetadata
                           : AITestUtils::GetFakeFeatureMetadata);
    ON_CALL(session, GetSamplingParams()).WillByDefault([]() {
      // We don't need to use these value, so just mock it with defaults.
      return optimization_guide::SamplingParams{
          /*top_k=*/kDefaultTopK,
          /*temperature=*/kDefaultTemperature};
    });
    ON_CALL(session, GetSizeInTokens(_, _))
        .WillByDefault(
            [](const std::string& text,
               optimization_guide::OptimizationGuideModelSizeInTokenCallback
                   callback) { std::move(callback).Run(text.size()); });
    ON_CALL(session, GetContextSizeInTokens(_, _))
        .WillByDefault(
            [](const google::protobuf::MessageLite& request_metadata,
               optimization_guide::OptimizationGuideModelSizeInTokenCallback
                   callback) {
              std::move(callback).Run(ToString(request_metadata).size());
            });
  }

  void TestPromptCall(mojo::Remote<blink::mojom::AIAssistant>& mock_session,
                      std::string& prompt) {
    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop responder_run_loop;
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
          responder_run_loop.Quit();
        });

    mock_session->Prompt(prompt, mock_responder.BindNewPipeAndPassRemote());
    responder_run_loop.Run();
  }

  std::unique_ptr<AITestUtils::MockSupportsUserData> mock_host_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AIAssistantTest, PromptDefaultSession) {
  RunPromptTest(AIAssistantTest::Options{
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AIAssistantTest, PromptSessionWithSamplingParams) {
  RunPromptTest(AIAssistantTest::Options{
      .sampling_params = blink::mojom::AIAssistantSamplingParams::New(
          /*top_k=*/kOverrideMaxTopK - 1, /*temperature=*/0.6),
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AIAssistantTest, PromptSessionWithSamplingParams_ExceedMaxTopK) {
  RunPromptTest(AIAssistantTest::Options{
      .sampling_params = blink::mojom::AIAssistantSamplingParams::New(
          /*top_k=*/kOverrideMaxTopK + 1, /*temperature=*/0.6),
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AIAssistantTest, PromptSessionWithSystemPrompt) {
  RunPromptTest(AIAssistantTest::Options{
      .system_prompt = kTestSystemPrompts,
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedSystemPrompts,
      .expected_cloned_context = kExpectedFormattedSystemPrompts +
                                 kExpectedFormattedTestPrompt + kTestResponse +
                                 "\n",
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AIAssistantTest, PromptSessionWithInitialPrompts) {
  RunPromptTest(AIAssistantTest::Options{
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedInitialPrompts,
      .expected_cloned_context = kExpectedFormattedInitialPrompts +
                                 kExpectedFormattedTestPrompt + kTestResponse +
                                 "\n",
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AIAssistantTest, PromptSessionWithSystemPromptAndInitialPrompts) {
  RunPromptTest(AIAssistantTest::Options{
      .system_prompt = kTestSystemPrompts,
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedSystemPromptAndInitialPrompts,
      .expected_cloned_context =
          kExpectedFormattedSystemPrompts + kExpectedFormattedInitialPrompts +
          kExpectedFormattedTestPrompt + kTestResponse + "\n",
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_F(AIAssistantTest, PromptSessionWithPromptApiRequests) {
  RunPromptTest(AIAssistantTest::Options{
      .system_prompt = "Test system prompt",
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = "Test prompt",
      .expected_context = ("S: Test system prompt\n"
                           "U: How are you?\n"
                           "M: I'm fine, thank you, and you?\n"
                           "U: I'm fine too.\n"),
      .expected_cloned_context = ("S: Test system prompt\n"
                                  "U: How are you?\n"
                                  "M: I'm fine, thank you, and you?\n"
                                  "U: I'm fine too.\n"
                                  "U: Test prompt\n"
                                  "M: Test response\n"),
      .expected_prompt = "U: Test prompt\nM: ",
      .use_prompt_api_proto = true,
  });
}

// Tests `AIAssistant::Context` creation without initial prompts.
TEST(AIAssistantContextCreationTest, CreateContext_WithoutInitialPrompts) {
  AIAssistant::Context context(kTestMaxContextToken, {},
                               /*use_prompt_api_request*/ false);
  EXPECT_FALSE(context.HasContextItem());
}

// Tests `AIAssistant::Context` creation with valid initial prompts.
TEST(AIAssistantContextCreationTest, CreateContext_WithInitialPrompts_Normal) {
  AIAssistant::Context context(
      kTestMaxContextToken,
      SimpleContextItem("initial prompts\n", kTestInitialPromptsToken),
      /*use_prompt_api_request*/ false);
  EXPECT_TRUE(context.HasContextItem());
}

// Tests `AIAssistant::Context` creation with initial prompts that exceeds the
// max token limit.
TEST(AIAssistantContextCreationTest,
     CreateContext_WithInitialPrompts_Overflow) {
  EXPECT_DEATH_IF_SUPPORTED(
      AIAssistant::Context context(kTestMaxContextToken,
                                   SimpleContextItem("long initial prompts\n",
                                                     kTestMaxContextToken + 1u),
                                   /*use_prompt_api_request*/ false),
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
          ? SimpleContextItem("initial prompts", kTestInitialPromptsToken)
          : AIAssistant::Context::ContextItem(),
      /*use_prompt_api_request*/ false};
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
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix());

  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}

// Tests `GetContextString()` and `HasContextItem()` when some items are added
// to the context.
TEST_P(AIAssistantContextTest, TestContextOperation_NonEmpty) {
  context_.AddContextItem(SimpleContextItem("test", 1u));
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix() + "test\n");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem(SimpleContextItem(" test again", 2u));
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "test\n test again\n");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_P(AIAssistantContextTest, TestContextOperation_Overflow) {
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
TEST_P(AIAssistantContextTest, TestContextOperation_OverflowOnFirstItem) {
  context_.AddContextItem(
      SimpleContextItem("test very long token", GetMaxContextToken() + 1u));
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix());
  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}
