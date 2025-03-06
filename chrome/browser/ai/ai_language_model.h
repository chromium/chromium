// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_
#define CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_

#include <deque>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace features {

BASE_DECLARE_FEATURE(kAILanguageModelForceStreamingFullResponse);

}  // namespace features

class AIManager;

// The implementation of `blink::mojom::AILanguageModel`, which exposes the APIs
// for model execution.
class AILanguageModel : public AIContextBoundObject,
                        public blink::mojom::AILanguageModel {
 public:
  using PromptApiPrompt = optimization_guide::proto::PromptApiPrompt;
  using PromptApiRequest = optimization_guide::proto::PromptApiRequest;
  using PromptApiMetadata = optimization_guide::proto::PromptApiMetadata;
  using CreateLanguageModelCallback = base::OnceCallback<void(
      base::expected<mojo::PendingRemote<blink::mojom::AILanguageModel>,
                     blink::mojom::AIManagerCreateClientError>,
      blink::mojom::AILanguageModelInstanceInfoPtr)>;

  // The minimum version of the model execution config for prompt API that
  // starts using proto instead of string value for the request.
  static constexpr uint32_t kMinVersionUsingProto = 2;

  // The Context class manages the history of prompt input and output, which are
  // used to build the context when performing the next execution. Context is
  // stored in a FIFO and kept below a limited number of tokens.
  class Context {
   public:
    // The structure storing the text in context and the number of tokens in the
    // text.
    struct ContextItem {
      ContextItem();
      ContextItem(const ContextItem&);
      ContextItem(ContextItem&&);
      ~ContextItem();

      google::protobuf::RepeatedPtrField<PromptApiPrompt> prompts;
      uint32_t tokens = 0;
    };

    Context(uint32_t max_tokens, ContextItem initial_prompts);
    Context(const Context&);
    ~Context();

    // The status of the result returned from `ReserveSpace()`.
    enum class SpaceReservationResult {
      // There remaining space is enough for the required tokens.
      kSufficientSpace = 0,
      // There remaining space is not enough for the required tokens, but after
      // evicting some of the oldest `ContextItem`s, it has enough space now.
      kSpaceMadeAvailable,
      // Even after evicting all the `ContextItem`s, it's not possible to make
      // enough space. In this case, no eviction will happen.
      kInsufficientSpace
    };

    // Make sure the context has at least `number_of_tokens` available, if there
    // is no enough space, the oldest `ContextItem`s will be evicted.
    SpaceReservationResult ReserveSpace(uint32_t num_tokens);

    // Insert a new context item, this may evict some oldest items to ensure the
    // total number of tokens in the context is below the limit. It returns the
    // result from the space reservation.
    SpaceReservationResult AddContextItem(ContextItem context_item);

    // Combines the initial prompts and all current items into a request.
    // The type of request produced is a PromptApiRequest.
    optimization_guide::MultimodalMessage MakeRequest();

    // Returns true if the system prompt is set or there is at least one context
    // item.
    bool HasContextItem();

    uint32_t max_tokens() const { return max_tokens_; }
    uint32_t current_tokens() const { return current_tokens_; }

   private:
    uint32_t max_tokens_;
    uint32_t current_tokens_ = 0;
    ContextItem initial_prompts_;
    std::deque<ContextItem> context_items_;
  };

  // TODO(crbug.com/385173789): Remove hacky multimodal prototype workarounds.
  class MultimodalResponder : public on_device_model::mojom::StreamingResponder,
                              public on_device_model::mojom::ContextClient {
   public:
    explicit MultimodalResponder(
        AILanguageModel* model,
        mojo::PendingReceiver<on_device_model::mojom::StreamingResponder>
            response_receiver,
        mojo::PendingReceiver<on_device_model::mojom::ContextClient>
            context_receiver,
        mojo::PendingRemote<blink::mojom::ModelStreamingResponder> responder);
    ~MultimodalResponder() override;
    // on_device_model::mojom::StreamingResponder:
    void OnResponse(on_device_model::mojom::ResponseChunkPtr chunk) override;
    void OnComplete(
        on_device_model::mojom::ResponseSummaryPtr summary) override;

    // on_device_model::mojom::ContextClient:
    void OnComplete(uint32_t tokens_processed) override;

   private:
    void OnDisconnect();

    uint32_t tokens_processed_ = 0;
    raw_ptr<AILanguageModel> model_;
    mojo::Receiver<on_device_model::mojom::StreamingResponder>
        response_receiver_;
    mojo::Receiver<on_device_model::mojom::ContextClient> context_receiver_;
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder_;
    std::string current_response_;
  };

  AILanguageModel(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      base::WeakPtr<content::BrowserContext> browser_context,
      mojo::PendingRemote<blink::mojom::AILanguageModel> pending_remote,
      AIContextBoundObjectSet& session_set,
      AIManager& ai_manager,
      AIUtils::LanguageCodes expected_input_languages,
      const std::optional<const Context>& context = std::nullopt);
  AILanguageModel(const AILanguageModel&) = delete;
  AILanguageModel& operator=(const AILanguageModel&) = delete;

  ~AILanguageModel() override;

  // Returns the the metadata parsed to the `PromptApiMetadata` from `any`.
  static PromptApiMetadata ParseMetadata(
      const optimization_guide::proto::Any& any);

  // `blink::mojom::AILanguageModel` implementation.
  void Prompt(std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client) override;
  void Destroy() override;
  void CountPromptTokens(
      const std::string& input,
      mojo::PendingRemote<blink::mojom::AILanguageModelCountPromptTokensClient>
          client) override;

  // Format the initial prompts, gets the token count, updates the session,
  // and passes the session information back through the callback.
  void SetInitialPrompts(
      const std::optional<std::string> system_prompt,
      std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
      CreateLanguageModelCallback callback);
  blink::mojom::AILanguageModelInstanceInfoPtr GetLanguageModelInstanceInfo();
  mojo::PendingRemote<blink::mojom::AILanguageModel> TakePendingRemote();

 private:
  void PromptGetInputSizeCompletion(mojo::RemoteSetElementId responder_id,
                                    Context::ContextItem current_item,
                                    uint32_t number_of_tokens);
  void ModelExecutionCallback(
      const Context::ContextItem& current_item,
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void InitializeContextWithInitialPrompts(Context::ContextItem initial_prompts,
                                           CreateLanguageModelCallback callback,
                                           uint32_t size);

  // Returns the copy of `expected_input_languages_` for the
  // `AILanguageModelInstanceInfo` or cloning.
  AIUtils::LanguageCodes GetExpectedInputLanguagesCopy();

  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;
  base::WeakPtr<content::BrowserContext> browser_context_;
  // Holds all the input and output from the previous prompt.
  std::unique_ptr<Context> context_;
  // It's safe to store `raw_ref` here since both `this` and `ai_manager_` are
  // owned by `context_bound_object_set_`, and they will be destroyed together.
  base::raw_ref<AIContextBoundObjectSet> context_bound_object_set_;
  base::raw_ref<AIManager> ai_manager_;

  AIUtils::LanguageCodes expected_input_languages_;
  bool is_on_device_session_streaming_chunk_by_chunk_;
  // The accumulated current response to simulate the old streaming behavior
  // that always returns all the response generated so far.
  std::string current_response_;

  mojo::PendingRemote<blink::mojom::AILanguageModel> pending_remote_;
  mojo::Receiver<blink::mojom::AILanguageModel> receiver_;

  // TODO(crbug.com/385173789): Remove hacky multimodal prototype workarounds.
  std::unique_ptr<MultimodalResponder> multimodal_responder_;

  base::WeakPtrFactory<AILanguageModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_
