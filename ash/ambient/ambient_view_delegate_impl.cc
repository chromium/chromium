// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_view_delegate_impl.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "base/check.h"
#include "base/functional/bind.h"

namespace ash {

AmbientViewDelegateImpl::AmbientViewDelegateImpl(
    AmbientController* ambient_controller)
    : ambient_controller_(ambient_controller) {}

AmbientViewDelegateImpl::~AmbientViewDelegateImpl() = default;

void AmbientViewDelegateImpl::AddObserver(
    AmbientViewDelegateObserver* observer) {
  view_delegate_observers_.AddObserver(observer);
}

void AmbientViewDelegateImpl::RemoveObserver(
    AmbientViewDelegateObserver* observer) {
  view_delegate_observers_.RemoveObserver(observer);
}

AmbientBackendModel* AmbientViewDelegateImpl::GetAmbientBackendModel() {
  return ambient_controller_->GetAmbientBackendModel();
}

AmbientWeatherModel* AmbientViewDelegateImpl::GetAmbientWeatherModel() {
  return ambient_controller_->GetAmbientWeatherModel();
}

void AmbientViewDelegateImpl::NotifyObserversMarkerHit(
    AmbientPhotoConfig::Marker marker) {
  for (AmbientViewDelegateObserver& observer : view_delegate_observers_) {
    observer.OnMarkerHit(marker);
  }
}

}  // namespace ash
