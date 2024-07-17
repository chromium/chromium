// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
#define CHROME_BROWSER_AI_AI_TEXT_SESSION_H_

#include <deque>

#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"

// The implementation of `blink::mojom::ModelGenericSession`, which exposes the
// single stream-based `Execute()` API for model execution.
class AITextSession : public blink::mojom::AITextSession {
 public:
  // The Context class manages the history of prompt input and output, which are
  // used to build the context when performing the next execution.
  // Context is stored in a FIFO and kept below a limited number of tokens.
  class Context {
   public:
    explicit Context(uint32_t max_tokens);
    Context(const Context&);
    ~Context();

    // Insert a new context item, this may evict some oldest items to ensure the
    // total number of tokens in the context is below the limit.
    void AddContextItem(const std::string& text, uint32_t size);
    // Puts all the texts in the context together into a string.
    std::string GetContextString();
    // Returns true if there is at least one context item, false otherwise.
    bool HasContextItem();

   private:
    // The structure storing the text in context and the number of tokens in the
    // text.
    struct ContextItem {
      std::string text;
      uint32_t tokens;
    };

    std::deque<ContextItem> context_item_;
    uint32_t max_tokens_;
    uint32_t current_tokens_ = 0;
  };

  AITextSession(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      std::optional<optimization_guide::SamplingParams> sampling_params);
  AITextSession(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      std::optional<optimization_guide::SamplingParams> sampling_params,
      Context context);
  AITextSession(const AITextSession&) = delete;
  AITextSession& operator=(const AITextSession&) = delete;

  ~AITextSession() override;

  // `blink::mojom::ModelGenericSession` implementation.
  void Prompt(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Destroy() override;

 private:
  void ModelExecutionCallback(
      const std::string& input,
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void GetContextSizeInTokensCallback(const std::string& text, uint32_t size);

  // Adds the text into context. If the number of tokens in the current context
  // exceeds the limit, remove the oldest ones to reduce the context size.
  void AppendContext(std::string& text);

  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;
  // The sampling parameters used when creating the current AITextSession.
  std::optional<optimization_guide::SamplingParams> sampling_params_;
  // Holds all the input and output from the previous prompt.
  Context context_;

  base::WeakPtrFactory<AITextSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
