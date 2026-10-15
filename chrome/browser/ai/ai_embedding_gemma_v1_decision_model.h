// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_EMBEDDING_GEMMA_V1_DECISION_MODEL_H_
#define CHROME_BROWSER_AI_AI_EMBEDDING_GEMMA_V1_DECISION_MODEL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/decision_model_schema_compiler.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/ai/ai_decision_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"

class AIContextBoundObjectSet;

// Implements `blink::mojom::AIDecisionModel` using EmbeddingGemma V1.
class AIEmbeddingGemmaV1DecisionModel : public AIContextBoundObject,
                                        public blink::mojom::AIDecisionModel {
 public:
  // Estimated char limit per passage (~2046 tokens at ~4 chars/token; the
  // embedder silently truncates passages exceeding its 2046-token window).
  // TODO(crbug.com/565849508): Plumb actual token counts and truncation status
  // from `passage_embeddings`, or chunk long passages.
  static constexpr uint32_t kDefaultMaxPassageChars = 8192;

  AIEmbeddingGemmaV1DecisionModel(
      AIContextBoundObjectSet& context_bound_object_set,
      DecisionModelSchemaCompiler::CompiledSchema schema,
      passage_embeddings::Embedder& embedder,
      mojo::PendingRemote<blink::mojom::AIManagerCreateDecisionModelClient>
          client,
      uint32_t max_passage_chars = kDefaultMaxPassageChars);
  AIEmbeddingGemmaV1DecisionModel(const AIEmbeddingGemmaV1DecisionModel&) =
      delete;
  AIEmbeddingGemmaV1DecisionModel& operator=(
      const AIEmbeddingGemmaV1DecisionModel&) = delete;
  ~AIEmbeddingGemmaV1DecisionModel() override;

  // AIContextBoundObject:
  void SetPriority(on_device_model::mojom::Priority priority) override;

  // blink::mojom::AIDecisionModel:
  void Decide(const std::string& input, DecideCallback callback) override;

 private:
  void OnOptionEmbeddingsComputed(
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      uint64_t job_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  void OnInputEmbeddingsComputed(
      DecideCallback callback,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      uint64_t job_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  const DecisionModelSchemaCompiler::CompiledSchema schema_;
  const raw_ref<passage_embeddings::Embedder> embedder_;
  const uint32_t max_passage_chars_;
  passage_embeddings::PassagePriority priority_ =
      passage_embeddings::PassagePriority::kUserInitiated;

  // Cached option embeddings indexed by `[question_index][option_index]`.
  std::vector<std::vector<passage_embeddings::Embedding>> option_embeddings_;
  std::vector<passage_embeddings::Embedder::Job> pending_jobs_;

  mojo::Remote<blink::mojom::AIManagerCreateDecisionModelClient> create_client_;
  mojo::Receiver<blink::mojom::AIDecisionModel> receiver_{this};

  base::WeakPtrFactory<AIEmbeddingGemmaV1DecisionModel> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_EMBEDDING_GEMMA_V1_DECISION_MODEL_H_
