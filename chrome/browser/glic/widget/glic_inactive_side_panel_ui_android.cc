// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForVisibleTab(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate) {
  return base::WrapUnique(new GlicInactiveSidePanelUi());
}

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForBackgroundTab(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate) {
  return base::WrapUnique(new GlicInactiveSidePanelUi());
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi() = default;

GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

Host::EmbedderDelegate* GlicInactiveSidePanelUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicInactiveSidePanelUi::Show(const ShowOptions& options) {}

bool GlicInactiveSidePanelUi::IsShowing() const {
  return false;
}

void GlicInactiveSidePanelUi::Close() {}

void GlicInactiveSidePanelUi::Focus() {}

bool GlicInactiveSidePanelUi::HasFocus() {
  return false;
}

std::unique_ptr<GlicUiEmbedder>
GlicInactiveSidePanelUi::CreateInactiveEmbedder() const {
  return nullptr;
}

mojom::PanelState GlicInactiveSidePanelUi::GetPanelState() const {
  return mojom::PanelState();
}

gfx::Size GlicInactiveSidePanelUi::GetPanelSize() {
  return gfx::Size();
}

std::string GlicInactiveSidePanelUi::DescribeForTesting() {
  return "";
}

}  // namespace glic
