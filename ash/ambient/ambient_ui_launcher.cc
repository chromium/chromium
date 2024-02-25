// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/metrics/ambient_consumer_session_metrics_delegate.h"

namespace ash {

void AmbientUiLauncher::SetReadyState(bool is_ready) {
  if (is_ready_ == is_ready) {
    return;
  }
  is_ready_ = is_ready;
  // Observer might not be set if this method is called from the constructor.
  if (observer_) {
    observer_->OnReadyStateChanged(is_ready_);
  }
}

bool AmbientUiLauncher::IsReady() {
  return is_ready_;
}

void AmbientUiLauncher::SetObserver(Observer* observer) {
  CHECK(!observer_);
  observer_ = observer;
}

std::unique_ptr<AmbientSessionMetricsRecorder::Delegate>
AmbientUiLauncher::CreateMetricsDelegate(
    AmbientUiSettings current_ui_settings) {
  return std::make_unique<AmbientConsumerSessionMetricsDelegate>(
      std::move(current_ui_settings));
}

}  // namespace ash
