// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
#define CHROME_BROWSER_AI_AI_TEXT_SESSION_H_

#include <deque>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

// The implementation of `blink::mojom::ModelGenericSession`, which exposes the
// single stream-based `Execute()` API for model execution.
class AITextSession : public AIContextBoundObject,
                      public blink::mojom::AITextSession {
 public:
  using CreateTextSessionCallback =
      base::OnceCallback<void(blink::mojom::AITextSessionInfoPtr)>;

  // The Context class manages the history of prompt input and output, which are
  // used to build the context when performing the next execution. Context is
  // stored in a FIFO and kept below a limited number of tokens.
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

    uint32_t max_tokens() const { return max_tokens_; }
    uint32_t current_tokens() const { return current_tokens_; }

   private:
    uint32_t max_tokens_;
    uint32_t current_tokens_ = 0;
    std::optional<ContextItem> system_prompt_;
    std::deque<ContextItem> context_items_;
  };

  // The `AITextSession` will be owned by the `AITextSessionSet` which is bound
  // to the `BucketContext`. However, the `deletion_callback` should be set to
  // properly remove the `AITextSession` from `AITextSessionSet` in case the
  // connection is closed before the `BucketContext` is destroyed.

  // The ownership chain of the relevant class is:
  // `BucketContext` (via `SupportsUserData` or `DocumentUserData`) --owns-->
  // `AITextSessionSet` --owns-->
  // `AITextSession` (implements blink::mojom::AITextSession) --owns-->
  // `mojo::Receiver<blink::mojom::AITextSession>`
  AITextSession(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      base::WeakPtr<content::BrowserContext> browser_context,
      mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
      AIContextBoundObjectSet* session_set,
      const std::optional<const Context>& context = std::nullopt);
  AITextSession(const AITextSession&) = delete;
  AITextSession& operator=(const AITextSession&) = delete;

  ~AITextSession() override;

  // `AIUserData` implementation.
  void SetDeletionCallback(base::OnceClosure deletion_callback) override;

  // `blink::mojom::ModelTextSession` implementation.
  void Prompt(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(mojo::PendingReceiver<blink::mojom::AITextSession> session,
            ForkCallback callback) override;
  void Destroy() override;

  // Gets the token count for the system prompt, updates the session, and passes
  // the session information back through the callback.
  void SetSystemPrompt(std::string system_prompt,
                       CreateTextSessionCallback callback);
  blink::mojom::AITextSessionInfoPtr GetTextSessionInfo();

 private:
  void ModelExecutionCallback(
      const std::string& input,
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void InitializeContextWithSystemPrompt(const std::string& text,
                                         CreateTextSessionCallback callback,
                                         uint32_t size);

  // This function is passed as a completion callback to the
  // `GetSizeInTokens()`. It will
  // - Add the text into context, and remove the oldest tokens to reduce the
  // context size if the number of tokens in the current context exceeds the
  // limit.
  // - Signal the completion of model execution through the `responder` with the
  // size returned from the `GetSizeInTokens()`.
  void OnGetSizeInTokensComplete(
      const std::string& text,
      blink::mojom::ModelStreamingResponder* responder,
      uint32_t size);

  void GetSizeInTokens(const std::string& text,
                       base::OnceCallback<uint32_t> callback);
  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;
  base::WeakPtr<content::BrowserContext> browser_context_;
  // Holds all the input and output from the previous prompt.
  std::unique_ptr<Context> context_;
  // It's safe to store a `raw_ptr` here since `this` is owned by
  // `context_bound_object_set_`.
  const raw_ptr<AIContextBoundObjectSet> context_bound_object_set_;

  mojo::Receiver<blink::mojom::AITextSession> receiver_;

  base::WeakPtrFactory<AITextSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
