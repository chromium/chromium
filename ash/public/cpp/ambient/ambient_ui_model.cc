// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_ui_model.h"

namespace ash {

namespace {

AmbientUiModel* g_ambient_ui_model = nullptr;

}  // namespace

// static
// TODO(b/158039112): return a const* instead.
AmbientUiModel* AmbientUiModel::Get() {
  return g_ambient_ui_model;
}

AmbientUiModel::AmbientUiModel() {
  DCHECK(!g_ambient_ui_model);
  g_ambient_ui_model = this;
}

AmbientUiModel::~AmbientUiModel() {
  DCHECK_EQ(g_ambient_ui_model, this);
  g_ambient_ui_model = nullptr;
}

void AmbientUiModel::AddObserver(AmbientUiModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AmbientUiModel::RemoveObserver(AmbientUiModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AmbientUiModel::SetUiVisibility(AmbientUiVisibility visibility) {
  if (ui_visibility_ == visibility)
    return;

  ui_visibility_ = visibility;
  NotifyAmbientUiVisibilityChanged();
}

void AmbientUiModel::SetUiMode(AmbientUiMode ui_mode) {
  if (ui_mode_ == ui_mode)
    return;

  ui_mode_ = ui_mode;
}

void AmbientUiModel::NotifyAmbientUiVisibilityChanged() {
  for (auto& observer : observers_)
    observer.OnAmbientUiVisibilityChanged(ui_visibility_);
}

std::ostream& operator<<(std::ostream& out, AmbientUiMode mode) {
  switch (mode) {
    case AmbientUiMode::kLockScreenUi:
      out << "kLockScreenUi";
      break;
    case AmbientUiMode::kInSessionUi:
      out << "kInSessionUi";
      break;
  }
  return out;
}

}  // namespace ash
