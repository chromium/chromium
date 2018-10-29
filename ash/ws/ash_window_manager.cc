// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/ash_window_manager.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "services/ws/window_tree.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

AshWindowManager::AshWindowManager(ws::WindowTree* window_tree,
                                   mojo::ScopedInterfaceEndpointHandle handle)
    : window_tree_(window_tree),
      binding_(this,
               mojo::AssociatedInterfaceRequest<mojom::AshWindowManager>(
                   std::move(handle))) {}

AshWindowManager::~AshWindowManager() = default;

void AshWindowManager::AddWindowToTabletMode(ws::Id window_id) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window))
    Shell::Get()->tablet_mode_controller()->AddWindow(window);
  else
    DVLOG(1) << "AddWindowToTableMode passed invalid window, id=" << window_id;
}

void AshWindowManager::ShowSnapPreview(ws::Id window_id,
                                       mojom::SnapDirection snap) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window) &&
      caption_controller_.CanSnap(window)) {
    caption_controller_.ShowSnapPreview(window, snap);
  } else {
    DVLOG(1) << "ShowSnapPreview passed invalid window, id=" << window_id;
  }
}

void AshWindowManager::CommitSnap(ws::Id window_id, mojom::SnapDirection snap) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window) &&
      caption_controller_.CanSnap(window)) {
    caption_controller_.CommitSnap(window, snap);
  } else {
    DVLOG(1) << "CommitSnap passed invalid window, id=" << window_id;
  }
}

void AshWindowManager::LockOrientation(
    ws::Id window_id,
    mojom::OrientationLockType lock_orientation) {
  if (window_tree_->connection_type() ==
      ws::WindowTree::ConnectionType::kEmbedding) {
    DVLOG(1) << "LockOrientation not allowed from embed connection";
    return;
  }

  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window) {
    Shell::Get()->screen_orientation_controller()->LockOrientationForWindow(
        window, lock_orientation);
  } else {
    DVLOG(1) << "LockOrientation passed invalid window, id=" << window_id;
  }
}

void AshWindowManager::UnlockOrientation(ws::Id window_id) {
  if (window_tree_->connection_type() ==
      ws::WindowTree::ConnectionType::kEmbedding) {
    DVLOG(1) << "UnlockOrientation not allowed from embed connection";
    return;
  }

  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window) {
    Shell::Get()->screen_orientation_controller()->UnlockOrientationForWindow(
        window);
  } else {
    DVLOG(1) << "UnlockOrientation passed invalid window, id=" << window_id;
  }
}

void AshWindowManager::MaximizeWindowByCaptionClick(
    ws::Id window_id,
    ui::mojom::PointerKind pointer) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (!window || !window_tree_->IsTopLevel(window)) {
    DVLOG(1) << "MaximizeWindowByCaptionClick passed invalid window, id="
             << window_id;
    return;
  }

  if (pointer == ui::mojom::PointerKind::MOUSE) {
    base::RecordAction(base::UserMetricsAction("Caption_ClickTogglesMaximize"));
  } else if (pointer == ui::mojom::PointerKind::TOUCH) {
    base::RecordAction(
        base::UserMetricsAction("Caption_GestureTogglesMaximize"));
  } else {
    DVLOG(1) << "MaximizeWindowByCaptionClick passed invalid event";
    return;
  }

  const wm::WMEvent wm_event(wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
  wm::GetWindowState(window)->OnWMEvent(&wm_event);
}

void AshWindowManager::BounceWindow(ws::Id window_id) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (!window || !window_tree_->IsTopLevel(window)) {
    DVLOG(1) << "BounceWindow passed invalid window, id=" << window_id;
    return;
  }
  ::wm::AnimateWindow(window, ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
}

void AshWindowManager::SetWindowFrameMenuItems(
    ws::Id window_id,
    menu_utils::MenuItemList menu_item_list,
    mojom::MenuDelegatePtr delegate) {
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (!window) {
    DVLOG(1) << "SetWindowFrameMenuItems passed invalid window, id="
             << window_id;
    return;
  }

  NonClientFrameViewAsh* frame_view = NonClientFrameViewAsh::Get(window);
  if (!frame_view) {
    DVLOG(1) << "SetWindowFrameMenuItems called on frameless window";
    return;
  }

  frame_view->SetWindowFrameMenuItems(menu_item_list, std::move(delegate));
}

}  // namespace ash
