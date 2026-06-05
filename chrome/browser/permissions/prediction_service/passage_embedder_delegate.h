// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PASSAGE_EMBEDDER_DELEGATE_H_
#define CHROME_BROWSER_PERMISSIONS_PREDICTION_SERVICE_PASSAGE_EMBEDDER_DELEGATE_H_

#include <string>

#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace permissions {

// A delegate class that computes passage embeddings from rendered text.
// This class is responsible for interacting with the passage embedder model,
// handling timeouts, and managing the lifecycle of embedding jobs.
class PassageEmbedderDelegate {
 public:
  explicit PassageEmbedderDelegate(Profile* profile);
  virtual ~PassageEmbedderDelegate();
  // The timeout for the passage embedding computation in seconds. If the
  // passage embeddings computation takes longer than this, the fallback
  // callback will be invoked.
  static const int kPassageEmbedderDelegateTimeout = 1;

  // A callback that is run when the passage embedding has been successfully
  // computed.
  using PassageEmbeddingsComputedCallback =
      base::OnceCallback<void(std::vector<float> passage_embedding)>;

  // Computes passage embeddings from the given `text`.
  // The `text` is split into `passage_count` chunks, each of size
  // `kPageContentMaxLength`.
  // This function will cancel any pending embedding jobs before starting a new
  // one. On success, `callback` is invoked with the computed averaged
  // embedding. If the computation fails or times out, `fallback_callback` is
  // invoked.
  void CreatePassageEmbeddingsFromRenderedText(
      std::string text,
      int passage_count,
      PassageEmbeddingsComputedCallback callback,
      base::OnceCallback<void()> fallback_callback);

  // Clears the job ID.
  void Reset();

 protected:
  virtual passage_embeddings::Embedder* GetPassageEmbedder();

 private:
  // Callback for the passage embeddings model.
  // This function is called when the passage embedder model has finished its
  // computation. It handles the result, including checking for success,
  // managing job IDs, and invoking the appropriate callback.
  void OnPassageEmbeddingsComputed(
      base::TimeTicks model_inquire_start_time,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      uint64_t job_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // Called when the passage embedding computation times out.
  // This will invoke the `fallback_callback_`.
  void OnTimeout();

  // The handle for the current passage embedding job. This is used to
  // automatically cancel a still running embedding job for a previous,
  // stale query when it is reset or replaced by a new job.
  std::optional<passage_embeddings::Embedder::Job> passage_embeddings_job_;

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
