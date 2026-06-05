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

// ---------------------------------------------------------------------------
// --------------------------- PassageEmbedderMock ---------------------------
// ---------------------------------------------------------------------------

PassageEmbedderMock::PassageEmbedderMock() = default;
PassageEmbedderMock::~PassageEmbedderMock() = default;

passage_embeddings::Embedder::Job
PassageEmbedderMock::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  last_passages_ = passages;
  const uint64_t job_id = next_job_id_++;
  if (status_ == ComputeEmbeddingsStatus::kSuccess) {
    std::vector<passage_embeddings::Embedding> embeddings;
    for (size_t i = 0; i < passages.size(); ++i) {
      // The AIv4 test TFLite models are built with a fixed input tensor size
      // of 768. We must provide embeddings of this exact size to avoid a
      // dimension mismatch in the model executor.
      std::vector<float> data(768, 0.0f);
      data[0] = 1.0f;
      embeddings.emplace_back(std::move(data));
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), passages,
                                  std::move(embeddings), job_id, status_));
    return passage_embeddings::Embedder::Job(GetWeakPtr(), job_id);
  }

  std::move(callback).Run(passages, {}, job_id, status_);
  return passage_embeddings::Embedder::Job(GetWeakPtr(), job_id);
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
    uint64_t expected_job_id,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    uint64_t job_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  std::move(compute_embeddings_callback_)
      .Run(std::move(passages), std::move(embeddings), expected_job_id, status);
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

passage_embeddings::Embedder::Job
DelayedPassageEmbedderMock::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  const uint64_t job_id = next_job_id_++;
  compute_embeddings_callback_ = std::move(callback);
  execution_callback_ = base::BindOnce(
      &DelayedPassageEmbedderMock::OnCallbackReleased,
      weak_ptr_factory_.GetWeakPtr(), priority, std::move(passages),
      base::BindOnce(
          &DelayedPassageEmbedderMock::ComputePassageEmbeddingsCallbackWrapper,
          weak_ptr_factory_.GetWeakPtr(), job_id));

  if (on_callback_received_) {
    std::move(on_callback_received_).Run();
  }

  return passage_embeddings::Embedder::Job(GetWeakPtr(), job_id);
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
