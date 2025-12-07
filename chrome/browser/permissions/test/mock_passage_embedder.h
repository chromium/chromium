// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_TEST_MOCK_PASSAGE_EMBEDDER_H_
#define CHROME_BROWSER_PERMISSIONS_TEST_MOCK_PASSAGE_EMBEDDER_H_

#include <string>
#include <vector>

#include "base/run_loop.h"
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

// A mock that simulates a delayed execution of passage embeddings computation.
// This is useful for testing asynchronous client code, for example timeout
// logic. The computation is not executed upon calling
// `ComputePassagesEmbeddings`, but is rather stored. It can be invoked later
// by calling `ReleaseCallback`.
class DelayedPassageEmbedderMock : public PassageEmbedderMock {
 public:
  DelayedPassageEmbedderMock();
  ~DelayedPassageEmbedderMock() override;

  // passage_embeddings::TestEmbedder:
  // Overrides the base class implementation to simulate a delay. Instead of
  // computing the embeddings, it captures the arguments and the callback. The
  // actual computation (in this case a fake) is deferred until
  // `ReleaseCallback` is called.
  passage_embeddings::Embedder::TaskId ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority priority,
      std::vector<std::string> passages,
      ComputePassagesEmbeddingsCallback callback) override;

  // Executes the pending embeddings computation. This will run the stored
  // callback and block until the entire asynchronous flow is complete.
  // This is necessary because the passage_embeddings::TestEmbedder uses a
  // task_runner internally to simulate an async model execution.
  void ReleaseCallback();

 private:
  // A wrapper for the `ComputePassagesEmbeddingsCallback`. This is invoked when
  // the underlying `PassageEmbedderMock` completes its computation. It forwards
  // the results to the original callback and quits the run loop to unblock the
  // test execution that called `ReleaseCallback`.
  void ComputePassageEmbeddingsCallbackWrapper(
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // An internal helper method that is bound as a callback and executed when
  // `ReleaseCallback` is called. It initiates the fake computation by calling
  // the base class's `ComputePassagesEmbeddings`.
  void OnCallbackReleased(passage_embeddings::PassagePriority priority,
                          std::vector<std::string> passages,
                          ComputePassagesEmbeddingsCallback callback);

  // Stores the pending computation, which is a call to `OnCallbackReleased`
  // with all the necessary parameters. It is set in
  // `ComputePassagesEmbeddings` and run in `ReleaseCallback`.
  base::OnceCallback<void()> execution_callback_;

  // Stores the original callback from the `ComputePassagesEmbeddings` call, so
  // it can be invoked later when the computation is unblocked.
  ComputePassagesEmbeddingsCallback compute_embeddings_callback_;

  // A run loop used to make the asynchronous execution behave synchronously for
  // tests. `ReleaseCallback` runs this loop, and
  // `ComputePassageEmbeddingsCallbackWrapper` quits it.
  base::RunLoop model_execute_run_loop_for_testing_;

  // Used for creating the execution_callback_.
  base::WeakPtrFactory<DelayedPassageEmbedderMock> weak_ptr_factory_{this};
};

class EmbedderMetadataProviderFake
    : public passage_embeddings::EmbedderMetadataProvider {
 public:
  EmbedderMetadataProviderFake();
  ~EmbedderMetadataProviderFake() override;

  // passage_embeddings::EmbedderMetadataProvider:
  void AddObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;
  void RemoveObserver(
      passage_embeddings::EmbedderMetadataObserver* observer) override;

  void NotifyObservers(passage_embeddings::EmbedderMetadata metadata);

  static passage_embeddings::EmbedderMetadata GetValidEmbedderMetadata();
  static passage_embeddings::EmbedderMetadata GetInvalidEmbedderMetadata();

 private:
  base::ObserverList<passage_embeddings::EmbedderMetadataObserver>
      observer_list_;
};

}  // namespace test

#endif  // CHROME_BROWSER_PERMISSIONS_TEST_MOCK_PASSAGE_EMBEDDER_H_
