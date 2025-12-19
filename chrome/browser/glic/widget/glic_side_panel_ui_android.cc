// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"

#include "base/notreached.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"

namespace glic {

GlicSidePanelUi::GlicSidePanelUi(Profile* profile,
                                 base::WeakPtr<tabs::TabInterface> tab,
                                 GlicUiEmbedder::Delegate& delegate,
                                 GlicInstanceMetrics& instance_metrics)
    : tab_(tab), delegate_(delegate) {}

GlicSidePanelUi::~GlicSidePanelUi() = default;

Host::EmbedderDelegate* GlicSidePanelUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicSidePanelUi::Show(const ShowOptions& options) {}

bool GlicSidePanelUi::IsShowing() const {
  return false;
}

void GlicSidePanelUi::Close() {}

void GlicSidePanelUi::Focus() {}

bool GlicSidePanelUi::HasFocus() {
  return false;
}

std::unique_ptr<GlicUiEmbedder> GlicSidePanelUi::CreateInactiveEmbedder()
    const {
  return GlicInactiveSidePanelUi::CreateForVisibleTab(tab_, *delegate_);
}

mojom::PanelState GlicSidePanelUi::GetPanelState() const {
  return mojom::PanelState();
}

gfx::Size GlicSidePanelUi::GetPanelSize() {
  return gfx::Size();
}

std::string GlicSidePanelUi::DescribeForTesting() {
  return "";
}

}  // namespace glic
