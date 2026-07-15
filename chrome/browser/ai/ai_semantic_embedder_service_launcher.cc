// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "content/public/browser/service_process_host.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

// static
AISemanticEmbedderServiceLauncher*
    AISemanticEmbedderServiceLauncher::testing_instance_ = nullptr;

// static
AISemanticEmbedderServiceLauncher* AISemanticEmbedderServiceLauncher::Get() {
  if (testing_instance_) {
    return testing_instance_;
  }
  static base::NoDestructor<AISemanticEmbedderServiceLauncher> instance;
  return instance.get();
}

// static
void AISemanticEmbedderServiceLauncher::SetForTesting(  // IN-TEST
    AISemanticEmbedderServiceLauncher* testing_instance) {
  testing_instance_ = testing_instance;
}

void AISemanticEmbedderServiceLauncher::RecordSuccessfulUse() {
  crash_tracker_.ResetCrashCount();
}

void AISemanticEmbedderServiceLauncher::AddDownloadObserver(
    mojo::PendingRemote<on_device_model::mojom::DownloadObserver> monitor) {
  if (!embeddings_download_progress_manager_ && g_browser_process &&
      g_browser_process->component_updater()) {
    embeddings_download_progress_manager_ = std::make_unique<
        optimization_guide::OnDeviceModelDownloadProgressManager>(
        g_browser_process->component_updater(),
        base::flat_set<std::string>{
            component_updater::GetAIEmbeddingsComponentId()},
        /*enable_unloadable_progress=*/true);
  }
  if (embeddings_download_progress_manager_) {
    embeddings_download_progress_manager_->AddObserver(std::move(monitor));
  }
}

void AISemanticEmbedderServiceLauncher::LaunchService(
    mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbeddingsService>
        receiver) {
  content::ServiceProcessHost::Launch<
      passage_embeddings::mojom::PassageEmbeddingsService>(
      std::move(receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("AI Embeddings Service")  // Unique name for JS API!
          .Pass());
}

void AISemanticEmbedderServiceLauncher::OnServiceDisconnected(bool is_idle) {
  if (!is_idle) {
    crash_tracker_.RecordCrash();
  }
}

bool AISemanticEmbedderServiceLauncher::AllowedToLaunch() const {
  return !crash_tracker_.IsCrashLimitReached();
}

AISemanticEmbedderServiceLauncher::AISemanticEmbedderServiceLauncher() {
  controller_.AddObserver(this);
}

AISemanticEmbedderServiceLauncher::~AISemanticEmbedderServiceLauncher() {
  controller_.RemoveObserver(this);
}

void AISemanticEmbedderServiceLauncher::WaitForModelAvailable(
    base::OnceClosure callback) {
  if (controller_.IsModelAvailable()) {
    std::move(callback).Run();
    embeddings_download_progress_manager_.reset();
  } else {
    bool was_empty = pending_model_availability_callbacks_.empty();
    pending_model_availability_callbacks_.push_back(std::move(callback));

    if (was_empty) {
      component_updater::UpdateAIEmbeddingsComponentOnDemand(
          component_updater::OnDemandUpdater::Priority::FOREGROUND,
          base::BindOnce(
              &AISemanticEmbedderServiceLauncher::OnComponentUpdateFinished,
              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void AISemanticEmbedderServiceLauncher::FlushCallbacks() {
  auto callbacks = std::exchange(pending_model_availability_callbacks_, {});
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
  embeddings_download_progress_manager_.reset();
}

void AISemanticEmbedderServiceLauncher::OnComponentUpdateFinished() {
  if (!controller_.IsModelAvailable()) {
    FlushCallbacks();
  }
}

void AISemanticEmbedderServiceLauncher::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata /*metadata*/) {
  FlushCallbacks();
}

bool AISemanticEmbedderServiceLauncher::CrashTracker::IsCrashLimitReached()
    const {
  return consecutive_crashes_ >= kMaxCrashes;
}

void AISemanticEmbedderServiceLauncher::CrashTracker::ResetCrashCount() {
  consecutive_crashes_ = 0;
}

void AISemanticEmbedderServiceLauncher::CrashTracker::RecordCrash() {
  consecutive_crashes_++;
}
