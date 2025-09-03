// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/service/glic_instance.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"

namespace glic {

GlicSidePanelUi::GlicSidePanelUi(BrowserWindowInterface* associated_bwi,
                                 GlicInstance& instance)
    : associated_bwi_(associated_bwi), instance_(instance) {}
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

void GlicSidePanelUi::Show() {
  CHECK(associated_bwi_);
  auto* side_panel_coordinator =
      associated_bwi_->GetFeatures().side_panel_coordinator();
  side_panel_coordinator->Show(SidePanelEntry::Id::kGlic);
}

std::unique_ptr<GlicView> GlicSidePanelUi::CreateGlicView() {
  auto glic_view = std::make_unique<GlicView>(
      instance_->profile(), GlicWidget::GetInitialSize(), nullptr);
  // TODO(refactor): use the right host when we have multiple hosts
  glic_view->SetWebContents(instance_->host().webui_contents());
  glic_view->UpdateBackgroundColor();
  return glic_view;
}

}  // namespace glic
