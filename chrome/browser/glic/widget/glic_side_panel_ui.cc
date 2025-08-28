// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui.h"

#include "base/notimplemented.h"

namespace glic {

GlicSidePanelUi::GlicSidePanelUi() = default;
GlicSidePanelUi::~GlicSidePanelUi() = default;

const mojom::PanelState& GlicSidePanelUi::GetPanelState() const {
  NOTIMPLEMENTED();
  return panel_state_;
}

void GlicSidePanelUi::Resize(const gfx::Size& size,
                             base::TimeDelta duration,
                             base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::EnableDragResize(bool enabled) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::Attach() {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::Detach() {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool GlicSidePanelUi::IsShowing() const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace glic
