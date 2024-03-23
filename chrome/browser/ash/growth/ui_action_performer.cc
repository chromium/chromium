// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/ui_action_performer.h"

UiActionPerformer::UiActionPerformer() = default;

UiActionPerformer::~UiActionPerformer() = default;

void UiActionPerformer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void UiActionPerformer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void UiActionPerformer::NotifyReadyToLogImpression() {
  for (auto& observer : observers_) {
    observer.OnReadyToLogImpression();
  }
}

void UiActionPerformer::NotifyUiDismissed() {
  for (auto& observer : observers_) {
    observer.OnUiDismissed();
  }
}

void UiActionPerformer::NotifyPrimaryButtonPressed() {
  for (auto& observer : observers_) {
    observer.OnPrimaryButtonPressed();
  }
}

void UiActionPerformer::NotifySecondaryButtonPressed() {
  for (auto& observer : observers_) {
    observer.OnSecondaryButtonPressed();
  }
}

void UiActionPerformer::NotifyCloseButtonPressed() {
  for (auto& observer : observers_) {
    observer.OnCloseButtonPressed();
  }
}
