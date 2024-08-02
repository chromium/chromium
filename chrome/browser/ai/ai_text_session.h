// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
#define CHROME_BROWSER_AI_AI_TEXT_SESSION_H_

#include <deque>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

// The implementation of `blink::mojom::ModelGenericSession`, which exposes the
// single stream-based `Execute()` API for model execution.
class AITextSession : public blink::mojom::AITextSession {
 public:
  // The Context class manages the history of prompt input and output, which are
  // used to build the context when performing the next execution.
  // Context is stored in a FIFO and kept below a limited number of tokens.
  class Context {
   public:
    // The structure storing the text in context and the number of tokens in the
    // text.
    struct ContextItem {
      const std::string text;
      uint32_t tokens;
    };

    Context(uint32_t max_tokens, std::optional<ContextItem> system_prompt);
    Context(const Context&);
    ~Context();

    // Insert a new context item, this may evict some oldest items to ensure the
    // total number of tokens in the context is below the limit.
    void AddContextItem(ContextItem context_item);

    // Puts all the texts in the context together into a string.
    std::string GetContextString();
    // Returns true if the system prompt is set or there is at least one context
    // item.
    bool HasContextItem();
    // Clone a context with the same content.
    std::unique_ptr<Context> CloneContext();

   private:
    uint32_t max_tokens_;
    uint32_t current_tokens_ = 0;
    std::optional<ContextItem> system_prompt_;
    std::deque<ContextItem> context_items_;
  };

  AITextSession(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      std::optional<optimization_guide::SamplingParams> sampling_params,
      base::WeakPtr<content::BrowserContext> browser_context,
      const std::optional<const Context>& context = std::nullopt);
  AITextSession(const AITextSession&) = delete;
  AITextSession& operator=(const AITextSession&) = delete;

  ~AITextSession() override;

  // `blink::mojom::ModelTextSession` implementation.
  void Prompt(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(mojo::PendingReceiver<blink::mojom::AITextSession> session,
            ForkCallback callback) override;
  void Destroy() override;

  void SetSystemPrompt(std::string system_prompt,
                       base::OnceCallback<void(bool)> callback);

 private:
  void ModelExecutionCallback(
      const std::string& input,
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void InitializeContextWithSystemPrompt(const std::string& text,
                                         base::OnceCallback<void(bool)>,
                                         uint32_t size);

  // Adds the text into context. If the number of tokens in the current context
  // exceeds the limit, remove the oldest ones to reduce the context size.
  void AppendContextItem(const std::string& text, uint32_t size);

  void GetSizeInTokens(const std::string& text,
                       base::OnceCallback<uint32_t> callback);
  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;
  // The sampling parameters used when creating the current AITextSession.
  std::optional<optimization_guide::SamplingParams> sampling_params_;
  base::WeakPtr<content::BrowserContext> browser_context_;
  // Holds all the input and output from the previous prompt.
  std::unique_ptr<Context> context_;

  base::WeakPtrFactory<AITextSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
