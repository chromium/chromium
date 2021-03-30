// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/model/projector_ui_model.h"

namespace ash {

ProjectorUiModel::ProjectorUiModel() = default;

ProjectorUiModel::~ProjectorUiModel() = default;

void ProjectorUiModel::AddObserver(ProjectorUiModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ProjectorUiModel::RemoveObserver(ProjectorUiModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ProjectorUiModel::SetBarEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;
  NotifyBarStateChanged(enabled);
}

void ProjectorUiModel::NotifyBarStateChanged(bool enabled) {
  for (ProjectorUiModelObserver& observer : observers_)
    observer.OnProjectorBarStateChanged(enabled);
}

}  // namespace ash
