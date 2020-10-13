// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SNAP_CONTROLLER_IMPL_H_
#define ASH_FRAME_SNAP_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/caption_buttons/snap_controller.h"
#include "base/macros.h"

namespace ash {

class PhantomWindowController;

// A controller for toplevel window actions which can only run in Ash.
class ASH_EXPORT SnapControllerImpl : public SnapController {
 public:
  SnapControllerImpl();
  ~SnapControllerImpl() override;

  bool CanSnap(aura::Window* window) override;
  void ShowSnapPreview(aura::Window* window, SnapDirection snap) override;
  void CommitSnap(aura::Window* window, SnapDirection snap) override;

 private:
  std::unique_ptr<PhantomWindowController> phantom_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(SnapControllerImpl);
};

}  // namespace ash

#endif  // ASH_FRAME_SNAP_CONTROLLER_IMPL_H_
