// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/features.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-shared.h"

using testing::_;
using testing::ReturnRef;
using testing::Test;
using Role = blink::mojom::AILanguageModelInitialPromptRole;

namespace {

using optimization_guide::proto::PromptApiRequest;
using optimization_guide::proto::PromptApiRole;

constexpr uint32_t kTestMaxContextToken = 10u;
constexpr uint32_t kTestInitialPromptsToken = 5u;
constexpr uint32_t kTestDefaultTopK = 1u;
constexpr float kTestDefaultTemperature = 0.3;
constexpr uint32_t kTestMaxTopK = 5u;
constexpr float kTestMaxTemperature = 1.5;
constexpr uint64_t kTestModelDownloadSize = 572u;
static_assert(kTestDefaultTopK <= kTestMaxTopK);
static_assert(kTestDefaultTemperature <= kTestMaxTemperature);

const char kTestPrompt[] = "Test prompt";
const char kExpectedFormattedTestPrompt[] = "U: Test prompt\nM: ";
const char kTestSystemPrompts[] = "Test system prompt";
const char kExpectedFormattedSystemPrompts[] = "S: Test system prompt\n";
const char kTestResponse[] = "Test response";

const char kTestInitialPromptsUser1[] = "How are you?";
const char kTestInitialPromptsSystem1[] = "I'm fine, thank you, and you?";
const char kTestInitialPromptsUser2[] = "I'm fine too.";
const char kExpectedFormattedInitialPrompts[] =
    ("U: How are you?\n"
     "M: I'm fine, thank you, and you?\n"
     "U: I'm fine too.\n");
const char kExpectedFormattedSystemPromptAndInitialPrompts[] =
    ("S: Test system prompt\n"
     "U: How are you?\n"
     "M: I'm fine, thank you, and you?\n"
     "U: I'm fine too.\n");

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

std::string GetContextString(AILanguageModel::Context& ctx) {
  return ToString(*ctx.MakeRequest());
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
                            public testing::WithParamInterface<testing::tuple<
                                /*is_model_streaming_chunk_by_chunk=*/bool,
                                /*is_api_streaming_chunk_by_chunk=*/bool>> {
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
    bool should_use_supported_language = true;
  };

  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features{};
    std::vector<base::test::FeatureRef> disabled_features{};
    if (IsAPIStreamingChunkByChunk()) {
      disabled_features.push_back(
          features::kAILanguageModelForceStreamingFullResponse);
    } else {
      enabled_features.push_back(base::test::FeatureRefAndParams(
          features::kAILanguageModelForceStreamingFullResponse, {}));
    }
    AITestUtils::AITestBase::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

 protected:
  void SetupMockOptimizationGuideKeyedService() override {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();
    ON_CALL(*mock_optimization_guide_keyed_service_, GetSamplingParamsConfig(_))
        .WillByDefault([](optimization_guide::ModelBasedCapabilityKey feature) {
          return optimization_guide::SamplingParamsConfig{
              .default_top_k = kTestDefaultTopK,
              .default_temperature = kTestDefaultTemperature};
        });

    ON_CALL(*mock_optimization_guide_keyed_service_, GetFeatureMetadata(_))
        .WillByDefault([](optimization_guide::ModelBasedCapabilityKey feature) {
          optimization_guide::proto::SamplingParams sampling_params;
          sampling_params.set_top_k(kTestMaxTopK);
          sampling_params.set_temperature(kTestMaxTemperature);
          optimization_guide::proto::PromptApiMetadata metadata;
          *metadata.mutable_max_sampling_params() = sampling_params;
          optimization_guide::proto::Any any;
          any.set_value(metadata.SerializeAsString());
          any.set_type_url("type.googleapis.com/" + metadata.GetTypeName());
          return any;
        });
  }

  bool IsModelStreamingChunkByChunk() { return std::get<0>(GetParam()); }
  bool IsAPIStreamingChunkByChunk() { return std::get<1>(GetParam()); }

  // The helper function that creates a `AILanguageModel` and executes the
  // prompt.
  void RunPromptTest(Options options) {
    blink::mojom::AILanguageModelSamplingParamsPtr sampling_params_copy;
    if (options.sampling_params) {
      sampling_params_copy = options.sampling_params->Clone();
    }

    // Set up mock service.
    SetupMockOptimizationGuideKeyedService();

    if (options.should_use_supported_language) {
      // `StartSession()` will run twice when creating and cloning the session.
      EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
          .Times(2)
          .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature,
                        const std::optional<
                            optimization_guide::SessionConfigParams>&
                            config_params) {
            auto session = std::make_unique<
                testing::NiceMock<optimization_guide::MockSession>>();
            if (sampling_params_copy) {
              EXPECT_EQ(config_params->sampling_params->top_k,
                        std::min(kTestMaxTopK, sampling_params_copy->top_k));
              EXPECT_EQ(config_params->sampling_params->temperature,
                        std::min(kTestMaxTemperature,
                                 sampling_params_copy->temperature));
            }

            SetUpMockSession(*session, options.use_prompt_api_proto,
                             IsModelStreamingChunkByChunk());

            ON_CALL(*session, GetContextSizeInTokens(_, _))
                .WillByDefault(
                    [&](const google::protobuf::MessageLite& request_metadata,
                        optimization_guide::
                            OptimizationGuideModelSizeInTokenCallback
                                callback) {
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
                      StreamResponse(callback);
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
                      StreamResponse(callback);
                    });
            return session;
          });
    }

    // Test session creation.
    mojo::Remote<blink::mojom::AILanguageModel> mock_session;
    AITestUtils::MockCreateLanguageModelClient
        mock_create_language_model_client;
    base::RunLoop creation_run_loop;
    bool is_initial_prompts_or_system_prompt_set =
        options.initial_prompts.size() > 0 ||
        (options.system_prompt.has_value() &&
         options.system_prompt->size() > 0);
    if (options.should_use_supported_language) {
      EXPECT_CALL(mock_create_language_model_client, OnResult(_, _))
          .WillOnce([&](mojo::PendingRemote<blink::mojom::AILanguageModel>
                            language_model,
                        blink::mojom::AILanguageModelInstanceInfoPtr info) {
            EXPECT_TRUE(language_model);
            EXPECT_EQ(info->max_tokens,
                      AITestUtils::GetFakeTokenLimits().max_context_tokens);
            if (is_initial_prompts_or_system_prompt_set) {
              EXPECT_GT(info->current_tokens, 0ul);
            } else {
              EXPECT_EQ(info->current_tokens, 0ul);
            }
            mock_session = mojo::Remote<blink::mojom::AILanguageModel>(
                std::move(language_model));
            creation_run_loop.Quit();
          });
    } else {
      EXPECT_CALL(mock_create_language_model_client, OnError(_))
          .WillOnce([&](blink::mojom::AIManagerCreateClientError error) {
            EXPECT_EQ(
                error,
                blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
            creation_run_loop.Quit();
          });
    }

    mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();

    if (options.should_use_supported_language) {
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
      ASSERT_TRUE(base::test::RunUntil([this] {
        return GetAIManagerDownloadProgressObserversSize() == 1u;
      }));

      MockDownloadProgressUpdate(kTestModelDownloadSize,
                                 kTestModelDownloadSize);
      download_progress_run_loop.Run();
    }

    std::vector<blink::mojom::AILanguageCodePtr> expected_input_languages;
    if (!options.should_use_supported_language) {
      expected_input_languages.emplace_back(
          blink::mojom::AILanguageCode::New("ja"));
    }

    mock_remote->CreateLanguageModel(
        mock_create_language_model_client.BindNewPipeAndPassRemote(),
        blink::mojom::AILanguageModelCreateOptions::New(
            std::move(options.sampling_params), options.system_prompt,
            std::move(options.initial_prompts),
            std::move(expected_input_languages)));
    creation_run_loop.Run();

    if (!options.should_use_supported_language) {
      return;
    }

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
                blink::mojom::AILanguageModelInstanceInfoPtr info) {
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

  void TestSessionDestroy(
      base::OnceCallback<void(
          mojo::Remote<blink::mojom::AILanguageModel> mock_session,
          AITestUtils::MockModelStreamingResponder& mock_responder)> callback) {
    SetupMockOptimizationGuideKeyedService();
    base::OnceClosure size_in_token_callback;
    EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillOnce(
            [&](optimization_guide::ModelBasedCapabilityKey feature,
                const std::optional<optimization_guide::SessionConfigParams>&
                    config_params) {
              auto session = std::make_unique<
                  testing::NiceMock<optimization_guide::MockSession>>();

              SetUpMockSession(*session, /*use_prompt_api_proto=*/true,
                               /*is_streaming_chunk_by_chunk=*/true);
              ON_CALL(*session, GetExecutionInputSizeInTokens(_, _))
                  .WillByDefault(
                      [&](const google::protobuf::MessageLite& request_metadata,
                          optimization_guide::
                              OptimizationGuideModelSizeInTokenCallback
                                  callback) {
                        size_in_token_callback =
                            base::BindOnce(std::move(callback),
                                           ToString(request_metadata).size());
                      });

              // The model should not be executed.
              EXPECT_CALL(*session, ExecuteModel(_, _)).Times(0);
              return session;
            });

    mojo::Remote<blink::mojom::AILanguageModel> mock_session =
        CreateMockSession();

    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop responder_run_loop;

    EXPECT_CALL(mock_responder, OnError(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelStreamingResponseStatus status) {
              EXPECT_EQ(status, blink::mojom::ModelStreamingResponseStatus::
                                    kErrorSessionDestroyed);
              responder_run_loop.Quit();
            }));

    std::move(callback).Run(std::move(mock_session), mock_responder);
    // Defers the `size_in_token_callback` until the testing callback which
    // destroys the session is run.
    if (size_in_token_callback) {
      std::move(size_in_token_callback).Run();
    }
    responder_run_loop.Run();
  }

  optimization_guide::OptimizationGuideModelStreamingExecutionResult
  CreateExecutionResult(const std::string& output,
                        bool is_complete,
                        uint32_t input_token_count,
                        uint32_t output_token_count) {
    optimization_guide::proto::StringValue response;
    response.set_value(output);

    return optimization_guide::OptimizationGuideModelStreamingExecutionResult(
        optimization_guide::StreamingResponse{
            .response = optimization_guide::AnyWrapProto(response),
            .is_complete = is_complete,
            .input_token_count = input_token_count,
            .output_token_count = output_token_count},
        /*provided_by_on_device=*/true);
  }

  void TestSessionAddContext(bool should_overflow_context) {
    SetupMockOptimizationGuideKeyedService();
    // Use `max_context_token / 2 + 1` to ensure the
    // context overflow on the second prompt.
    uint32_t mock_size_in_tokens =
        should_overflow_context
            ? 1 + AITestUtils::GetFakeTokenLimits().max_context_tokens / 2
            : 1;

    EXPECT_CALL(*mock_optimization_guide_keyed_service_, StartSession(_, _))
        .WillOnce([&](optimization_guide::ModelBasedCapabilityKey feature,
                      const std::optional<
                          optimization_guide::SessionConfigParams>&
                          config_params) {
          auto session = std::make_unique<
              testing::NiceMock<optimization_guide::MockSession>>();

          SetUpMockSession(*session, /*use_prompt_api_proto=*/false,
                           IsModelStreamingChunkByChunk());

          ON_CALL(*session, GetContextSizeInTokens(_, _))
              .WillByDefault(
                  [&](const google::protobuf::MessageLite& request_metadata,
                      optimization_guide::
                          OptimizationGuideModelSizeInTokenCallback callback) {
                    std::move(callback).Run(mock_size_in_tokens);
                  });

          ON_CALL(*session, GetExecutionInputSizeInTokens(_, _))
              .WillByDefault(
                  [&](const google::protobuf::MessageLite& request_metadata,
                      optimization_guide::
                          OptimizationGuideModelSizeInTokenCallback callback) {
                    std::move(callback).Run(mock_size_in_tokens);
                  });

          // If the context is overflow, the previous prompt history should not
          // be added to the context.
          EXPECT_CALL(*session, AddContext(_))
              .Times(should_overflow_context ? 0 : 1);

          EXPECT_CALL(*session, ExecuteModel(_, _))
              .Times(2)
              .WillOnce(
                  [&](const google::protobuf::MessageLite& request_metadata,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultStreamingCallback
                              callback) {
                    EXPECT_THAT(ToString(request_metadata), "U: A\nM: ");
                    callback.Run(CreateExecutionResult(
                        "OK", /*is_complete=*/true, /*input_token_count=*/1u,
                        /*output_token_count=*/mock_size_in_tokens));
                  })
              .WillOnce(
                  [&](const google::protobuf::MessageLite& request_metadata,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultStreamingCallback
                              callback) {
                    EXPECT_THAT(ToString(request_metadata), "U: B\nM: ");
                    callback.Run(CreateExecutionResult(
                        "OK", /*is_complete=*/true, /*input_token_count=*/1u,
                        /*output_token_count=*/mock_size_in_tokens));
                  });
          return session;
        });

    mojo::Remote<blink::mojom::AILanguageModel> mock_session =
        CreateMockSession();

    AITestUtils::MockModelStreamingResponder mock_responder_1;
    AITestUtils::MockModelStreamingResponder mock_responder_2;

    base::RunLoop responder_run_loop_1;
    base::RunLoop responder_run_loop_2;

    EXPECT_CALL(mock_responder_1, OnStreaming(_, _))
        .WillOnce(testing::Invoke(
            [&](const std::string& text,
                blink::mojom::ModelStreamingResponderAction action) {
              EXPECT_THAT(text, "OK");
              EXPECT_EQ(
                  IsAPIStreamingChunkByChunk()
                      ? blink::mojom::ModelStreamingResponderAction::kAppend
                      : blink::mojom::ModelStreamingResponderAction::kReplace,
                  action);
            }));
    EXPECT_CALL(mock_responder_2, OnStreaming(_, _))
        .WillOnce(testing::Invoke(
            [&](const std::string& text,
                blink::mojom::ModelStreamingResponderAction action) {
              EXPECT_THAT(text, "OK");
              EXPECT_EQ(
                  IsAPIStreamingChunkByChunk()
                      ? blink::mojom::ModelStreamingResponderAction::kAppend
                      : blink::mojom::ModelStreamingResponderAction::kReplace,
                  action);
            }));

    EXPECT_CALL(mock_responder_2, OnContextOverflow())
        .Times(should_overflow_context ? 1 : 0);

    EXPECT_CALL(mock_responder_1, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              responder_run_loop_1.Quit();
            }));
    EXPECT_CALL(mock_responder_2, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              responder_run_loop_2.Quit();
            }));

    mock_session->Prompt("A", mock_responder_1.BindNewPipeAndPassRemote());
    responder_run_loop_1.Run();
    mock_session->Prompt("B", mock_responder_2.BindNewPipeAndPassRemote());
    responder_run_loop_2.Run();
  }

 private:
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
          /*top_k=*/kTestDefaultTopK,
          /*temperature=*/kTestDefaultTemperature};
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

  void StreamResponse(
      optimization_guide::OptimizationGuideModelExecutionResultStreamingCallback
          callback) {
    std::string responses[3];
    std::string response = std::string(kTestResponse);
    if (IsModelStreamingChunkByChunk()) {
      responses[0] = response.substr(0, 1);
      responses[1] = response.substr(1);
      responses[2] = "";
    } else {
      responses[0] = response.substr(0, 1);
      responses[1] = kTestResponse;
      responses[2] = kTestResponse;
    }
    callback.Run(CreateExecutionResult(responses[0],
                                       /*is_complete=*/false,
                                       /*input_token_count=*/1u,
                                       /*output_token_count=*/1u));
    callback.Run(CreateExecutionResult(responses[1],
                                       /*is_complete=*/false,
                                       /*input_token_count=*/1u,
                                       /*output_token_count=*/1u));
    callback.Run(CreateExecutionResult(responses[2],
                                       /*is_complete=*/true,
                                       /*input_token_count=*/1u,
                                       /*output_token_count=*/1u));
  }

  void TestPromptCall(mojo::Remote<blink::mojom::AILanguageModel>& mock_session,
                      std::string& prompt,
                      bool should_overflow_context) {
    AITestUtils::MockModelStreamingResponder mock_responder;

    base::RunLoop responder_run_loop;
    std::string response = std::string(kTestResponse);
    EXPECT_CALL(mock_responder, OnStreaming(_, _))
        .Times(3)
        .WillOnce(testing::Invoke(
            [&](const std::string& text,
                blink::mojom::ModelStreamingResponderAction action) {
              EXPECT_THAT(text, response.substr(0, 1));
              EXPECT_EQ(
                  IsAPIStreamingChunkByChunk()
                      ? blink::mojom::ModelStreamingResponderAction::kAppend
                      : blink::mojom::ModelStreamingResponderAction::kReplace,
                  action);
            }))
        .WillOnce(testing::Invoke(
            [&](const std::string& text,
                blink::mojom::ModelStreamingResponderAction action) {
              EXPECT_THAT(text, IsAPIStreamingChunkByChunk()
                                    ? response.substr(1)
                                    : kTestResponse);
              EXPECT_EQ(
                  IsAPIStreamingChunkByChunk()
                      ? blink::mojom::ModelStreamingResponderAction::kAppend
                      : blink::mojom::ModelStreamingResponderAction::kReplace,
                  action);
            }))
        .WillOnce(testing::Invoke(
            [&](const std::string& text,
                blink::mojom::ModelStreamingResponderAction action) {
              EXPECT_THAT(text,
                          IsAPIStreamingChunkByChunk() ? "" : kTestResponse);
              EXPECT_EQ(
                  IsAPIStreamingChunkByChunk()
                      ? blink::mojom::ModelStreamingResponderAction::kAppend
                      : blink::mojom::ModelStreamingResponderAction::kReplace,
                  action);
            }));

    EXPECT_CALL(mock_responder, OnCompletion(_))
        .WillOnce(testing::Invoke(
            [&](blink::mojom::ModelExecutionContextInfoPtr context_info) {
              responder_run_loop.Quit();
            }));

    mock_session->Prompt(prompt, mock_responder.BindNewPipeAndPassRemote());
    responder_run_loop.Run();
  }

  mojo::Remote<blink::mojom::AILanguageModel> CreateMockSession() {
    mojo::Remote<blink::mojom::AILanguageModel> mock_session;
    AITestUtils::MockCreateLanguageModelClient
        mock_create_language_model_client;
    base::RunLoop creation_run_loop;
    EXPECT_CALL(mock_create_language_model_client, OnResult(_, _))
        .WillOnce([&](mojo::PendingRemote<blink::mojom::AILanguageModel>
                          language_model,
                      blink::mojom::AILanguageModelInstanceInfoPtr info) {
          EXPECT_TRUE(language_model);
          mock_session = mojo::Remote<blink::mojom::AILanguageModel>(
              std::move(language_model));
          creation_run_loop.Quit();
        });

    mojo::Remote<blink::mojom::AIManager> mock_remote = GetAIManagerRemote();

    mock_remote->CreateLanguageModel(
        mock_create_language_model_client.BindNewPipeAndPassRemote(),
        blink::mojom::AILanguageModelCreateOptions::New());
    creation_run_loop.Run();

    return mock_session;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AILanguageModelTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<testing::tuple<bool, bool>>& info) {
      std::string description = "";
      description += std::get<0>(info.param)
                         ? "IsModelStreamingChunkByChunk"
                         : "IsModelStreamingWithCurrentResponse";
      description += "_";
      description += std::get<1>(info.param)
                         ? "IsAPIStreamingChunkByChunk"
                         : "IsAPIStreamingWithCurrentResponse";
      return description;
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
          /*top_k=*/kTestMaxTopK - 1,
          /*temperature=*/kTestMaxTemperature * 0.9),
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest, PromptSessionWithSamplingParams_ExceedMaxTopK) {
  RunPromptTest(AILanguageModelTest::Options{
      .sampling_params = blink::mojom::AILanguageModelSamplingParams::New(
          /*top_k=*/kTestMaxTopK + 1,
          /*temperature=*/kTestMaxTemperature * 0.9),
      .prompt_input = kTestPrompt,
      .expected_prompt = kExpectedFormattedTestPrompt,
  });
}

TEST_P(AILanguageModelTest,
       PromptSessionWithSamplingParams_ExceedMaxTemperature) {
  RunPromptTest(AILanguageModelTest::Options{
      .sampling_params = blink::mojom::AILanguageModelSamplingParams::New(
          /*top_k=*/kTestMaxTopK - 1,
          /*temperature=*/kTestMaxTemperature + 0.1),
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

TEST_P(AILanguageModelTest, PromptSessionWithUnsupportedLanguage) {
  RunPromptTest({.should_use_supported_language = true});
}

// Tests that sending `Prompt()` after destroying the session won't make a real
// call to the model.
TEST_P(AILanguageModelTest, PromptAfterDestroy) {
  TestSessionDestroy(base::BindOnce(
      [](mojo::Remote<blink::mojom::AILanguageModel> mock_session,
         AITestUtils::MockModelStreamingResponder& mock_responder) {
        mock_session->Destroy();
        mock_session->Prompt(kTestPrompt,
                             mock_responder.BindNewPipeAndPassRemote());
      }));
}

// Tests that sending `Prompt()` right before destroying the session won't make
// a real call to the model.
TEST_P(AILanguageModelTest, PromptBeforeDestroy) {
  TestSessionDestroy(base::BindOnce(
      [](mojo::Remote<blink::mojom::AILanguageModel> mock_session,
         AITestUtils::MockModelStreamingResponder& mock_responder) {
        mock_session->Prompt(kTestPrompt,
                             mock_responder.BindNewPipeAndPassRemote());
        mock_session->Destroy();
      }));
}

// Tests that the session will call `AddContext()` from the second prompt when
// there is no context overflow.
TEST_P(AILanguageModelTest, PromptWithHistoryWithoutContextOverflow) {
  TestSessionAddContext(/*should_overflow_context=*/false);
}

// Tests that the session will not call `AddContext()` from the second prompt
// when there is context overflow.
TEST_P(AILanguageModelTest, PromptWithHistoryWithContextOverflow) {
  TestSessionAddContext(/*should_overflow_context=*/true);
}

TEST_P(AILanguageModelTest, CanCreate_IsLanguagesSupported) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibility(_))
      .WillRepeatedly(testing::Return(
          optimization_guide::OnDeviceModelEligibilityReason::kSuccess));
  auto options = blink::mojom::AILanguageModelAvailabilityOptions::New(
      /*top_k=*/std::nullopt,
      /*temperature=*/std::nullopt,
      /*expected_input_languages=*/
      std::vector<blink::mojom::AILanguageCodePtr>{
          AITestUtils::ToMojoLanguageCodes({"en"})});

  base::MockCallback<AIManager::CanCreateLanguageModelCallback> callback;
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  callback.Get());
}

TEST_P(AILanguageModelTest, CanCreate_UnIsLanguagesSupported) {
  SetupMockOptimizationGuideKeyedService();
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibility(_))
      .WillRepeatedly(testing::Return(
          optimization_guide::OnDeviceModelEligibilityReason::kSuccess));
  auto options = blink::mojom::AILanguageModelAvailabilityOptions::New(
      /*top_k=*/std::nullopt,
      /*temperature=*/std::nullopt,
      /*expected_input_languages=*/
      std::vector<blink::mojom::AILanguageCodePtr>{
          AITestUtils::ToMojoLanguageCodes({"ja"})});

  base::MockCallback<AIManager::CanCreateLanguageModelCallback> callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage));
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  callback.Get());
}

// Tests `AILanguageModel::Context` creation without initial prompts.
TEST(AILanguageModelContextCreationTest, CreateContext_WithoutInitialPrompts) {
  AILanguageModel::Context context(kTestMaxContextToken, {});
  EXPECT_FALSE(context.HasContextItem());
}

// Tests `AILanguageModel::Context` creation with valid initial prompts.
TEST(AILanguageModelContextCreationTest,
     CreateContext_WithInitialPrompts_Normal) {
  AILanguageModel::Context context(
      kTestMaxContextToken,
      SimpleContextItem("initial prompts\n", kTestInitialPromptsToken));
  EXPECT_TRUE(context.HasContextItem());
}

// Tests `AILanguageModel::Context` creation with initial prompts that exceeds
// the max token limit.
TEST(AILanguageModelContextCreationTest,
     CreateContext_WithInitialPrompts_Overflow) {
  EXPECT_DEATH_IF_SUPPORTED(
      AILanguageModel::Context context(
          kTestMaxContextToken, SimpleContextItem("long initial prompts\n",
                                                  kTestMaxContextToken + 1u)),
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
    return IsInitializedWithInitialPrompts() ? "S: initial prompts\n" : "";
  }

  AILanguageModel::Context context_{
      kTestMaxContextToken,
      IsInitializedWithInitialPrompts()
          ? SimpleContextItem("initial prompts", kTestInitialPromptsToken)
          : AILanguageModel::Context::ContextItem()};
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
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("test", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "S: test\n");
  EXPECT_TRUE(context_.HasContextItem());

  context_.AddContextItem(SimpleContextItem(" test again", 2u));
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "S: test\nS:  test again\n");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow.
TEST_P(AILanguageModelContextTest, TestContextOperation_Overflow) {
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("test", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "S: test\n");
  EXPECT_TRUE(context_.HasContextItem());

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("test long token", GetMaxContextToken())),
      AILanguageModel::Context::SpaceReservationResult::kSpaceMadeAvailable);
  EXPECT_EQ(GetContextString(context_),
            GetInitialPromptsPrefix() + "S: test long token\n");
  EXPECT_TRUE(context_.HasContextItem());
}

// Tests `GetContextString()` and `HasContextItem()` when the items overflow on
// the first insertion.
TEST_P(AILanguageModelContextTest, TestContextOperation_OverflowOnFirstItem) {
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("test very long token", GetMaxContextToken() + 1u)),
      AILanguageModel::Context::SpaceReservationResult::kInsufficientSpace);
  EXPECT_EQ(GetContextString(context_), GetInitialPromptsPrefix());
  if (IsInitializedWithInitialPrompts()) {
    EXPECT_TRUE(context_.HasContextItem());
  } else {
    EXPECT_FALSE(context_.HasContextItem());
  }
}
