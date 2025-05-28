// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_
#define CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_

#include <deque>
#include <optional>

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/safety_checker.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

// The implementation of `blink::mojom::AILanguageModel`, which exposes the APIs
// for model execution.
class AILanguageModel : public AIContextBoundObject,
                        public blink::mojom::AILanguageModel,
                        public optimization_guide::TextSafetyClient {
 public:
  using PromptApiMetadata = optimization_guide::proto::PromptApiMetadata;

  // The minimum version of the model execution config for prompt API that
  // starts using proto instead of string value for the request.
  static constexpr uint32_t kMinVersionUsingProto = 2;

  // The Context class manages the history of prompt input and output. Context
  // is stored in a FIFO and kept below a limited number of tokens when overflow
  // occurs.
  class Context {
   public:
    // A piece of the prompt history and it's size.
    struct ContextItem {
      ContextItem();
      ContextItem(const ContextItem&);
      ContextItem(ContextItem&&);
      ~ContextItem();

      on_device_model::mojom::InputPtr input;
      uint32_t tokens = 0;
    };

    // `max_tokens` is the number of tokens remaining after the initial prompts.
    explicit Context(uint32_t max_tokens);
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

    // Returns an input containing all of the current prompt history excluding
    // the initial prompts. This does not include prompts removed due to
    // overflow handling.
    on_device_model::mojom::InputPtr GetNonInitialPrompts();

    // The number of tokens remaining after the initial prompts.
    uint32_t max_tokens() const { return max_tokens_; }
    uint32_t current_tokens() const { return current_tokens_; }

   private:
    uint32_t max_tokens_;
    uint32_t current_tokens_ = 0;
    std::deque<ContextItem> context_items_;
  };

  AILanguageModel(AIContextBoundObjectSet& context_bound_object_set,
                  on_device_model::mojom::SessionParamsPtr session_params,
                  base::WeakPtr<optimization_guide::ModelClient> model_client,
                  mojo::PendingRemote<on_device_model::mojom::Session> session,
                  base::WeakPtr<OptimizationGuideLogger> logger);
  AILanguageModel(const AILanguageModel&) = delete;
  AILanguageModel& operator=(const AILanguageModel&) = delete;

  ~AILanguageModel() override;

  // Returns the the metadata parsed to the `PromptApiMetadata` from `any`.
  static PromptApiMetadata ParseMetadata(
      const optimization_guide::proto::Any& any);

  // Format the initial prompts, gets the token count, updates the session,
  // and reports to `create_client`.
  void Initialize(
      std::vector<blink::mojom::AILanguageModelPromptPtr> initial_prompts,
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          create_client);

  // `blink::mojom::AILanguageModel` implementation.
  void Prompt(std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
              on_device_model::mojom::ResponseConstraintPtr constraint,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Append(std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client) override;
  void Destroy() override;
  void MeasureInputUsage(
      std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
      MeasureInputUsageCallback callback) override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

  // optimization_guide::TextSafetyClient:
  void StartSession(
      mojo::PendingReceiver<on_device_model::mojom::TextSafetySession> session)
      override;

  blink::mojom::AILanguageModelInstanceInfoPtr GetLanguageModelInstanceInfo();

 private:
  mojo::PendingRemote<blink::mojom::AILanguageModel> BindRemote();

  class PromptState;
  void InitializeGetInputSizeComplete(
      on_device_model::mojom::InputPtr input,
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          create_client,
      std::optional<uint32_t> token_count);
  void InitializeSafetyChecksComplete(
      on_device_model::mojom::InputPtr input,
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          create_client,
      optimization_guide::SafetyChecker::Result safety_result);

  void ForkInternal(
      mojo::PendingRemote<blink::mojom::AIManagerCreateLanguageModelClient>
          client,
      base::OnceClosure on_complete);

  void PromptInternal(
      std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
      on_device_model::mojom::ResponseConstraintPtr constraint,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder,
      base::OnceClosure on_complete);
  void PromptGetInputSizeComplete(base::OnceClosure on_complete,
                                  std::optional<uint32_t> result);
  void OnPromptOutputComplete();

  void AppendInternal(
      std::vector<blink::mojom::AILanguageModelPromptPtr> prompts,
      mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
          pending_responder,
      base::OnceClosure on_complete);

  void HandleOverflow();
  void GetSizeInTokens(
      on_device_model::mojom::InputPtr input,
      base::OnceCallback<void(std::optional<uint32_t>)> callback);

  // These methods are used for implementing queueing.
  using QueueCallback = base::OnceCallback<void(base::OnceClosure)>;
  void AddToQueue(QueueCallback task);
  void TaskComplete();
  void RunNext();

  // Contains just the initial prompts. This should not change throughout the
  // lifetime of this object. If this object is valid, `current_session_` can
  // also be assumed to be valid, as any disconnects should apply to both
  // remotes (e.g. a service crash).
  mojo::Remote<on_device_model::mojom::Session> initial_session_;

  // Contains the current committed session state. This will be replaced after a
  // successful prompt with the latest session state.
  mojo::Remote<on_device_model::mojom::Session> current_session_;

  // The session params the initial session was created with.
  on_device_model::mojom::SessionParamsPtr session_params_;

  // Holds all the input and output from the previous prompt.
  std::unique_ptr<Context> context_;
  // It's safe to store `raw_ref` here since both `this` and `ai_manager_` are
  // owned by `context_bound_object_set_`, and they will be destroyed together.
  base::raw_ref<AIContextBoundObjectSet> context_bound_object_set_;

  // Holds the queue of operations to be run.
  base::queue<QueueCallback> queue_;
  // Whether a task is currently running.
  bool task_running_ = false;

  std::unique_ptr<optimization_guide::SafetyChecker> safety_checker_;
  base::WeakPtr<optimization_guide::ModelClient> model_client_;

  // Holds state for any currently active prompt. This holds a reference to
  // `safety_checker_` so must be ordered after that member.
  std::unique_ptr<PromptState> prompt_state_;

  base::WeakPtr<OptimizationGuideLogger> logger_;

  mojo::Receiver<blink::mojom::AILanguageModel> receiver_{this};

  base::WeakPtrFactory<AILanguageModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_LANGUAGE_MODEL_H_
