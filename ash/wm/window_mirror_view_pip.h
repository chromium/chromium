// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MIRROR_VIEW_PIP_H_
#define ASH_WM_WINDOW_MIRROR_VIEW_PIP_H_

#include "ash/wm/window_mirror_view.h"

namespace ash {

// A view that mirrors the client area of a single (source) window.
// TODO(edcourtney): This currently displays nothing, but should display Android PIP windows with
// the controls not shown.
class ASH_EXPORT WindowMirrorViewPip : public WindowMirrorView {
 public:
  WindowMirrorViewPip(aura::Window* source, bool trilinear_filtering_on_init);
  ~WindowMirrorViewPip() override;

 protected:
  // WindowMirrorView:
  void InitLayerOwner() override;
  ui::Layer* GetMirrorLayer() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowMirrorViewPip);
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MIRROR_VIEW_PIP_H_
