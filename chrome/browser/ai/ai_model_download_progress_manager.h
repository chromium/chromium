// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_
#define CHROME_BROWSER_AI_AI_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_

#include <memory>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/component_updater/component_updater_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom.h"

namespace on_device_ai {

// Manages a set of `ModelDownloadProgressObserver`s and sends them download
// progress updates for their respective components.
class AIModelDownloadProgressManager {
 public:
  AIModelDownloadProgressManager();
  ~AIModelDownloadProgressManager();

  // Not copyable or movable.
  AIModelDownloadProgressManager(const AIModelDownloadProgressManager&) =
      delete;
  AIModelDownloadProgressManager& operator=(
      const AIModelDownloadProgressManager&) = delete;

  // Adds a `ModelDownloadProgressObserver` to send progress updates for
  // `component_ids`.
  void AddObserver(
      component_updater::ComponentUpdateService* component_update_service,
      mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
          observer_remote,
      base::flat_set<std::string> component_ids);

  int GetNumberOfReportersForTesting();

 private:
  // Observes progress updates from the `component_update_service`, filters and
  // processes them, and reports the result to `observer_remote`.
  class Reporter : public component_updater::ServiceObserver {
   public:
    Reporter(
        AIModelDownloadProgressManager& manager,
        component_updater::ComponentUpdateService* component_update_service,
        mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
            observer_remote,
        base::flat_set<std::string> component_ids);
    ~Reporter() override;

    // Not copyable or movable.
    Reporter(const Reporter&) = delete;
    Reporter& operator=(const Reporter&) = delete;

    // component_updater::ServiceObserver:
    void OnEvent(const component_updater::CrxUpdateItem& item) override;

   private:
    void OnRemoteDisconnect();

    // `manager_` owns `this`.
    base::raw_ref<AIModelDownloadProgressManager> manager_;

    mojo::Remote<blink::mojom::ModelDownloadProgressObserver> observer_remote_;

    base::ScopedObservation<component_updater::ComponentUpdateService,
                            component_updater::ComponentUpdateService::Observer>
        component_updater_observation_{this};

    // The ids of the components we're reporting the progress for.
    base::flat_set<std::string> component_ids_;

    base::WeakPtrFactory<Reporter> weak_ptr_factory_{this};
  };

  void RemoveReporter(Reporter* reporter);

  base::flat_set<std::unique_ptr<Reporter>, base::UniquePtrComparator>
      reporters_;
};
}  // namespace on_device_ai

#endif  // CHROME_BROWSER_AI_AI_MODEL_DOWNLOAD_PROGRESS_MANAGER_H_
