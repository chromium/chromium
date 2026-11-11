// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/core/passage_embeddings_service_launcher.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "components/prefs/pref_change_registrar.h"
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

class PrefService;

class AISemanticEmbedderServiceLauncher
    : public passage_embeddings::PassageEmbeddingsServiceLauncher,
      public passage_embeddings::EmbedderMetadataObserver,
      public component_updater::ComponentUpdateService::Observer {
 public:
  static AISemanticEmbedderServiceLauncher* Get();
  // Like Get(), but returns null instead of creating the launcher.
  static AISemanticEmbedderServiceLauncher* GetIfExists();
  static void SetForTesting(
      AISemanticEmbedderServiceLauncher* testing_instance);

  passage_embeddings::PassageEmbeddingsServiceController* controller() {
    return &controller_;
  }

  // Returns the input window of the loaded model, in tokens. Call only while
  // `controller()->IsModelAvailable()` is true.
  uint32_t input_window_size();

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

  // Called after the AI embeddings component was uninstalled, which deletes
  // its files. Drops the loaded model, if any, so it can't be used with
  // deleted files; the next create() registers the component again. Does
  // nothing when no model is loaded, so a create() waiting for a new download
  // keeps waiting.
  void OnComponentUninstalled();

 protected:
  AISemanticEmbedderServiceLauncher();
  ~AISemanticEmbedderServiceLauncher() override;

 private:
  friend base::NoDestructor<AISemanticEmbedderServiceLauncher>;

  // passage_embeddings::EmbedderMetadataObserver implementation:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

  // Observes the prefs that gate model access in `local_state`, replacing any
  // earlier observation.
  void ObserveModelAccessPrefs(PrefService& local_state);

  // Called when the "On-device AI" user setting or the
  // GenAILocalFoundationalModelSettings enterprise policy changes. When the
  // model is no longer allowed, the component is uninstalled, the loaded model
  // is dropped (in-flight requests fail) and queued availability callbacks run.
  // Re-enabling does nothing until the next create() re-registers the
  // component.
  void OnModelAccessPrefsChanged();

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

  // The input window of the last model `controller_` accepted, in tokens.
  // Dropping the model doesn't clear it, so `input_window_size()` checks that a
  // model is loaded.
  uint32_t input_window_size_ = 0;

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

  // Watches the prefs that gate model access in the browser's local state.
  // Get() rebinds it when local state changes.
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<AISemanticEmbedderServiceLauncher> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_AI_AI_SEMANTIC_EMBEDDER_SERVICE_LAUNCHER_H_
