// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/passage_embedder_delegate.h"

#include "base/logging.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_uma_util.h"

namespace permissions {

namespace {
// The maximum length of a page's content. For now, page content is limited by
// the passage embedders 64 token limit. We therefore limit the input text as
// well.
constexpr size_t kPageContentMaxLength = 500;
}  // namespace

using passage_embeddings::Embedder;

PassageEmbedderDelegate::~PassageEmbedderDelegate() = default;
PassageEmbedderDelegate::PassageEmbedderDelegate(Profile* profile)
    : profile_(profile) {}

Embedder* PassageEmbedderDelegate::GetPassageEmbedder() {
  if (auto* prediction_model_handler_provider =
          PredictionModelHandlerProviderFactory::GetForBrowserContext(
              profile_)) {
    bool is_ready = prediction_model_handler_provider->IsPassageEmbedderReady();
    PermissionUmaUtil::RecordPassageEmbedderMetadataValid(is_ready);
    if (!is_ready) {
      VLOG(1) << "[PermissionsAIv4] "
                 "PassageEmbedderDelegate::GetPassageEmbedder is not ready.";
      return nullptr;
    }
    return prediction_model_handler_provider->GetPassageEmbedder();
  }
  PermissionUmaUtil::RecordPassageEmbedderMetadataValid(false);
  return nullptr;
}

void PassageEmbedderDelegate::CreatePassageEmbeddingsFromRenderedText(
    std::string text,
    int passage_count,
    PassageEmbeddingsComputedCallback on_passage_embeddings_computed,
    base::OnceCallback<void()> fallback_callback) {
  VLOG(1) << "[PermissionsAIv4] "
             "PassageEmbedderDelegate::CreatePassageEmbeddingsFromRenderedText";
  if (text.empty() || passage_count < 1) {
    std::move(fallback_callback).Run();
    return;
  }

  fallback_callback_ = std::move(fallback_callback);
  on_passage_embeddings_computed_ = std::move(on_passage_embeddings_computed);

  // Split text into passages.
  std::vector<std::string> passages;
  size_t current_index = 0;
  while (passages.size() < static_cast<size_t>(passage_count) &&
         current_index < text.size()) {
    size_t len = std::min(kPageContentMaxLength, text.size() - current_index);
    passages.push_back(text.substr(current_index, len));
    current_index += len;
  }

  if (passages.empty()) {
    std::move(fallback_callback_).Run();
    return;
  }

  if (Embedder* passage_embedder = GetPassageEmbedder()) {
    bool previous_job_needs_canceling =
        (passage_embeddings_job_ != std::nullopt);
    PermissionUmaUtil::RecordTryCancelPreviousEmbeddingsModelExecution(
        PredictionModelType::kOnDeviceAiV4Model, previous_job_needs_canceling);

    if (previous_job_needs_canceling) {
      VLOG(1) << "[PermissionsAIv4]: The embedding job did not return yet.";
      // Try to cancel the embedding job for the previous query, if any.
      passage_embeddings_job_.reset();
    }

    VLOG(1)
        << "[PermissionsAIv4]: Starting Embedder::ComputePassagesEmbeddings";
    passage_embeddings_job_ = passage_embedder->ComputePassagesEmbeddings(
        passage_embeddings::PassagePriority::kUserInitiated,
        std::move(passages),
        base::BindOnce(&PassageEmbedderDelegate::OnPassageEmbeddingsComputed,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*model_inquire_start_time=*/base::TimeTicks::Now()));

    VLOG(1) << "[PermissionsAIv4]: Starting "
               "Embedder::ComputePassagesEmbeddings 1-second timeout_timer_";
    // We start a timer here, in case the passage embeddings computation takes
    // more than |kPassageEmbedderDelegateTimeout| seconds. It will call the
    // fallback callback if the passages embeddings computation doesn't return
    // during the timeout interval.
    timeout_timer_.Start(FROM_HERE,
                         base::Seconds(kPassageEmbedderDelegateTimeout),
                         base::BindOnce(&PassageEmbedderDelegate::OnTimeout,
                                        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  VLOG(1) << "[PermissionsAIv4]: No embedder available.";
  std::move(fallback_callback_).Run();
}

void PassageEmbedderDelegate::Reset() {
  timeout_timer_.Stop();
  passage_embeddings_job_.reset();
  on_passage_embeddings_computed_.Reset();
  fallback_callback_.Reset();
}

void PassageEmbedderDelegate::OnTimeout() {
  VLOG(1) << "[PermissionsAIv4] PassageEmbedderDelegate::OnTimeout";
  PermissionUmaUtil::RecordPassageEmbeddingsCalculationTimeout(
      /*timeout=*/true);
  if (fallback_callback_) {
    std::move(fallback_callback_).Run();
  }
}

void PassageEmbedderDelegate::OnPassageEmbeddingsComputed(
    base::TimeTicks model_inquire_start_time,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    uint64_t job_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  timeout_timer_.Stop();
  PermissionUmaUtil::RecordPassageEmbeddingsCalculationTimeout(
      /*timeout=*/false);
  bool succeeded =
      status == passage_embeddings::ComputeEmbeddingsStatus::kSuccess;

  PermissionUmaUtil::RecordPassageEmbeddingModelExecutionTimeAndStatus(
      PredictionModelType::kOnDeviceAiV4Model, model_inquire_start_time,
      status);

  VLOG(1) << "[PermissionsAIv4]: TextEmbedding computed with "
          << (succeeded ? "" : "no ") << "success.";

  if (!succeeded || passages.empty()) {
    if (passage_embeddings_job_ && passage_embeddings_job_->id() == job_id) {
      passage_embeddings_job_.reset();
    }
    return std::move(fallback_callback_).Run();
  }

  bool is_outdated_job =
      !passage_embeddings_job_ || passage_embeddings_job_->id() != job_id;
  PermissionUmaUtil::RecordFinishedPassageEmbeddingsJobOutdated(
      PredictionModelType::kOnDeviceAiV4Model, is_outdated_job);
  if (is_outdated_job) {
    // If the job ID is different, a new permission request has started
    // in the meantime and the request that started this call is stale.
    return;
  }
  passage_embeddings_job_.reset();

  if (embeddings.empty()) {
    std::move(fallback_callback_).Run();
    return;
  }

  if (embeddings.size() == 1) {
    std::move(on_passage_embeddings_computed_).Run(embeddings[0].GetData());
    return;
  }

  // We currently average the up to 5 passages we get from the passage embedder
  // as the AIv4 model expects only one vector as input. We do this by
  // computing the mathematical mean of the embedding vectors.
  // This matches how the model is trained by the AI researchers.
  std::vector<float> averaged_data(embeddings[0].GetData().size(), 0.0f);
  for (const auto& embedding : embeddings) {
    const auto& data = embedding.GetData();
    DCHECK_EQ(data.size(), averaged_data.size());
    for (size_t i = 0; i < averaged_data.size(); ++i) {
      averaged_data[i] += data[i];
    }
  }

  for (float& val : averaged_data) {
    val /= embeddings.size();
  }

  std::move(on_passage_embeddings_computed_).Run(std::move(averaged_data));
}

}  // namespace permissions
