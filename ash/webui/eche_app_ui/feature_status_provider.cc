// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/feature_status_provider.h"

namespace ash {
namespace eche_app {

FeatureStatusProvider::FeatureStatusProvider() = default;

FeatureStatusProvider::~FeatureStatusProvider() = default;

void FeatureStatusProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FeatureStatusProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FeatureStatusProvider::NotifyStatusChanged() {
  for (auto& observer : observer_list_)
    observer.OnFeatureStatusChanged();
}

}  // namespace eche_app
}  // namespace ash
