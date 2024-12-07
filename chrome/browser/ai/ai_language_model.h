// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_
#define CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_

#include <deque>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

class AIManager;

// The implementation of `blink::mojom::AILanguageModel`, which exposes the APIs
// for model execution.
class AILanguageModel : public AIContextBoundObject,
                        public blink::mojom::AILanguageModel {
 public:
  using PromptApiPrompt = optimization_guide::proto::PromptApiPrompt;
  using PromptApiRequest = optimization_guide::proto::PromptApiRequest;
  using CreateLanguageModelCallback = base::OnceCallback<void(
      base::expected<mojo::PendingRemote<blink::mojom::AILanguageModel>,
                     blink::mojom::AIManagerCreateLanguageModelError>,
      blink::mojom::AILanguageModelInfoPtr)>;

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

    Context(uint32_t max_tokens,
            ContextItem initial_prompts,
            bool use_prompt_api_proto);
    Context(const Context&);
    ~Context();

    // Insert a new context item, this may evict some oldest items to ensure the
    // total number of tokens in the context is below the limit.
    // It returns whether the context overflows and some existing item gets
    // evicted.
    bool AddContextItem(ContextItem context_item);

    // Combines the initial prompts and all current items into a request.
    // The type of request produced is either PromptApiRequest or StringValue,
    // depending on use_prompt_api_proto = true.
    std::unique_ptr<google::protobuf::MessageLite> MakeRequest();

    // Either returns it's argument wrapped in unique_ptr, or converts it to a
    // StringValue depending on whether this Context has
    // use_prompt_api_proto = true.
    std::unique_ptr<google::protobuf::MessageLite> MaybeFormatRequest(
        PromptApiRequest request);

    // Returns true if the system prompt is set or there is at least one context
    // item.
    bool HasContextItem();

    uint32_t max_tokens() const { return max_tokens_; }
    uint32_t current_tokens() const { return current_tokens_; }
    bool use_prompt_api_proto() const { return use_prompt_api_proto_; }

   private:
    uint32_t max_tokens_;
    uint32_t current_tokens_ = 0;
    ContextItem initial_prompts_;
    std::deque<ContextItem> context_items_;
    // Whether this should use PromptApiRequest or StringValue as request type.
    bool use_prompt_api_proto_;
  };

  AILanguageModel(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      base::WeakPtr<content::BrowserContext> browser_context,
      mojo::PendingRemote<blink::mojom::AILanguageModel> pending_remote,
      AIContextBoundObjectSet& session_set,
      AIManager& ai_manager,
      const std::optional<const Context>& context = std::nullopt);
  AILanguageModel(const AILanguageModel&) = delete;
  AILanguageModel& operator=(const AILanguageModel&) = delete;

  ~AILanguageModel() override;

  // `blink::mojom::AILanguageModel` implementation.
  void Prompt(const std::string& input,
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
      std::vector<blink::mojom::AILanguageModelInitialPromptPtr>
          initial_prompts,
      CreateLanguageModelCallback callback);
  blink::mojom::AILanguageModelInfoPtr GetLanguageModelInfo();
  mojo::PendingRemote<blink::mojom::AILanguageModel> TakePendingRemote();

 private:
  void ModelExecutionCallback(
      const PromptApiRequest& input,
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void InitializeContextWithInitialPrompts(
      optimization_guide::proto::PromptApiRequest request,
      CreateLanguageModelCallback callback,
      uint32_t size);

  // This function is passed as a completion callback to the
  // `GetSizeInTokens()`. It will
  // - Add the item into context, and remove the oldest items to reduce the
  // context size if the number of tokens in the current context exceeds the
  // limit.
  // - Signal the completion of model execution through the `responder` with the
  // new size of the context.
  void AddPromptHistoryAndSendCompletion(
      const PromptApiRequest& history_item,
      blink::mojom::ModelStreamingResponder* responder,
      uint32_t size);

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

  bool is_streaming_chunk_by_chunk_;
  // The accumulated current response to simulate the old streaming behavior
  // that always returns all the response generated so far.
  std::string current_response_;

  mojo::PendingRemote<blink::mojom::AILanguageModel> pending_remote_;
  mojo::Receiver<blink::mojom::AILanguageModel> receiver_;

  base::WeakPtrFactory<AILanguageModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_
