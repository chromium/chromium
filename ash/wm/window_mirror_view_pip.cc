// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view_pip.h"

namespace ash {

WindowMirrorViewPip::WindowMirrorViewPip(aura::Window* source)
    : WindowMirrorView(source) {}

WindowMirrorViewPip::~WindowMirrorViewPip() = default;

void WindowMirrorViewPip::InitLayerOwner() {
  // Do nothing.
}

ui::Layer* WindowMirrorViewPip::GetMirrorLayer() {
  return layer();
}

}  // namespace ash
