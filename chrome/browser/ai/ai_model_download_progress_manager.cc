// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_model_download_progress_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/ai/ai_utils.h"

namespace on_device_ai {

AIModelDownloadProgressManager::AIModelDownloadProgressManager() = default;
AIModelDownloadProgressManager::~AIModelDownloadProgressManager() = default;

void AIModelDownloadProgressManager::AddObserver(
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
        observer_remote,
    base::flat_set<std::unique_ptr<Component>> components) {
  reporters_.emplace(std::make_unique<Reporter>(
      *this, std::move(observer_remote), std::move(components)));
}

void AIModelDownloadProgressManager::RemoveReporter(Reporter* reporter) {
  CHECK(reporter);
  reporters_.erase(reporter);
}

int AIModelDownloadProgressManager::GetNumberOfReporters() {
  return reporters_.size();
}

AIModelDownloadProgressManager::Component::Component() = default;
AIModelDownloadProgressManager::Component::~Component() = default;

AIModelDownloadProgressManager::Component::Component(Component&&) = default;

void AIModelDownloadProgressManager::Component::SetDownloadedBytes(
    int64_t downloaded_bytes) {
  if (downloaded_bytes < downloaded_bytes_.value_or(0)) {
    return;
  }

  downloaded_bytes_ = downloaded_bytes;
  MaybeRunEventCallback();
}
void AIModelDownloadProgressManager::Component::SetTotalBytes(
    int64_t total_bytes) {
  if (total_bytes == total_bytes_) {
    return;
  }

  // `total_bytes_` should never change after it's been set.
  CHECK(!total_bytes_.has_value());
  CHECK_GE(total_bytes, 0);

  total_bytes_ = total_bytes;
  MaybeRunEventCallback();
}

void AIModelDownloadProgressManager::Component::SetEventCallback(
    EventCallback event_callback) {
  event_callback_ = std::move(event_callback);
  MaybeRunEventCallback();
}

void AIModelDownloadProgressManager::Component::MaybeRunEventCallback() {
  if (!determined_bytes() || !event_callback_) {
    return;
  }
  event_callback_.Run(
      *this, !last_determined_bytes_,
      downloaded_bytes_.value() - last_downloaded_bytes_.value_or(0));
  last_downloaded_bytes_ = downloaded_bytes_;
  last_determined_bytes_ = true;
}

AIModelDownloadProgressManager::Reporter::Reporter(
    AIModelDownloadProgressManager& manager,
    mojo::PendingRemote<blink::mojom::ModelDownloadProgressObserver>
        observer_remote,
    base::flat_set<std::unique_ptr<Component>> components)
    : manager_(manager),
      observer_remote_(std::move(observer_remote)),
      components_(std::move(components)) {
  observer_remote_.set_disconnect_handler(base::BindOnce(
      &Reporter::OnRemoteDisconnect, weak_ptr_factory_.GetWeakPtr()));

  // Don't watch any components that are already installed.
  for (auto iter = components_.begin(); iter != components_.end();) {
    if ((*iter)->is_complete()) {
      iter = components_.erase(iter);
      continue;
    }

    Component& component = *iter->get();

    // Watch for progress updates.
    component.SetEventCallback(base::BindRepeating(
        &Reporter::OnEvent, weak_ptr_factory_.GetWeakPtr()));

    ++iter;
  }

  // If there are no component ids to observe, just send zero and one hundred
  // percent.
  if (components_.empty()) {
    observer_remote_->OnDownloadProgressUpdate(
        0, AIUtils::kNormalizedDownloadProgressMax);
    observer_remote_->OnDownloadProgressUpdate(
        AIUtils::kNormalizedDownloadProgressMax,
        AIUtils::kNormalizedDownloadProgressMax);
  }
}

AIModelDownloadProgressManager::Reporter::~Reporter() = default;

void AIModelDownloadProgressManager::Reporter::OnRemoteDisconnect() {
  // Destroy `this` when the `ModelDownloadProgressObserver` is garbage
  // collected in the renderer.
  manager_->RemoveReporter(this);
}

bool AIModelDownloadProgressManager::Reporter::ReadyToReport() {
  // If we have observed the downloaded bytes of all our components then we're
  // ready to start reporting.
  return determined_components_ == static_cast<int>(components_.size());
}

void AIModelDownloadProgressManager::Reporter::ProcessEvent(
    const Component& component,
    bool just_determined,
    int64_t downloaded_bytes_delta) {
  // Should only receive events for components that have their bytes determined.
  CHECK(component.determined_bytes());

  CHECK_GE(component.downloaded_bytes(), 0);
  CHECK_GE(component.total_bytes(), 0);

  components_downloaded_bytes_ += downloaded_bytes_delta;

  // If we haven't just determined bytes, that means we've already seen this
  // component and we don't need to do anything further.
  if (!just_determined) {
    return;
  }

  // We shouldn't already be ready to report if a component's bytes have just
  // been determined.
  CHECK(!ReadyToReport());

  components_total_bytes_ += component.total_bytes();
  determined_components_++;

  if (!ReadyToReport()) {
    return;
  }

  last_reported_progress_ = 0;
  last_progress_time_ = base::TimeTicks::Now();

  // We don't want to include already downloaded bytes in our progress
  // calculation, so determine it for later calculations and remove it now
  // from components_total_bytes_.
  components_total_bytes_ -= components_downloaded_bytes_;
  components_downloaded_bytes_ = 0;

  CHECK_GE(components_total_bytes_, 0);

  // Must always fire the zero progress event first.
  observer_remote_->OnDownloadProgressUpdate(
      0, AIUtils::kNormalizedDownloadProgressMax);
}

void AIModelDownloadProgressManager::Reporter::OnEvent(
    Component& component,
    bool just_determined,
    int64_t downloaded_bytes_delta) {
  ProcessEvent(component, just_determined, downloaded_bytes_delta);

  // Wait for the total number of bytes to be downloaded to become determined.
  if (!ReadyToReport()) {
    return;
  }

  CHECK_GE(components_downloaded_bytes_, 0);
  CHECK_LE(components_downloaded_bytes_, components_total_bytes_);

  // Only report this event if we're at 100% or if more than 50ms has passed
  // since the last time we reported a progress event.
  if (components_downloaded_bytes_ != components_total_bytes_) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    if (current_time - last_progress_time_ <= base::Milliseconds(50)) {
      return;
    }
    last_progress_time_ = current_time;
  }

  int normalized_progress = AIUtils::NormalizeModelDownloadProgress(
      components_downloaded_bytes_, components_total_bytes_);

  // Don't report progress events we've already sent.
  if (normalized_progress == last_reported_progress_) {
    return;
  }

  CHECK_GT(normalized_progress, last_reported_progress_);
  last_reported_progress_ = normalized_progress;

  // Send the progress event to the observer.
  observer_remote_->OnDownloadProgressUpdate(
      normalized_progress, AIUtils::kNormalizedDownloadProgressMax);
}

}  // namespace on_device_ai
