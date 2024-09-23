// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/nearby_share/nearby_share_controller_impl.h"

namespace ash {

NearbyShareControllerImpl::NearbyShareControllerImpl() = default;

NearbyShareControllerImpl::~NearbyShareControllerImpl() = default;

void NearbyShareControllerImpl::HighVisibilityEnabledChanged(bool enabled) {
  for (auto& observer : observers_) {
    observer.OnHighVisibilityEnabledChanged(enabled);
  }
}

void NearbyShareControllerImpl::VisibilityChanged(
    ::nearby_share::mojom::Visibility visibility) const {
  for (auto& observer : observers_) {
    observer.OnVisibilityChanged(visibility);
  }
}

void NearbyShareControllerImpl::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void NearbyShareControllerImpl::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

}  // namespace ash
