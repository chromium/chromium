// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_embedding_gemma_v1_decision_model.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"
#include "components/on_device_ai/ai_utils.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

namespace {

base::unexpected<blink::mojom::AIDecisionModelDecideErrorPtr> DecideError(
    blink::mojom::ModelStreamingResponseStatus status,
    blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
  return base::unexpected(blink::mojom::AIDecisionModelDecideError::New(
      status, std::move(quota_error_info)));
}

}  // namespace

AIEmbeddingGemmaV1DecisionModel::AIEmbeddingGemmaV1DecisionModel(
    AIContextBoundObjectSet& context_bound_object_set,
    DecisionModelSchemaCompiler::CompiledSchema schema,
    passage_embeddings::Embedder& embedder,
    mojo::PendingRemote<blink::mojom::AIManagerCreateDecisionModelClient>
        client,
    uint32_t max_passage_chars)
    : AIContextBoundObject(context_bound_object_set),
      schema_(std::move(schema)),
      embedder_(embedder),
      max_passage_chars_(max_passage_chars),
      create_client_(std::move(client)) {
  create_client_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));

  std::vector<std::string> option_passages;
  uint32_t max_schema_chars = 0;
  for (size_t q = 0; q < schema_.questions.size(); ++q) {
    for (size_t o = 0; o < schema_.questions[q].options.size(); ++o) {
      std::string passage =
          DecisionModelSchemaCompiler::BuildOptionEmbeddingPassage(schema_, q,
                                                                   o);
      max_schema_chars =
          std::max(max_schema_chars, static_cast<uint32_t>(passage.size()));
      option_passages.push_back(std::move(passage));
    }
  }

  if (max_schema_chars > max_passage_chars_) {
    // Remove asynchronously after `AddContextBoundObject()` inserts `this`.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AIEmbeddingGemmaV1DecisionModel::RemoveFromSet,
                       weak_ptr_factory_.GetWeakPtr()));
    on_device_ai::SendClientRemoteError(
        create_client_,
        blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(max_schema_chars,
                                          max_passage_chars_));
    create_client_.reset();
    return;
  }

  SetPriority(context_bound_object_set.priority());
  pending_jobs_.push_back(embedder_->ComputePassagesEmbeddings(
      priority_, std::move(option_passages),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &AIEmbeddingGemmaV1DecisionModel::OnOptionEmbeddingsComputed,
          weak_ptr_factory_.GetWeakPtr()))));
}

AIEmbeddingGemmaV1DecisionModel::~AIEmbeddingGemmaV1DecisionModel() {
  if (create_client_) {
    on_device_ai::SendClientRemoteError(
        create_client_,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
  }
}

void AIEmbeddingGemmaV1DecisionModel::SetPriority(
    on_device_model::mojom::Priority priority) {
  const auto new_priority =
      priority == on_device_model::mojom::Priority::kForeground
          ? passage_embeddings::PassagePriority::kUserInitiated
          : passage_embeddings::PassagePriority::kPassive;
  if (priority_ == new_priority) {
    return;
  }
  priority_ = new_priority;
  for (passage_embeddings::Embedder::Job& job : pending_jobs_) {
    job.Reprioritize(priority_);
  }
}

void AIEmbeddingGemmaV1DecisionModel::Decide(const std::string& input,
                                             DecideCallback callback) {
  CHECK_EQ(option_embeddings_.size(), schema_.questions.size());
  if (base::TrimWhitespaceASCII(input, base::TRIM_ALL).empty()) {
    receiver_.ReportBadMessage("Invalid empty input");
    RemoveFromSet();
    return;
  }

  if (!AISemanticEmbedderServiceLauncher::Get()->AllowedToLaunch()) {
    std::move(callback).Run(DecideError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure));
    return;
  }

  std::string input_passage =
      DecisionModelSchemaCompiler::BuildInputEmbeddingPassage(input);
  const uint32_t input_chars = static_cast<uint32_t>(input_passage.size());
  if (input_chars > max_passage_chars_) {
    std::move(callback).Run(DecideError(
        blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge,
        blink::mojom::QuotaErrorInfo::New(input_chars, max_passage_chars_)));
    return;
  }

  pending_jobs_.push_back(embedder_->ComputePassagesEmbeddings(
      priority_, {std::move(input_passage)},
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &AIEmbeddingGemmaV1DecisionModel::OnInputEmbeddingsComputed,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)))));
}

void AIEmbeddingGemmaV1DecisionModel::OnOptionEmbeddingsComputed(
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    uint64_t job_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  std::erase_if(pending_jobs_,
                [job_id](const passage_embeddings::Embedder::Job& job) {
                  return job.id() == job_id;
                });
  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess ||
      embeddings.size() != passages.size()) {
    on_device_ai::SendClientRemoteError(
        create_client_,
        blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
    create_client_.reset();
    RemoveFromSet();
    return;
  }

  AISemanticEmbedderServiceLauncher::Get()->RecordSuccessfulUse();

  option_embeddings_.reserve(schema_.questions.size());
  size_t offset = 0;
  for (const auto& question : schema_.questions) {
    const size_t count = question.options.size();
    option_embeddings_.emplace_back(
        std::make_move_iterator(embeddings.begin() + offset),
        std::make_move_iterator(embeddings.begin() + offset + count));
    offset += count;
  }

  auto remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &AIContextBoundObject::RemoveFromSet, base::Unretained(this)));
  create_client_->OnSessionCreated(std::move(remote));
  create_client_.reset();
}

void AIEmbeddingGemmaV1DecisionModel::OnInputEmbeddingsComputed(
    DecideCallback callback,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    uint64_t job_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  std::erase_if(pending_jobs_,
                [job_id](const passage_embeddings::Embedder::Job& job) {
                  return job.id() == job_id;
                });

  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess ||
      embeddings.size() != 1u) {
    std::move(callback).Run(DecideError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure));
    return;
  }

  AISemanticEmbedderServiceLauncher::Get()->RecordSuccessfulUse();

  std::vector<std::vector<float>> scores;
  scores.reserve(schema_.questions.size());
  for (size_t q = 0; q < schema_.questions.size(); ++q) {
    scores.push_back(DecisionModelSchemaCompiler::ScoreQuestionEmbeddings(
        embeddings[0], option_embeddings_[q]));
  }

  auto result = DecisionModelSchemaCompiler::DecodeResult(schema_, scores);
  if (!result) {
    std::move(callback).Run(DecideError(
        blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure));
    return;
  }
  std::move(callback).Run(std::move(result));
}
