// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/test/mock_passage_embedder.h"

#include <string>
#include <vector>

namespace test {

using passage_embeddings::ComputeEmbeddingsStatus;
using passage_embeddings::PassagePriority;
using passage_embeddings::TestEmbedder;
using TaskId = passage_embeddings::Embedder::TaskId;

TaskId PassageEmbedderMock::ComputePassagesEmbeddings(
    PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  if (status_ == ComputeEmbeddingsStatus::kSuccess) {
    TestEmbedder::ComputePassagesEmbeddings(priority, passages,
                                            std::move(callback));
    return 0;
  }

  std::move(callback).Run(passages, {}, 0, status_);
  return 0;
}

void PassageEmbedderMock::set_status(ComputeEmbeddingsStatus status) {
  status_ = status;
}
}  // namespace test
