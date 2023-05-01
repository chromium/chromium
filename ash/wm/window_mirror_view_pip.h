// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MIRROR_VIEW_PIP_H_
#define ASH_WM_WINDOW_MIRROR_VIEW_PIP_H_

#include "ash/wm/window_mirror_view.h"

namespace ash {

// A view that mirrors the client area of a single (source) window.
// TODO(edcourtney): This currently displays nothing, but should display Android
// PIP windows with the controls not shown.
class WindowMirrorViewPip : public WindowMirrorView {
 public:
  explicit WindowMirrorViewPip(aura::Window* source);

  WindowMirrorViewPip(const WindowMirrorViewPip&) = delete;
  WindowMirrorViewPip& operator=(const WindowMirrorViewPip&) = delete;

  ~WindowMirrorViewPip() override;

 protected:
  // WindowMirrorView:
  void InitLayerOwner() override;
  ui::Layer* GetMirrorLayer() override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MIRROR_VIEW_PIP_H_
