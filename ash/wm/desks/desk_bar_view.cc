// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_bar_view.h"

#include "ui/aura/window.h"

namespace ash {

// -----------------------------------------------------------------------------
// DeskBarView:

DeskBarView::DeskBarView(aura::Window* root)
    : DeskBarViewBase(root, DeskBarViewBase::Type::kDeskButton) {}

const char* DeskBarView::GetClassName() const {
  return "DeskBarView";
}

}  // namespace ash
