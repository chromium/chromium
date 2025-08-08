// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PASSAGE_EMBEDDER_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PASSAGE_EMBEDDER_DELEGATE_H_

#include <string>

#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace permissions {

// A delegate class that computes passage embeddings from rendered text.
// This class is responsible for interacting with the passage embedder model,
// handling timeouts, and managing the lifecycle of embedding tasks.
class PassageEmbedderDelegate {
 public:
  explicit PassageEmbedderDelegate(Profile* profile);
  ~PassageEmbedderDelegate();

  // The timeout for the passage embedding computation in seconds. If the
  // passage embeddings computation takes longer than this, the fallback
  // callback will be invoked.
  static const int kPassageEmbedderDelegateTimeout = 1;

  // A callback that is run when the passage embedding has been successfully
  // computed.
  using PassageEmbeddingsComputedCallback =
      base::OnceCallback<void(passage_embeddings::Embedding passage_embedding)>;

  // Computes a passage embedding from the given `rendered_text`.
  // This function will cancel any pending embedding tasks before starting a new
  // one. On success, `callback` is invoked with the computed embedding.
  // If the computation fails or times out, `fallback_callback` is invoked.
  void CreatePassageEmbeddingFromRenderedText(
      std::string rendered_text,
      PassageEmbeddingsComputedCallback callback,
      base::OnceCallback<void()> fallback_callback);

  // Clears the task ID.
  void Reset();

 private:
  // Callback for the passage embeddings model.
  // This function is called when the passage embedder model has finished its
  // computation. It handles the result, including checking for success,
  // managing task IDs, and invoking the appropriate callback.
  void OnPassageEmbeddingsComputed(
      base::TimeTicks model_inquire_start_time,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      passage_embeddings::Embedder::TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // Called when the passage embedding computation times out.
  // This will invoke the `fallback_callback_`.
  void OnTimeout();

  passage_embeddings::Embedder* get_passage_embedder();

  // The ID of the current passage embedding task. This is used to cancel
  // a still running embedding task for a previous, stale query.
  std::optional<passage_embeddings::Embedder::TaskId>
      passage_embeddings_task_id_;

  // The profile used to access the passage embedder model.
  raw_ptr<Profile> profile_;

  // Called when passage embeddings were computed successfully.
  PassageEmbeddingsComputedCallback on_passage_embeddings_computed_;

  // Called when passage computation takes longer than the timeout or when
  // passage embedding computation status is not kSuccess.
  base::OnceCallback<void()> fallback_callback_;

  // A timer to enforce the `kPassageEmbedderDelegateTimeout`.
  base::OneShotTimer timeout_timer_;

  // Used for the timer to bind OnTimeout as a callback.
  base::WeakPtrFactory<PassageEmbedderDelegate> weak_ptr_factory_{this};
};

}  // namespace permissions
#endif  // CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PASSAGE_EMBEDDER_DELEGATE_H_
