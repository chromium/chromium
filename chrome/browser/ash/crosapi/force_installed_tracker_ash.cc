// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/force_installed_tracker_ash.h"

namespace crosapi {

ForceInstalledTrackerAsh::ForceInstalledTrackerAsh() = default;

ForceInstalledTrackerAsh::~ForceInstalledTrackerAsh() = default;

void ForceInstalledTrackerAsh::OnForceInstalledExtensionsReady() {
  is_ready_ = true;
  for (auto& obs : observers_) {
    obs.OnForceInstalledExtensionsReady();
  }
}

void ForceInstalledTrackerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ForceInstalledTracker> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ForceInstalledTrackerAsh::AddObserver(
    extensions::ForceInstalledTracker::Observer* observer) {
  observers_.AddObserver(observer);
}

void ForceInstalledTrackerAsh::RemoveObserver(
    extensions::ForceInstalledTracker::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ForceInstalledTrackerAsh::IsReady() const {
  return is_ready_;
}

}  // namespace crosapi
