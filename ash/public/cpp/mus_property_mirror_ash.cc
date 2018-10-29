// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/mus_property_mirror_ash.h"

#include "ash/public/cpp/window_properties.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/core/window_properties.h"

namespace {

template <typename T>
void MirrorOwnedProperty(aura::Window* window,
                         aura::Window* root_window,
                         const aura::WindowProperty<T*>* key) {
  T* value = window->GetProperty(key);
  if (!value)
    root_window->ClearProperty(key);
  else
    root_window->SetProperty(key, new T(*value));
}

}  // namespace

namespace ash {

MusPropertyMirrorAsh::MusPropertyMirrorAsh() = default;
MusPropertyMirrorAsh::~MusPropertyMirrorAsh() = default;

void MusPropertyMirrorAsh::MirrorPropertyFromWidgetWindowToRootWindow(
    aura::Window* window,
    aura::Window* root_window,
    const void* key) {
  if (key == aura::client::kAppType) {
    root_window->SetProperty(aura::client::kAppType,
                             window->GetProperty(aura::client::kAppType));
  } else if (key == kBlockedForAssistantSnapshotKey) {
    root_window->SetProperty(
        kBlockedForAssistantSnapshotKey,
        window->GetProperty(kBlockedForAssistantSnapshotKey));
  } else if (key == kPanelAttachedKey) {
    bool value = window->GetProperty(kPanelAttachedKey);
    root_window->SetProperty(kPanelAttachedKey, value);
  } else if (key == kShelfItemTypeKey) {
    int32_t value = window->GetProperty(kShelfItemTypeKey);
    root_window->SetProperty(kShelfItemTypeKey, value);
  } else if (key == kWindowStateTypeKey) {
    ash::mojom::WindowStateType value =
        window->GetProperty(kWindowStateTypeKey);
    root_window->SetProperty(kWindowStateTypeKey, value);
  } else if (key == kWindowPinTypeKey) {
    ash::mojom::WindowPinType value = window->GetProperty(kWindowPinTypeKey);
    root_window->SetProperty(kWindowPinTypeKey, value);
  } else if (key == aura::client::kAppIconKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kAppIconKey);
  } else if (key == kRestoreBoundsOverrideKey) {
    MirrorOwnedProperty(window, root_window, kRestoreBoundsOverrideKey);
  } else if (key == kRestoreWindowStateTypeOverrideKey) {
    ash::mojom::WindowStateType value =
        window->GetProperty(kRestoreWindowStateTypeOverrideKey);
    root_window->SetProperty(kRestoreWindowStateTypeOverrideKey, value);
  } else if (key == kShelfIDKey) {
    MirrorOwnedProperty(window, root_window, kShelfIDKey);
  } else if (key == aura::client::kDrawAttentionKey) {
    bool value = window->GetProperty(aura::client::kDrawAttentionKey);
    root_window->SetProperty(aura::client::kDrawAttentionKey, value);
  } else if (key == aura::client::kMinimumSize) {
    MirrorOwnedProperty(window, root_window, aura::client::kMinimumSize);
  } else if (key == aura::client::kTitleKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kTitleKey);
  } else if (key == aura::client::kTitleShownKey) {
    root_window->SetProperty(aura::client::kTitleShownKey,
                             window->GetProperty(aura::client::kTitleShownKey));
  } else if (key == aura::client::kTopViewInset) {
    root_window->SetProperty(aura::client::kTopViewInset,
                             window->GetProperty(aura::client::kTopViewInset));

  } else if (key == aura::client::kWindowIconKey) {
    MirrorOwnedProperty(window, root_window, aura::client::kWindowIconKey);
  } else if (key == kFrameActiveColorKey) {
    root_window->SetProperty(kFrameActiveColorKey,
                             window->GetProperty(kFrameActiveColorKey));
  } else if (key == kFrameInactiveColorKey) {
    root_window->SetProperty(kFrameInactiveColorKey,
                             window->GetProperty(kFrameInactiveColorKey));
  } else if (key == kImmersiveImpliedByFullscreen) {
    root_window->SetProperty(
        kImmersiveImpliedByFullscreen,
        window->GetProperty(kImmersiveImpliedByFullscreen));
  } else if (key == kImmersiveIsActive) {
    root_window->SetProperty(kImmersiveIsActive,
                             window->GetProperty(kImmersiveIsActive));
  } else if (key == kImmersiveTopContainerBoundsInScreen) {
    MirrorOwnedProperty(window, root_window,
                        kImmersiveTopContainerBoundsInScreen);
  } else if (key == kImmersiveWindowType) {
    root_window->SetProperty(kImmersiveWindowType,
                             window->GetProperty(kImmersiveWindowType));
  } else if (key == ui::kChildAXTreeID) {
    MirrorOwnedProperty(window, root_window, ui::kChildAXTreeID);
  } else if (key == wm::kWindowVisibilityAnimationDurationKey) {
    root_window->SetProperty(
        wm::kWindowVisibilityAnimationDurationKey,
        window->GetProperty(wm::kWindowVisibilityAnimationDurationKey));
  } else if (key == wm::kWindowVisibilityAnimationTransitionKey) {
    root_window->SetProperty(
        wm::kWindowVisibilityAnimationTransitionKey,
        window->GetProperty(wm::kWindowVisibilityAnimationTransitionKey));
  } else if (key == wm::kWindowVisibilityAnimationTypeKey) {
    root_window->SetProperty(
        wm::kWindowVisibilityAnimationTypeKey,
        window->GetProperty(wm::kWindowVisibilityAnimationTypeKey));
  } else if (key == wm::kWindowVisibilityAnimationVerticalPositionKey) {
    root_window->SetProperty(
        wm::kWindowVisibilityAnimationVerticalPositionKey,
        window->GetProperty(wm::kWindowVisibilityAnimationVerticalPositionKey));
  }
}

}  // namespace ash
