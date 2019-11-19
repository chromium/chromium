// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view_pip.h"

namespace ash {

WindowMirrorViewPip::WindowMirrorViewPip(aura::Window* source,
                                         bool trilinear_filtering_on_init)
    : WindowMirrorView(source, trilinear_filtering_on_init) {}

WindowMirrorViewPip::~WindowMirrorViewPip() = default;

void WindowMirrorViewPip::InitLayerOwner() {
  // Do nothing.
}

ui::Layer* WindowMirrorViewPip::GetMirrorLayer() {
  return layer();
}

}  // namespace ash
