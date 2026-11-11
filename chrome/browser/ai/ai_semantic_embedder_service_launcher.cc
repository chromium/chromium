// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"
#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/service_process_host.h"
#include "services/on_device_model/public/mojom/download_observer.mojom.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace {

// The process-wide launcher, once Get() has created it. Never destroyed.
AISemanticEmbedderServiceLauncher* g_instance = nullptr;

}  // namespace

// static
AISemanticEmbedderServiceLauncher*
    AISemanticEmbedderServiceLauncher::testing_instance_ = nullptr;

// static
AISemanticEmbedderServiceLauncher* AISemanticEmbedderServiceLauncher::Get() {
  if (testing_instance_) {
    return testing_instance_;
  }
  if (!g_instance) {
    static base::NoDestructor<AISemanticEmbedderServiceLauncher> instance;
    g_instance = instance.get();
    return g_instance;
  }
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (local_state &&
      local_state != g_instance->pref_change_registrar_.prefs()) {
    // The launcher is never destroyed, so keep it bound to the current local
    // state. In the browser that only differs if the launcher was created
    // before local state existed; unit tests also replace the browser process
    // and its local state.
    g_instance->ObserveModelAccessPrefs(*local_state);
  }
  return g_instance;
}

// static
AISemanticEmbedderServiceLauncher*
AISemanticEmbedderServiceLauncher::GetIfExists() {
  return testing_instance_ ? testing_instance_ : g_instance;
}

// static
void AISemanticEmbedderServiceLauncher::SetForTesting(  // IN-TEST
    AISemanticEmbedderServiceLauncher* testing_instance) {
  testing_instance_ = testing_instance;
}

void AISemanticEmbedderServiceLauncher::RecordSuccessfulUse() {
  crash_tracker_.ResetCrashCount();
}

// TODO(crbug.com/537464692): Evaluate Origin Trial architecture for model
// lifecycle management. Currently separate from Manifest Broker, but we need to
// resolve duplication for space-checking, GC, and download tracking.
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
  if (g_browser_process && g_browser_process->local_state()) {
    ObserveModelAccessPrefs(*g_browser_process->local_state());
  }
}

AISemanticEmbedderServiceLauncher::~AISemanticEmbedderServiceLauncher() {
  controller_.RemoveObserver(this);
}

void AISemanticEmbedderServiceLauncher::ObserveModelAccessPrefs(
    PrefService& local_state) {
  pref_change_registrar_.Reset();
  pref_change_registrar_.Init(&local_state);
  // Unretained is safe: the registrar is a member and is destroyed first.
  auto on_changed = base::BindRepeating(
      &AISemanticEmbedderServiceLauncher::OnModelAccessPrefsChanged,
      base::Unretained(this));
  pref_change_registrar_.Add(optimization_guide::model_execution::prefs::
                                 localstate::kOnDeviceAiUserSettingsEnabled,
                             on_changed);
  pref_change_registrar_.Add(
      optimization_guide::model_execution::prefs::localstate::
          kGenAILocalFoundationalModelEnterprisePolicySettings,
      on_changed);
}

void AISemanticEmbedderServiceLauncher::OnModelAccessPrefsChanged() {
  PrefService* local_state = pref_change_registrar_.prefs();
  if (component_updater::IsAIEmbeddingsModelAllowed(local_state)) {
    // Re-enabled. The component is re-registered lazily by the next
    // WaitForModelAvailable() call.
    return;
  }

  // Unregister (or uninstall) the component and clear the eligibility pref.
  // TODO(crbug.com/579902011): On Windows, deleting the files fails while the
  // embeddings process still has them mapped, and the next startup with the
  // setting off is what removes them. Retry once the process disconnects.
  component_updater::PurgeAIEmbeddingsComponent(
      g_browser_process ? g_browser_process->component_updater() : nullptr,
      *local_state);

  // Drop the loaded model. This disconnects the embedder remote, so in-flight
  // requests fail, and makes IsModelAvailable() false, so queued jobs and
  // later Embed() calls fail instead of launching the service.
  controller_.MaybeUpdateModelInfo(std::nullopt);

  // Pending availability callbacks observe the unavailable model and fail.
  FlushCallbacks();
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
      if (g_browser_process && g_browser_process->component_updater() &&
          g_browser_process->local_state() &&
          component_updater::IsAIEmbeddingsModelAllowed(
              g_browser_process->local_state())) {
        g_browser_process->local_state()->SetBoolean(
            optimization_guide::model_execution::prefs::localstate::
                kEmbeddingApiModelDownloadEligible,
            true);
        component_updater::RegisterAIEmbeddingsComponent(
            g_browser_process->component_updater(),
            g_browser_process->local_state(),
            base::BindOnce(&AISemanticEmbedderServiceLauncher::
                               OnComponentRegistrationCompleted,
                           weak_ptr_factory_.GetWeakPtr()));
      } else {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(&AISemanticEmbedderServiceLauncher::FlushCallbacks,
                           weak_ptr_factory_.GetWeakPtr()));
      }
    }
  }
}

void AISemanticEmbedderServiceLauncher::OnComponentUninstalled() {
  if (!controller_.IsModelAvailable()) {
    return;
  }
  // For example, an update finished after the setting was turned off and back
  // on, so ComponentReady() loaded it, and then the unregister the purge had
  // deferred until the end of the update deleted its files.
  controller_.MaybeUpdateModelInfo(std::nullopt);
  FlushCallbacks();
}

void AISemanticEmbedderServiceLauncher::FlushCallbacks() {
  auto callbacks = std::exchange(pending_model_availability_callbacks_, {});
  embeddings_download_progress_manager_.reset();
  component_updater_observation_.Reset();
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void AISemanticEmbedderServiceLauncher::OnComponentRegistrationCompleted(
    bool registered) {
  if (!registered || !g_browser_process ||
      !g_browser_process->component_updater()) {
    FlushCallbacks();
    return;
  }
  // The setting or policy may have changed while Register() was running. The
  // component wasn't registered yet then, so unregister it now and don't
  // download.
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state ||
      !component_updater::IsAIEmbeddingsModelAllowed(local_state)) {
    if (local_state) {
      component_updater::PurgeAIEmbeddingsComponent(
          g_browser_process->component_updater(), *local_state);
    }
    FlushCallbacks();
    return;
  }
  // The purge may also have run and the setting been turned back on while
  // Register() was running. It failed the waiting create() calls and cleared
  // the eligibility pref, which only WaitForModelAvailable() sets again, so
  // if the pref is clear nothing has asked for the model since: unregister
  // the component and don't download.
  if (!local_state->GetBoolean(
          optimization_guide::model_execution::prefs::localstate::
              kEmbeddingApiModelDownloadEligible)) {
    g_browser_process->component_updater()->UnregisterComponent(
        component_updater::GetAIEmbeddingsComponentId());
    FlushCallbacks();
    return;
  }
  if (!component_updater_observation_.IsObserving() && g_browser_process &&
      g_browser_process->component_updater()) {
    component_updater_observation_.Observe(
        g_browser_process->component_updater());
  }
  component_updater::UpdateAIEmbeddingsComponentOnDemand(
      component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce(
          &AISemanticEmbedderServiceLauncher::OnComponentUpdateFinished,
          weak_ptr_factory_.GetWeakPtr()));
}

void AISemanticEmbedderServiceLauncher::OnComponentUpdateFinished(
    update_client::Error error) {
  if (controller_.IsModelAvailable()) {
    return;
  }
  if (error != update_client::Error::NONE &&
      error != update_client::Error::UPDATE_IN_PROGRESS) {
    FlushCallbacks();
  }
  // If NONE or UPDATE_IN_PROGRESS, we wait for OnEvent (kUpdated, kUpToDate,
  // or kUpdateError) to trigger the fallback/flush, or EmbedderMetadataUpdated
  // to succeed.
}

void AISemanticEmbedderServiceLauncher::OnEvent(
    const component_updater::CrxUpdateItem& item) {
  if (item.id != component_updater::GetAIEmbeddingsComponentId()) {
    return;
  }

  if (item.state == update_client::ComponentState::kUpdateError ||
      item.state == update_client::ComponentState::kUpdated ||
      item.state == update_client::ComponentState::kUpToDate) {
    if (!controller_.IsModelAvailable()) {
      FlushCallbacks();
    }
  }
}

void AISemanticEmbedderServiceLauncher::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata /*metadata*/) {
  crash_tracker_.ResetCrashCount();
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
