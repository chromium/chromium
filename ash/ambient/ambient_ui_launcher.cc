// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_ui_launcher.h"

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

}  // namespace ash
