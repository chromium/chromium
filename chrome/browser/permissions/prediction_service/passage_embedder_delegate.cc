// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/passage_embedder_delegate.h"

#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_uma_util.h"

namespace permissions {
using passage_embeddings::Embedder;

PassageEmbedderDelegate::~PassageEmbedderDelegate() = default;
PassageEmbedderDelegate::PassageEmbedderDelegate(Profile* profile)
    : profile_(profile) {}

Embedder* PassageEmbedderDelegate::get_passage_embedder() {
  if (auto* prediction_model_handler_provider =
          PredictionModelHandlerProviderFactory::GetForBrowserContext(
              profile_)) {
    bool is_ready = prediction_model_handler_provider->IsPassageEmbedderReady();
    PermissionUmaUtil::RecordPassageEmbedderMetadataValid(is_ready);
    if (!is_ready) {
      VLOG(1) << "[PermissionsAIv4] "
                 "PassageEmbedderDelegate::get_passage_embedder is not ready.";
      return nullptr;
    }
    return prediction_model_handler_provider->GetPassageEmbedder();
  }
  PermissionUmaUtil::RecordPassageEmbedderMetadataValid(false);
  return nullptr;
}

void PassageEmbedderDelegate::CreatePassageEmbeddingFromRenderedText(
    std::string rendered_text,
    PassageEmbeddingsComputedCallback on_passage_embeddings_computed,
    base::OnceCallback<void()> fallback_callback) {
  VLOG(1) << "[PermissionsAIv4] "
             "PassageEmbedderDelegate::CreatePassageEmbeddingFromRenderedText";
  DCHECK(rendered_text.size() != 0);

  fallback_callback_ = std::move(fallback_callback);
  on_passage_embeddings_computed_ = std::move(on_passage_embeddings_computed);
  if (Embedder* passage_embedder = get_passage_embedder()) {
    bool previous_task_needs_canceling =
        (passage_embeddings_task_id_ != std::nullopt);
    PermissionUmaUtil::RecordTryCancelPreviousEmbeddingsModelExecution(
        PredictionModelType::kOnDeviceAiV4Model, previous_task_needs_canceling);

    if (previous_task_needs_canceling) {
      VLOG(1) << "[PermissionsAIv4]: The embedding task did not return yet.";
      // Try to cancel the embedding task for the previous query, if any.
      passage_embedder->TryCancel(*passage_embeddings_task_id_);
    }

    VLOG(1)
        << "[PermissionsAIv4]: Starting Embedder::ComputePassagesEmbeddings";
    passage_embeddings_task_id_ = passage_embedder->ComputePassagesEmbeddings(
        passage_embeddings::PassagePriority::kUserInitiated,
        {std::move(rendered_text)},
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
  passage_embeddings_task_id_ = std::nullopt;
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
    passage_embeddings::Embedder::TaskId task_id,
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

  if (!succeeded) {
    if (passage_embeddings_task_id_ == task_id) {
      passage_embeddings_task_id_ = std::nullopt;
    }
    return std::move(fallback_callback_).Run();
  }

  DCHECK(passages.size() == 1);

  bool is_outdated_task = passage_embeddings_task_id_ != task_id;
  PermissionUmaUtil::RecordFinishedPassageEmbeddingsTaskOutdated(
      PredictionModelType::kOnDeviceAiV4Model, is_outdated_task);
  if (is_outdated_task) {
    // If the task id is different, a new permission request has started
    // in the meantime and the request that started this call is stale.
    return;
  } else {
    passage_embeddings_task_id_ = std::nullopt;
  }

  std::move(on_passage_embeddings_computed_).Run(std::move(embeddings[0]));
}

}  // namespace permissions
