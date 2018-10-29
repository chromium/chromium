// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_ASH_WINDOW_MANAGER_H_
#define ASH_WS_ASH_WINDOW_MANAGER_H_

#include "ash/frame/ash_frame_caption_controller.h"
#include "ash/public/cpp/menu_utils.h"
#include "ash/public/interfaces/ash_window_manager.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/ws/common/types.h"
#include "services/ws/window_manager_interface.h"

namespace mojo {
class ScopedInterfaceEndpointHandle;
}

namespace ws {
class WindowTree;
}

namespace ash {

// Implementation of mojom::AshWindowManager, see it for details.
class AshWindowManager : public mojom::AshWindowManager,
                         public ws::WindowManagerInterface {
 public:
  AshWindowManager(ws::WindowTree* window_tree,
                   mojo::ScopedInterfaceEndpointHandle handle);
  ~AshWindowManager() override;

  // mojom::AshWindowManager:
  void AddWindowToTabletMode(ws::Id window_id) override;
  void ShowSnapPreview(ws::Id window_id, mojom::SnapDirection snap) override;
  void CommitSnap(ws::Id window_id, mojom::SnapDirection snap) override;
  void LockOrientation(ws::Id window_id,
                       mojom::OrientationLockType lock_orientation) override;
  void UnlockOrientation(ws::Id window_id) override;
  void MaximizeWindowByCaptionClick(ws::Id window_id,
                                    ui::mojom::PointerKind pointer) override;
  void BounceWindow(ws::Id window_id) override;
  void SetWindowFrameMenuItems(ws::Id window_id,
                               menu_utils::MenuItemList menu_item_list,
                               mojom::MenuDelegatePtr delegate) override;

 private:
  ws::WindowTree* window_tree_;
  mojo::AssociatedBinding<mojom::AshWindowManager> binding_;

  AshFrameCaptionController caption_controller_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowManager);
};

}  // namespace ash

#endif  // ASH_WS_ASH_WINDOW_MANAGER_H_
