// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/accessibility_features.h"

#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/webui/common/mojom/accessibility_features.mojom.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

namespace {

bool ShouldForceHiddenElementsVisible() {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (!accessibility_controller) {
    return false;
  }

  return accessibility_controller->spoken_feedback().enabled() ||
         accessibility_controller->switch_access().enabled() ||
         accessibility_controller->fullscreen_magnifier().enabled();
}

}  // namespace

AccessibilityFeatures::AccessibilityFeatures() {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (!accessibility_controller) {
    return;
  }

  // Set the initial value from the controller.
  force_hidden_elements_visible_ = ShouldForceHiddenElementsVisible();

  accessibility_controller->AddObserver(this);
}

AccessibilityFeatures::~AccessibilityFeatures() {
  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  if (!accessibility_controller) {
    return;
  }

  accessibility_controller->RemoveObserver(this);
}

void AccessibilityFeatures::ObserveForceHiddenElementsVisible(
    mojo::PendingRemote<common::mojom::ForceHiddenElementsVisibleObserver>
        observer,
    ObserveForceHiddenElementsVisibleCallback callback) {
  force_hidden_elements_visible_observers_.Add(std::move(observer));

  std::move(callback).Run(force_hidden_elements_visible_);
}

void AccessibilityFeatures::OnAccessibilityStatusChanged() {
  // Get the latest state and update the observer if there's a change.
  if (force_hidden_elements_visible_ != ShouldForceHiddenElementsVisible()) {
    force_hidden_elements_visible_ = !force_hidden_elements_visible_;

    for (auto& observer : force_hidden_elements_visible_observers_) {
      observer->OnForceHiddenElementsVisibleChange(
          force_hidden_elements_visible_);
    }
  }
}

void AccessibilityFeatures::BindInterface(
    mojo::PendingReceiver<common::mojom::AccessibilityFeatures>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

}  // namespace ash
