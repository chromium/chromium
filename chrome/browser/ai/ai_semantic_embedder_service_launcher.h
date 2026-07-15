// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_

#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/core/passage_embeddings_service_launcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom-forward.h"

namespace base {
template <typename T>
class NoDestructor;
}

class AISemanticEmbedderServiceLauncher
    : public passage_embeddings::PassageEmbeddingsServiceLauncher {
 public:
  static AISemanticEmbedderServiceLauncher* Get();

  passage_embeddings::PassageEmbeddingsServiceController* controller() {
    return &controller_;
  }

  void RecordSuccessfulUse();

  // passage_embeddings::PassageEmbeddingsServiceLauncher implementation:
  void LaunchService(
      mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbeddingsService>
          receiver) override;
  void OnServiceDisconnected(bool is_idle) override;
  bool AllowedToLaunch() const override;

 protected:
  AISemanticEmbedderServiceLauncher();
  ~AISemanticEmbedderServiceLauncher() override;

 private:
  friend base::NoDestructor<AISemanticEmbedderServiceLauncher>;

  class CrashTracker {
   public:
    bool IsCrashLimitReached() const;
    void ResetCrashCount();
    void RecordCrash();

   private:
    static constexpr int kMaxCrashes = 3;
    int consecutive_crashes_ = 0;
  };

  CrashTracker crash_tracker_;
  passage_embeddings::PassageEmbeddingsServiceController controller_{
      *this,
      /*execute_for_gemma=*/true};
};

#endif  // CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_
