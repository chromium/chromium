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

namespace {

bool IsDownloadEvent(const component_updater::CrxUpdateItem& item) {
  switch (item.state) {
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpToDate:
    case update_client::ComponentState::kDownloadingDiff:
    case update_client::ComponentState::kUpdatingDiff:
      return item.total_bytes >= 0;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
    case update_client::ComponentState::kLastStatus:
      return false;
  }
}

}  // namespace

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

  // If there are no component ids to observe, just send zero and one hundred
  // percent.
  if (component_ids_.empty()) {
    observer_remote_->OnDownloadProgressUpdate(
        0, AIUtils::kNormalizedDownloadProgressMax);
    observer_remote_->OnDownloadProgressUpdate(
        AIUtils::kNormalizedDownloadProgressMax,
        AIUtils::kNormalizedDownloadProgressMax);
    return;
  }

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
  if (!IsDownloadEvent(item) || !component_ids_.contains(item.id)) {
    return;
  }

  int64_t bytes_so_far = item.downloaded_bytes;
  int64_t total_bytes = item.total_bytes;

  CHECK_GE(bytes_so_far, 0);
  CHECK_LE(bytes_so_far, total_bytes);

  if (!has_previous_progress_event_) {
    has_previous_progress_event_ = true;

    // Just send zero for the first event unless `bytes_so_far` is equal to
    // `total_bytes`.
    observer_remote_->OnDownloadProgressUpdate(
        0, AIUtils::kNormalizedDownloadProgressMax);
    if (bytes_so_far != total_bytes) {
      return;
    }
  }

  int normalized_progress =
      AIUtils::NormalizeModelDownloadProgress(bytes_so_far, total_bytes);

  // Send the progress event to the observer.
  observer_remote_->OnDownloadProgressUpdate(
      normalized_progress, AIUtils::kNormalizedDownloadProgressMax);
}

}  // namespace on_device_ai
