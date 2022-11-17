// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/app_stream_launcher_data_model.h"

namespace ash::phonehub {

AppStreamLauncherDataModel::AppStreamLauncherDataModel() = default;

AppStreamLauncherDataModel::~AppStreamLauncherDataModel() = default;

void AppStreamLauncherDataModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AppStreamLauncherDataModel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppStreamLauncherDataModel::SetShouldShowMiniLauncher(
    bool should_show_mini_launcher) {
  should_show_app_stream_launcher_ = should_show_mini_launcher;
  for (auto& observer : observer_list_)
    observer.OnShouldShowMiniLauncherChanged();
}

bool AppStreamLauncherDataModel::GetShouldShowMiniLauncher() {
  return should_show_app_stream_launcher_;
}

void AppStreamLauncherDataModel::ResetState() {
  should_show_app_stream_launcher_ = false;
}

}  // namespace ash::phonehub
