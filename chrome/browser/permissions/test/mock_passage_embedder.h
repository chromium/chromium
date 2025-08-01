// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_TEST_MOCK_PASSAGE_EMBEDDER_H_
#define CHROME_BROWSER_PERMISSIONS_TEST_MOCK_PASSAGE_EMBEDDER_H_

#include <string>
#include <vector>

#include "components/passage_embeddings/passage_embeddings_test_util.h"

namespace test {

class PassageEmbedderMock : public passage_embeddings::TestEmbedder {
 public:
  PassageEmbedderMock() = default;
  ~PassageEmbedderMock() override = default;

  // passage_embeddings::TestEmbedder:
  passage_embeddings::Embedder::TaskId ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;

  void set_status(passage_embeddings::ComputeEmbeddingsStatus status);

 private:
  passage_embeddings::ComputeEmbeddingsStatus status_ =
      passage_embeddings::ComputeEmbeddingsStatus::kSuccess;
};
}  // namespace test

#endif  // CHROME_BROWSER_PERMISSIONS_TEST_MOCK_PASSAGE_EMBEDDER_H_
