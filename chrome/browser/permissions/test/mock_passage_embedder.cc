// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/test/mock_passage_embedder.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace test {

using passage_embeddings::ComputeEmbeddingsStatus;
using passage_embeddings::EmbedderMetadata;
using passage_embeddings::PassagePriority;
using passage_embeddings::TestEmbedder;
using TaskId = passage_embeddings::Embedder::TaskId;

// ---------------------------------------------------------------------------
// --------------------------- PassageEmbedderMock ---------------------------
// ---------------------------------------------------------------------------

PassageEmbedderMock::PassageEmbedderMock() = default;
PassageEmbedderMock::~PassageEmbedderMock() = default;

PassageEmbedderMock::PassageEmbedderMock(const PassageEmbedderMock&) = default;
PassageEmbedderMock& PassageEmbedderMock::operator=(
    const PassageEmbedderMock&) = default;

PassageEmbedderMock::PassageEmbedderMock(PassageEmbedderMock&&) = default;
PassageEmbedderMock& PassageEmbedderMock::operator=(PassageEmbedderMock&&) =
    default;

TaskId PassageEmbedderMock::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  last_passages_ = passages;
  if (status_ == ComputeEmbeddingsStatus::kSuccess) {
    std::vector<passage_embeddings::Embedding> embeddings;
    for (size_t i = 0; i < passages.size(); ++i) {
      // The AIv4 test TFLite models are built with a fixed input tensor size
      // of 768. We must provide embeddings of this exact size to avoid a
      // dimension mismatch in the model executor.
      embeddings.emplace_back(std::vector<float>(768, 1.0f));
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), passages, std::move(embeddings),
                       /*task_id=*/0, status_));
    return 0;
  }

  std::move(callback).Run(passages, {}, 0, status_);
  return 0;
}

void PassageEmbedderMock::set_status(ComputeEmbeddingsStatus status) {
  status_ = status;
}

// ---------------------------------------------------------------------------
// ----------------------- DelayedPassageEmbedderMock ------------------------
// ---------------------------------------------------------------------------

DelayedPassageEmbedderMock::DelayedPassageEmbedderMock() = default;
DelayedPassageEmbedderMock::~DelayedPassageEmbedderMock() = default;

void DelayedPassageEmbedderMock::ReleaseCallback() {
  if (execution_callback_) {
    model_execute_run_loop_ = std::make_unique<base::RunLoop>();
    std::move(execution_callback_).Run();
    model_execute_run_loop_->Run();
  }
}

void DelayedPassageEmbedderMock::ComputePassageEmbeddingsCallbackWrapper(
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    TaskId task_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  std::move(compute_embeddings_callback_)
      .Run(std::move(passages), std::move(embeddings), task_id, status);
  if (model_execute_run_loop_) {
    model_execute_run_loop_->Quit();
  }
}

void DelayedPassageEmbedderMock::OnCallbackReleased(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  PassageEmbedderMock::ComputePassagesEmbeddings(priority, std::move(passages),
                                                 std::move(callback));
}

TaskId DelayedPassageEmbedderMock::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  compute_embeddings_callback_ = std::move(callback);
  execution_callback_ = base::BindOnce(
      &DelayedPassageEmbedderMock::OnCallbackReleased,
      weak_ptr_factory_.GetWeakPtr(), priority, std::move(passages),
      base::BindOnce(
          &DelayedPassageEmbedderMock::ComputePassageEmbeddingsCallbackWrapper,
          weak_ptr_factory_.GetWeakPtr()));

  if (on_callback_received_) {
    std::move(on_callback_received_).Run();
  }

  return 0;
}

void DelayedPassageEmbedderMock::WaitForEmbedderToBeTriggered() {
  if (execution_callback_) {
    return;  // already received
  }
  base::RunLoop loop;
  on_callback_received_ = loop.QuitClosure();
  loop.Run();
}

// ---------------------------------------------------------------------------
// ----------------------- EmbedderMetadataProviderFake ----------------------
// ---------------------------------------------------------------------------

EmbedderMetadataProviderFake::EmbedderMetadataProviderFake() = default;
EmbedderMetadataProviderFake::~EmbedderMetadataProviderFake() = default;

// static
EmbedderMetadata EmbedderMetadataProviderFake::GetValidEmbedderMetadata() {
  return EmbedderMetadata(1, 768);
}

// static
EmbedderMetadata EmbedderMetadataProviderFake::GetInvalidEmbedderMetadata() {
  return EmbedderMetadata(0, 0);
}

void EmbedderMetadataProviderFake::AddObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  observer_list_.AddObserver(observer);
}

void EmbedderMetadataProviderFake::RemoveObserver(
    passage_embeddings::EmbedderMetadataObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void EmbedderMetadataProviderFake::NotifyObservers(EmbedderMetadata metadata) {
  observer_list_.Notify(
      &passage_embeddings::EmbedderMetadataObserver::EmbedderMetadataUpdated,
      std::move(metadata));
}

}  // namespace test
