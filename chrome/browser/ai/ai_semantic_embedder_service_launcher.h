// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/core/passage_embeddings_service_launcher.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "components/update_client/update_client_errors.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/on_device_model/public/mojom/download_observer.mojom-forward.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom-forward.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace optimization_guide {
class OnDeviceModelDownloadProgressManager;
}

class AISemanticEmbedderServiceLauncher
    : public passage_embeddings::PassageEmbeddingsServiceLauncher,
      public passage_embeddings::EmbedderMetadataObserver,
      public component_updater::ComponentUpdateService::Observer {
 public:
  static AISemanticEmbedderServiceLauncher* Get();
  static void SetForTesting(
      AISemanticEmbedderServiceLauncher* testing_instance);

  passage_embeddings::PassageEmbeddingsServiceController* controller() {
    return &controller_;
  }

  void AddDownloadObserver(
      mojo::PendingRemote<on_device_model::mojom::DownloadObserver> monitor);

  void RecordSuccessfulUse();

  // passage_embeddings::PassageEmbeddingsServiceLauncher implementation:
  void LaunchService(
      mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbeddingsService>
          receiver) override;
  void OnServiceDisconnected(bool is_idle) override;
  bool AllowedToLaunch() const override;

  // Queues callback if the model isn't available, runs it immediately
  // otherwise.
  void WaitForModelAvailable(base::OnceClosure callback);

 protected:
  AISemanticEmbedderServiceLauncher();
  ~AISemanticEmbedderServiceLauncher() override;

 private:
  friend base::NoDestructor<AISemanticEmbedderServiceLauncher>;

  // passage_embeddings::EmbedderMetadataObserver implementation:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

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

  void FlushCallbacks();
  void OnComponentRegistrationCompleted(bool registered);
  void OnComponentUpdateFinished(update_client::Error error);

  // component_updater::ComponentUpdateService::Observer:
  void OnEvent(const component_updater::CrxUpdateItem& item) override;

  std::vector<base::OnceClosure> pending_model_availability_callbacks_;

  std::unique_ptr<optimization_guide::OnDeviceModelDownloadProgressManager>
      embeddings_download_progress_manager_;

  static AISemanticEmbedderServiceLauncher* testing_instance_;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      component_updater_observation_{this};

  base::WeakPtrFactory<AISemanticEmbedderServiceLauncher> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_
