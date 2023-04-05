// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/quick_app_access_model.h"

namespace ash {

QuickAppAccessModel::QuickAppAccessModel() = default;

QuickAppAccessModel::~QuickAppAccessModel() = default;

void QuickAppAccessModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void QuickAppAccessModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void QuickAppAccessModel::SetQuickApp(const std::string& app_id) {
  quick_app_id_ = app_id;
  for (auto& observer : observers_) {
    // TODO(b/266734005): Check whether the app has an icon already loaded, if
    // not, then call LoadIcon() to begin the icon loading process for the quick
    // app. Call OnQuickAppChanged once the icon is loaded.
    observer.OnQuickAppChanged();
  }
}

}  // namespace ash
