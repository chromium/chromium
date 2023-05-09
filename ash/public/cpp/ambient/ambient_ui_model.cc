// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_ui_model.h"

#include "base/check_op.h"

namespace ash {

namespace {

AmbientUiModel* g_ambient_ui_model = nullptr;

constexpr AmbientJitterConfig kSlideshowPeripheralUiJitterConfig{
    .step_size = 5,
    .x_min_translation = 0,
    .x_max_translation = 20,
    .y_min_translation = -20,
    .y_max_translation = 0};

constexpr AmbientJitterConfig kAnimationJitterConfig{.step_size = 2,
                                                     .x_min_translation = -10,
                                                     .x_max_translation = 10,
                                                     .y_min_translation = -10,
                                                     .y_max_translation = 10};

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

void AmbientUiModel::SetLockScreenInactivityTimeout(base::TimeDelta timeout) {
  if (timeout == lock_screen_inactivity_timeout_)
    return;

  lock_screen_inactivity_timeout_ = timeout;
  NotifyLockScreenInactivityTimeoutChanged();
}

void AmbientUiModel::SetBackgroundLockScreenTimeout(base::TimeDelta timeout) {
  if (timeout == background_lock_screen_timeout_)
    return;

  background_lock_screen_timeout_ = timeout;
  NotifyBackgroundLockScreenTimeoutChanged();
}

void AmbientUiModel::SetPhotoRefreshInterval(base::TimeDelta interval) {
  if (interval == photo_refresh_interval_)
    return;

  photo_refresh_interval_ = interval;
}

AmbientJitterConfig AmbientUiModel::GetSlideshowPeripheralUiJitterConfig() {
  return jitter_config_for_testing_.value_or(
      kSlideshowPeripheralUiJitterConfig);
}

AmbientJitterConfig AmbientUiModel::GetAnimationJitterConfig() {
  return jitter_config_for_testing_.value_or(kAnimationJitterConfig);
}

void AmbientUiModel::NotifyAmbientUiVisibilityChanged() {
  for (auto& observer : observers_)
    observer.OnAmbientUiVisibilityChanged(ui_visibility_);
}

void AmbientUiModel::NotifyLockScreenInactivityTimeoutChanged() {
  for (auto& observer : observers_) {
    observer.OnLockScreenInactivityTimeoutChanged(
        lock_screen_inactivity_timeout_);
  }
}

void AmbientUiModel::NotifyBackgroundLockScreenTimeoutChanged() {
  for (auto& observer : observers_) {
    observer.OnBackgroundLockScreenTimeoutChanged(
        background_lock_screen_timeout_);
  }
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

std::ostream& operator<<(std::ostream& out, AmbientUiVisibility visibility) {
  switch (visibility) {
    case AmbientUiVisibility::kShouldShow:
      out << "kShown";
      break;
    case AmbientUiVisibility::kPreview:
      out << "kPreview";
      break;
    case AmbientUiVisibility::kHidden:
      out << "kHidden";
      break;
    case AmbientUiVisibility::kClosed:
      out << "kClosed";
      break;
  }
  return out;
}

}  // namespace ash
