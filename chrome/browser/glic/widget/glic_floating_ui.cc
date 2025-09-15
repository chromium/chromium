// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_floating_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/widget/glic_inactive_floating_ui.h"

namespace glic {

GlicFloatingUi::GlicFloatingUi() = default;
GlicFloatingUi::~GlicFloatingUi() = default;

Host::Delegate* GlicFloatingUi::GetHostDelegate() {
  return this;
}

const mojom::PanelState& GlicFloatingUi::GetPanelState() const {
  NOTIMPLEMENTED();
  return panel_state_;
}

void GlicFloatingUi::Resize(const gfx::Size& size,
                            base::TimeDelta duration,
                            base::OnceClosure callback) {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::EnableDragResize(bool enabled) {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::Attach() {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::Detach() {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool GlicFloatingUi::IsShowing() const {
  NOTIMPLEMENTED();
  return false;
}

void GlicFloatingUi::Show() {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::Close() {
  NOTIMPLEMENTED();
}

std::unique_ptr<views::View> GlicFloatingUi::CreateView() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<GlicUiEmbedder> GlicFloatingUi::CreateInactiveEmbedder() const {
  return GlicInactiveFloatingUi::From(*this);
}

}  // namespace glic
