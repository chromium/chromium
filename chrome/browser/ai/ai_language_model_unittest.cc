// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"

using testing::_;
using testing::ReturnRef;
using testing::Test;
using Role = blink::mojom::AILanguageModelInitialPromptRole;

namespace {

using optimization_guide::proto::PromptApiRequest;
using optimization_guide::proto::PromptApiRole;

const uint32_t kTestMaxContextToken = 10u;
const uint32_t kTestInitialPromptsToken = 5u;
const uint32_t kDefaultTopK = 1u;
const uint32_t kOverrideMaxTopK = 5u;
const float kDefaultTemperature = 0.0;
const uint64_t kTestModelDownloadSize = 572u;

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

std::vector<blink::mojom::AILanguageModelInitialPromptPtr>
GetTestInitialPrompts() {
  auto create_initial_prompt = [](Role role, const char* content) {
    return blink::mojom::AILanguageModelInitialPrompt::New(role, content);
  };
  std::vector<blink::mojom::AILanguageModelInitialPromptPtr> initial_prompts{};
  initial_prompts.push_back(
      create_initial_prompt(Role::kUser, kTestInitialPromptsUser1));
  initial_prompts.push_back(
      create_initial_prompt(Role::kAssistant, kTestInitialPromptsSystem1));
  initial_prompts.push_back(
      create_initial_prompt(Role::kUser, kTestInitialPromptsUser2));
  return initial_prompts;
}

std::string GetContextString(AILanguageModel::Context& ctx) {
  auto msg = ctx.MakeRequest();
  auto* v = static_cast<optimization_guide::proto::StringValue*>(msg.get());
  return v->value();
}

AILanguageModel::Context::ContextItem SimpleContextItem(std::string text,
                                                        uint32_t size) {
  auto item = AILanguageModel::Context::ContextItem();
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

const optimization_guide::proto::Any& GetPromptApiMetadata(
    bool use_prompt_api_proto,
    bool is_streaming_chunk_by_chunk) {
  static base::NoDestructor<
      std::map<std::pair<bool, bool>, optimization_guide::proto::Any>>
      metadata_map;
  auto key = std::make_pair(use_prompt_api_proto, is_streaming_chunk_by_chunk);

  if (metadata_map->find(key) == metadata_map->end()) {
    metadata_map->emplace(key, [use_prompt_api_proto,
                                is_streaming_chunk_by_chunk]() {
      optimization_guide::proto::PromptApiMetadata metadata;
      metadata.set_version(
          use_prompt_api_proto ? AILanguageModel::kMinVersionUsingProto : 0);
      metadata.set_is_streaming_chunk_by_chunk(is_streaming_chunk_by_chunk);
      return optimization_guide::AnyWrapProto(metadata);
    }());
  }

  return metadata_map->at(key);
}

}  // namespace

class AILanguageModelTest : public AITestUtils::AITestBase,
                            public testing::WithParamInterface<
                                /*is_model_streaming_chunk_by_chunk=*/bool> {
 public:
  struct Options {
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params = nullptr;
    std::optional<std::string> system_prompt = std::nullopt;
    std::vector<blink::mojom::AILanguageModelInitialPromptPtr> initial_prompts;
    std::string prompt_input = kTestPrompt;
    std::string expected_context = "";
    std::string expected_cloned_context =
        base::StrCat({kExpectedFormattedTestPrompt, kTestResponse, "\n"});
    std::string expected_prompt = kExpectedFormattedTestPrompt;
    bool use_prompt_api_proto = false;
    bool should_overflow_context = false;
  };

  void SetUp() override {
    AITestUtils::AITestBase::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
            features::kAILanguageModelOverrideConfiguration,
            {{"max_top_k", base::NumberToString(kOverrideMaxTopK)}})},
        {});
  }

 protected:
  bool IsModelStreamingChunkByChunk() { return GetParam(); }

  // The helper function that creates a `AILanguageModel` and executes the
  // prompt.
  void RunPromptTest(Options options) {
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params_copy;
    if (options.sampling_params) {
      sampling_params_copy = options.sampling_params->Clone();
    }

    // Set up mock service.
    SetupMockOptimizationGuideKeyedService();
    // When the sampling param is not specified, `StartSession()` will run three
    // times:
    // 1. when getting the default sampling params.
    // 2. when creating the session.
    // 3. when cloning the session.
    // Other wise, it will run twice as the first one is unnecessary.
    auto& expectation =
        EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
            .Times(sampling_params_copy ? 2 : 3);
    if (!sampling_params_copy) {
      expectation.WillOnce(
          [&](optimization_guide::ModelBasedCapabilityKey feature,
              const std::optional<optimization_guide::SessionConfigParams>&
                  config_params) {
            auto session = std::make_unique<
                testing::NiceMock<optimization_guide::MockSession>>();
            SetUpMockSession(*session, options.use_prompt_api_proto,
                             IsModelStreamingChunkByChunk());
            ON_CALL(*session, GetSamplingParams())
                .WillByDefault(
                    [&]() -> const optimization_guide::SamplingParams {
                      return optimization_guide::SamplingParams{
                          .top_k = kDefaultTopK,
                          .temperature = kDefaultTemperature};
                    });

            return session;
          });
    }
    expectation
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

          SetUpMockSession(*session, options.use_prompt_api_proto,
                           IsModelStreamingChunkByChunk());

          ON_CALL(*session, GetContextSizeInTokens(_, _))
              .WillByDefault(
                  [&](const google::protobuf::MessageLite& request_metadata,
                      optimization_guide::
                          OptimizationGuideModelSizeInTokenCallback callback) {
                    std::move(callback).Run(
                        options.should_overflow_context
                            ? AITestUtils::GetFakeTokenLimits()
                                      .max_context_tokens +
                                  1
                            : 1);
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
        })
        .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature,
                      const std::optional<
                          optimization_guide::SessionConfigParams>&
                          config_params) {
          auto session = std::make_unique<
              testing::NiceMock<optimization_guide::MockSession>>();

          SetUpMockSession(*session, options.use_prompt_api_proto,
                           IsModelStreamingChunkByChunk());

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
    mojo::Remote<blink::mojom::AILanguageModel> mock_session;
    AITestUtils::MockCreateLanguageModelClient
        mock_create_language_model_client;
    base::RunLoop creation_run_loop;
    EXPECT_CALL(mock_create_language_model_client, OnResult(_, _))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<blink::mojom::AILanguageModel>
                    language_model,
                blink::mojom::AILanguageModelInfoPtr info) {
              EXPECT_TRUE(language_model);
              mock_session = mojo::Remote<blink::mojom::AILanguageModel>(
                  std::move(language_model));
              creation_run_loop.Quit();
            }));

    mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();

    EXPECT_EQ(GetAIManagerDownloadProgressObserversSize(), 0u);
    AITestUtils::MockModelDownloadProgressMonitor mock_monitor;
    base::RunLoop download_progress_run_loop;
    EXPECT_CALL(mock_monitor, OnDownloadProgressUpdate(_, _))
        .WillOnce(testing::Invoke(
            [&](uint64_t downloaded_bytes, uint64_t total_bytes) {
              EXPECT_EQ(downloaded_bytes, kTestModelDownloadSize);
              EXPECT_EQ(total_bytes, kTestModelDownloadSize);
              download_progress_run_loop.Quit();
            }));

    mock_remote->AddModelDownloadProgressObserver(
        mock_monitor.BindNewPipeAndPassRemote());
    ASSERT_TRUE(base::test::RunUntil(
        [this] { return GetAIManagerDownloadProgressObserversSize() == 1u; }));

    MockDownloadProgressUpdate(kTestModelDownloadSize, kTestModelDownloadSize);
    download_progress_run_loop.Run();

    mock_remote->CreateLanguageModel(
        mock_create_language_model_client.BindNewPipeAndPassRemote(),
        blink::mojom::AILanguageModelCreateOptions::New(
            std::move(options.sampling_params), options.system_prompt,
            std::move(options.initial_prompts)));
    creation_run_loop.Run();

    AITestUtils::MockModelStreamingResponder mock_responder;

    TestPromptCall(mock_session, options.prompt_input,
                   options.should_overflow_context);

    // Test session cloning.
    mojo::Remote<blink::mojom::AILanguageModel> mock_cloned_session;
    AITestUtils::MockCreateLanguageModelClient mock_clone_language_model_client;
    base::RunLoop clone_run_loop;
    EXPECT_CALL(mock_clone_language_model_client, OnResult(_, _))
        .WillOnce(testing::Invoke(
            [&](mojo::PendingRemote<blink::mojom::AILanguageModel>
                    language_model,
                blink::mojom::AILanguageModelInfoPtr info) {
              EXPECT_TRUE(language_model);
              mock_cloned_session = mojo::Remote<blink::mojom::AILanguageModel>(
                  std::move(language_model));
              clone_run_loop.Quit();
            }));

    mock_session->Fork(
        mock_clone_language_model_client.BindNewPipeAndPassRemote());
    clone_run_loop.Run();

    TestPromptCall(mock_cloned_session, options.prompt_input,
                   /*should_overflow_context=*/false);
  }

 private:
  optimization_guide::OptimizationGuideModelStreamingExecutionResult
  CreateExecutionResult(const std::string& output, bool is_complete) {
    optimization_guide::proto::StringValue response;
    response.set_value(output);
    return optimization_guide::OptimizationGuideModelStreamingExecutionResult(
        optimization_guide::StreamingResponse{
            .response = optimization_guide::AnyWrapProto(response),
            .is_complete = is_complete,
        },
        /*provided_by_on_device=*/true);
  }

  void SetUpMockSession(
      testing::NiceMock<optimization_guide::MockSession>& session,
      bool use_prompt_api_proto,
      bool is_streaming_chunk_by_chunk) {
    ON_CALL(session, GetTokenLimits())
        .WillByDefault(AITestUtils::GetFakeTokenLimits);

    ON_CALL(session, GetOnDeviceFeatureMetadata())
        .WillByDefault(ReturnRef(GetPromptApiMetadata(
            use_prompt_api_proto, is_streaming_chunk_by_chunk)));
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
    ON_CALL(session, GetExecutionInputSizeInTokens(_, _))
        .WillByDefault(
            [](const google::protobuf::MessageLite& request_metadata,
               optimization_guide::OptimizationGuideModelSizeInTokenCallback
                   callback) {
              std::move(callback).Run(ToString(request_metadata).size());
            });
    ON_CALL(session, GetContextSizeInTokens(_, _))
        .WillByDefault(
            [](const google::protobuf::MessageLite& request_metadata,
               optimization_guide::OptimizationGuideModelSizeInTokenCallback
                   callback) {
              std::move(callback).Run(ToString(request_metadata).size());
            });
  }

  void TestPromptCall(mojo::Remote<blink::mojom::AILanguageModel>& mock_session,
                      std::string& prompt,
                      bool should_overflow_context) {
    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop responder_run_loop;
    EXPECT_CALL(mock_responder, OnStreaming(_))
        .WillOnce(testing::Invoke([&](const std::string& text) {
          EXPECT_THAT(text, kTestResponse);
        }));

    EXPECT_CALL(mock_responder, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              responder_run_loop.Quit();
            }));

    mock_session->Prompt(prompt, mock_responder.BindNewPipeAndPassRemote());
    responder_run_loop.Run();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AILanguageModelTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "IsModelStreamingChunkByChunk"
                                      : "IsModelStreamingWithCurrentResponse";
                         });

TEST_P(AILanguageModelTest, PromptDefaultSession) {
  RunPromptTest(AILanguageModelTest::Options{
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest, PromptSessionWithSamplingParams) {
  RunPromptTest(AILanguageModelTest::Options{
      .sampling_params = blink::mojom::AILanguageModelSamplingParams::New(
          /*top_k=*/kOverrideMaxTopK - 1, /*temperature=*/0.6),
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest, PromptSessionWithSamplingParams_ExceedMaxTopK) {
  RunPromptTest(AILanguageModelTest::Options{
      .sampling_params = blink::mojom::AILanguageModelSamplingParams::New(
          /*top_k=*/kOverrideMaxTopK + 1, /*temperature=*/0.6),
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest, PromptSessionWithSystemPrompt) {
  RunPromptTest(AILanguageModelTest::Options{
      .system_prompt = kTestSystemPrompts,
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedSystemPrompts,
      .expected_cloned_context =
          base::StrCat({kExpectedFormattedSystemPrompts,
                        kExpectedFormattedTestPrompt, kTestResponse, "\n"}),
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest, PromptSessionWithInitialPrompts) {
  RunPromptTest(AILanguageModelTest::Options{
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedInitialPrompts,
      .expected_cloned_context =
          base::StrCat({kExpectedFormattedInitialPrompts,
                        kExpectedFormattedTestPrompt, kTestResponse, "\n"}),
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest, PromptSessionWithSystemPromptAndInitialPrompts) {
  RunPromptTest(AILanguageModelTest::Options{
      .system_prompt = kTestSystemPrompts,
      .initial_prompts = GetTestInitialPrompts(),
      .prompt_input = kTestPrompt,
      .expected_context = kExpectedFormattedSystemPromptAndInitialPrompts,
      .expected_cloned_context = base::StrCat(
          {kExpectedFormattedSystemPrompts, kExpectedFormattedInitialPrompts,
           kExpectedFormattedTestPrompt, kTestResponse, "\n"}),
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest, PromptSessionWithPromptApiRequests) {
  RunPromptTest(AILanguageModelTest::Options{
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

TEST_P(AILanguageModelTest, PromptSessionWithContextOverflow) {
  RunPromptTest({.prompt_input = kTestPrompt,
                 .expected_prompt = kExpectedFormattedTestPrompt,
                 .should_overflow_context = true});
}

// Tests `AILanguageModel::Context` creation without initial prompts.
TEST(AILanguageModelContextCreationTest, CreateContext_WithoutInitialPrompts) {
  AILanguageModel::Context context(kTestMaxContextToken, {},
                                   /*use_prompt_api_request*/ false);
  EXPECT_FALSE(context.HasContextItem());
}

// Tests `AILanguageModel::Context` creation with valid initial prompts.
TEST(AILanguageModelContextCreationTest,
     CreateContext_WithInitialPrompts_Normal) {
  AILanguageModel::Context context(
      kTestMaxContextToken,
      SimpleContextItem("initial prompts\n", kTestInitialPromptsToken),
      /*use_prompt_api_request*/ false);
  EXPECT_TRUE(context.HasContextItem());
}

// Tests `AILanguageModel::Context` creation with initial prompts that exceeds
// the max token limit.
TEST(AILanguageModelContextCreationTest,
     CreateContext_WithInitialPrompts_Overflow) {
  EXPECT_DEATH_IF_SUPPORTED(AILanguageModel::Context context(
                                kTestMaxContextToken,
                                SimpleContextItem("long initial prompts\n",
                                                  kTestMaxContextToken + 1u),
                                /*use_prompt_api_request*/ false),
                            "");
}

// Tests the `AILanguageModel::Context` that's initialized with/without any
// initial prompt.
class AILanguageModelContextTest : public testing::Test,
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

  AILanguageModel::Context context_{
      kTestMaxContextToken,
      IsInitializedWithInitialPrompts()
          ? SimpleContextItem("initial prompts", kTestInitialPromptsToken)
          : AILanguageModel::Context::ContextItem(),
      /*use_prompt_api_request*/ false};
};

INSTANTIATE_TEST_SUITE_P(All,
                         AILanguageModelContextTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithInitialPrompts"
                                             : "WithoutInitialPrompts";
                         });

// Tests `GetContextString()` and `HasContextItem()` when the context is empty.
TEST_P(AILanguageModelContextTest, TestContextOperation_Empty) {
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix());

  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}

// Tests `GetContextString()` and `HasContextItem()` when some items are added
// to the context.
TEST_P(AILanguageModelContextTest, TestContextOperation_NonEmpty) {
  context_.AddContextItem(SimpleContextItem("test", 1u));
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix() + "test\n");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem(SimpleContextItem(" test again", 2u));
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "test\n test again\n");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_P(AILanguageModelContextTest, TestContextOperation_Overflow) {
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
TEST_P(AILanguageModelContextTest, TestContextOperation_OverflowOnFirstItem) {
  context_.AddContextItem(
      SimpleContextItem("test very long token", GetMaxContextToken() + 1u));
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix());
  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}
