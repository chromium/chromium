// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/deep_scanning_metadata.h"

namespace safe_browsing {

DeepScanningMetadata::DownloadScopedObservation::DownloadScopedObservation(
    DeepScanningMetadata* metadata,
    download::DownloadItem::Observer* observer)
    : metadata_(metadata) {
  observation_ = std::make_unique<base::ScopedObservation<
      download::DownloadItem, download::DownloadItem::Observer>>(observer);
}

DeepScanningMetadata::DownloadScopedObservation::~DownloadScopedObservation() {
  // Remove observation from the list of observers.
  if (observation_ && metadata_) {
    metadata_->RemoveObservation(observation_->GetObserver());
  }
}

void DeepScanningMetadata::DownloadScopedObservation::Stop() {
  // Stop observing the `DownloadItem`.
  if (observation_ && observation_->IsObserving()) {
    observation_->Reset();
  }
}

void DeepScanningMetadata::DownloadScopedObservation::Observe(
    download::DownloadItem* source) {
  if (observation_) {
    observation_->Observe(source);
  }
}

}  // namespace safe_browsing
