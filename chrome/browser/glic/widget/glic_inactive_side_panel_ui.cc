// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveSidePanelUi> GlicInactiveSidePanelUi::From(
    const GlicSidePanelUi& active_ui,
    base::WeakPtr<tabs::TabInterface> tab) {
  // Using `new` to access a private constructor.
  auto inactive_side_panel = base::WrapUnique(new GlicInactiveSidePanelUi(tab));
  inactive_side_panel->VisibilityChanged(/*visible=*/true);
  return inactive_side_panel;
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi(
    base::WeakPtr<tabs::TabInterface> tab)
    : tab_(tab),
      empty_embedder_delegate_(std::make_unique<EmptyEmbedderDelegate>()) {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return;
  }

  auto* glic_side_panel_coordinator =
      tab_->GetTabFeatures()->glic_side_panel_coordinator();
  coordinator_observation_.Observe(glic_side_panel_coordinator);
  glic_side_panel_coordinator->SetContentsView(CreateView(tab_));
}

std::unique_ptr<views::View> GlicInactiveSidePanelUi::CreateView(
    base::WeakPtr<tabs::TabInterface> tab) {
  return std::make_unique<views::Label>(u"Inactive",
                                        views::style::CONTEXT_DIALOG_TITLE);
}

GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

Host::EmbedderDelegate* GlicInactiveSidePanelUi::GetHostEmbedderDelegate() {
  return empty_embedder_delegate_.get();
}

bool GlicInactiveSidePanelUi::IsShowing() const {
  return is_showing_;
}

void GlicInactiveSidePanelUi::Show() {
  NOTIMPLEMENTED();
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
