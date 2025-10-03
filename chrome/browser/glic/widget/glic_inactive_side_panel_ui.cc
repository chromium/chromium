// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/glic/widget/inactive_view_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "ui/views/view.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForVisibleTab(
    base::WeakPtr<tabs::TabInterface> tab,
    content::WebContents* glic_webui_contents) {
  // Using `new` to access a private constructor.
  auto inactive_side_panel = base::WrapUnique(new GlicInactiveSidePanelUi(tab));
  inactive_side_panel->VisibilityChanged(/*visible=*/true);

  // Capture screenshot asynchronously and update the inactive panel.
  inactive_side_panel->inactive_view_controller_.CaptureScreenshot(
      glic_webui_contents);

  return inactive_side_panel;
}

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForBackgroundTab(
    base::WeakPtr<tabs::TabInterface> tab) {
  // Using `new` to access a private constructor.
  auto inactive_side_panel = base::WrapUnique(new GlicInactiveSidePanelUi(tab));
  // Mark the side panel for showing next time the tab becomes active.
  inactive_side_panel->Show();
  return inactive_side_panel;
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi(
    base::WeakPtr<tabs::TabInterface> tab)
    : tab_(tab) {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return;
  }

  auto* glic_side_panel_coordinator =
      tab_->GetTabFeatures()->glic_side_panel_coordinator();

  panel_visibility_subscription_ =
      glic_side_panel_coordinator->AddVisibilityCallback(
          base::BindRepeating(&GlicInactiveSidePanelUi::VisibilityChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  glic_side_panel_coordinator->SetContentsView(CreateView(tab_));
}

std::unique_ptr<views::View> GlicInactiveSidePanelUi::CreateView(
    base::WeakPtr<tabs::TabInterface> tab) {
  return inactive_view_controller_.CreateView();
}

GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

Host::EmbedderDelegate* GlicInactiveSidePanelUi::GetHostEmbedderDelegate() {
  // This should not be called for an inactive embedder. The delegate is managed
  // by the GlicInstanceImpl.
  NOTREACHED();
}

bool GlicInactiveSidePanelUi::IsShowing() const {
  return is_showing_;
}

void GlicInactiveSidePanelUi::Show() {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return;
  }
  SidePanelRegistry* registry = tab_->GetTabFeatures()->side_panel_registry();
  SidePanelEntry* glic_entry =
      registry->GetEntryForKey(SidePanelEntry::Key(SidePanelEntry::Id::kGlic));
  if (glic_entry) {
    registry->SetActiveEntry(glic_entry);
  }
}

void GlicInactiveSidePanelUi::Close() {
  // TODO: implement close.
  NOTIMPLEMENTED();
}

std::unique_ptr<GlicUiEmbedder>
GlicInactiveSidePanelUi::CreateInactiveEmbedder() const {
  NOTREACHED() << "The embedder is already inactive.";
}

void GlicInactiveSidePanelUi::VisibilityChanged(bool visible) {
  is_showing_ = visible;
}

}  // namespace glic
