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
      return item.downloaded_bytes >= 0 && item.total_bytes >= 0;
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

int AIModelDownloadProgressManager::GetNumberOfReporters() {
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

  // Don't watch any components that are already installed.
  for (const auto& component_id : component_update_service->GetComponentIDs()) {
    component_ids_.erase(component_id);
  }

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

int64_t AIModelDownloadProgressManager::Reporter::GetDownloadedBytes() {
  int64_t bytes_so_far = 0;
  for (const auto& [id, downloaded_bytes] : observed_downloaded_bytes_) {
    bytes_so_far += downloaded_bytes;
  }
  return bytes_so_far;
}

void AIModelDownloadProgressManager::Reporter::ProcessEvent(
    const component_updater::CrxUpdateItem& item) {
  CHECK_GE(item.downloaded_bytes, 0);
  CHECK_GE(item.total_bytes, 0);

  auto iter = observed_downloaded_bytes_.find(item.id);

  // If we've seen this component before, then just update the downloaded bytes
  // for it.
  if (iter != observed_downloaded_bytes_.end()) {
    iter->second = item.downloaded_bytes;
    return;
  }

  // We shouldn't already be ready to report if a component is not in the
  // `component_downloaded_bytes_` map.
  CHECK(!ready_to_report_);

  auto result =
      observed_downloaded_bytes_.insert({item.id, item.downloaded_bytes});
  CHECK(result.second);

  components_total_bytes_ += item.total_bytes;

  // If we have observed the downloaded bytes of all our components then we're
  // ready to start reporting.
  ready_to_report_ = observed_downloaded_bytes_.size() == component_ids_.size();

  if (!ready_to_report_) {
    return;
  }

  last_reported_progress_ = 0;
  last_progress_time_ = base::TimeTicks::Now();

  // We don't want to include already downloaded bytes in our progress
  // calculation, so determine it for later calculations and remove it now
  // from components_total_bytes_.
  already_downloaded_bytes_ = GetDownloadedBytes();
  components_total_bytes_ -= already_downloaded_bytes_;

  // Must always fire the zero progress event first.
  observer_remote_->OnDownloadProgressUpdate(
      0, AIUtils::kNormalizedDownloadProgressMax);
}

void AIModelDownloadProgressManager::Reporter::OnEvent(
    const component_updater::CrxUpdateItem& item) {
  if (!IsDownloadEvent(item) || !component_ids_.contains(item.id)) {
    return;
  }

  ProcessEvent(item);

  // Wait for the total number of bytes to be downloaded to become determined.
  if (!ready_to_report_) {
    return;
  }

  // Calculate the total number of bytes downloaded so far. Don't include bytes
  // that were already downloaded before we determined the total bytes.
  int64_t bytes_so_far = GetDownloadedBytes() - already_downloaded_bytes_;

  CHECK_GE(bytes_so_far, 0);
  CHECK_LE(bytes_so_far, components_total_bytes_);

  // Only report this event if we're at 100% or if more than 50ms has passed
  // since the last time we reported a progress event.
  if (bytes_so_far != components_total_bytes_) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    if (current_time - last_progress_time_ <= base::Milliseconds(50)) {
      return;
    }
    last_progress_time_ = current_time;
  }

  // Determine the normalized progress.
  //
  // If `components_total_bytes_` is zero, we should have downloaded zero bytes
  // out of zero meaning we're at 100%. So set it to
  // `kNormalizedDownloadProgressMax` to avoid dividing by zero in
  // `NormalizeModelDownloadProgress`.
  int normalized_progress = components_total_bytes_ == 0
                                ? AIUtils::kNormalizedDownloadProgressMax
                                : AIUtils::NormalizeModelDownloadProgress(
                                      bytes_so_far, components_total_bytes_);

  // Don't report progress events we've already sent.
  if (normalized_progress <= last_reported_progress_) {
    CHECK(normalized_progress == last_reported_progress_);
    return;
  }
  last_reported_progress_ = normalized_progress;

  // Send the progress event to the observer.
  observer_remote_->OnDownloadProgressUpdate(
      normalized_progress, AIUtils::kNormalizedDownloadProgressMax);
}

}  // namespace on_device_ai
