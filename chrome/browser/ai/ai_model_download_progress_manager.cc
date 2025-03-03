// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_model_download_progress_manager.h"

#include "base/functional/bind.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_utils.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"

namespace on_device_ai {

AIModelDownloadProgressManager::AIModelDownloadProgressManager() = default;
AIModelDownloadProgressManager::~AIModelDownloadProgressManager() = default;

void AIModelDownloadProgressManager::AddObserver(
    component_updater::ComponentUpdateService* component_update_service,
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
        observer_remote,
    base::flat_set<std::string> component_ids) {
  reporters_.emplace(std::make_unique<Reporter>(*this, component_update_service,
                                                std::move(observer_remote),
                                                std::move(component_ids)));
}

void AIModelDownloadProgressManager::RemoveReporter(Reporter* reporter) {
  CHECK(reporter);
  reporters_.erase(reporter);
}

int AIModelDownloadProgressManager::GetNumberOfReportersForTesting() {
  return reporters_.size();
}

AIModelDownloadProgressManager::Reporter::Reporter(
    AIModelDownloadProgressManager& manager,
    component_updater::ComponentUpdateService* component_update_service,
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
        observer_remote,
    base::flat_set<std::string> component_ids)
    : manager_(manager),
      observer_remote_(std::move(observer_remote)),
      component_ids_(std::move(component_ids)) {
  CHECK(component_update_service);

  observer_remote_.set_disconnect_handler(base::BindOnce(
      &AIModelDownloadProgressManager::Reporter::OnRemoteDisconnect,
      weak_ptr_factory_.GetWeakPtr()));

  // Watch for progress updates.
  component_updater_observation_.Observe(component_update_service);
}

AIModelDownloadProgressManager::Reporter::~Reporter() = default;

void AIModelDownloadProgressManager::Reporter::OnRemoteDisconnect() {
  // Destroy `this` when the `ModelDownloadProgressObserver` is garbage
  // collected in the renderer.
  manager_->RemoveReporter(this);
}

void AIModelDownloadProgressManager::Reporter::OnEvent(
    const component_updater::CrxUpdateItem& item) {
  // TODO(crbug.com/391715395): Report actual download progress.
  observer_remote_->OnDownloadProgressUpdate(
      0, AIUtils::kNormalizedDownloadProgressMax);
}

}  // namespace on_device_ai
