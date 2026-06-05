// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/widget/conversions.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForVisibleTab(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate) {
  return base::WrapUnique(new GlicInactiveSidePanelUi(tab, delegate));
}

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForBackgroundTab(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate) {
  auto inactive_side_panel =
      base::WrapUnique(new GlicInactiveSidePanelUi(tab, delegate));
  return inactive_side_panel;
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate)
    : tab_(tab), delegate_(delegate) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (glic_side_panel_coordinator) {
    glic_side_panel_coordinator->SetWebContents(nullptr);
    state_subscription_ = glic_side_panel_coordinator->AddStateCallback(
        base::BindRepeating(&GlicInactiveSidePanelUi::OnSidePanelStateChanged,
                            base::Unretained(this)));
  }
}

GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

Host::EmbedderDelegate* GlicInactiveSidePanelUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicInactiveSidePanelUi::Show(const ShowOptions& options) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  glic_side_panel_coordinator->Show(ConvertToCoordinatorShowOptions(
      options, glic_side_panel_coordinator->SupportsPeek()));
}

bool GlicInactiveSidePanelUi::IsShowing() const {
  return GlicSidePanelCoordinator::IsShowing(tab_.get());
}

bool GlicInactiveSidePanelUi::IsShowingOrBackgrounded() const {
  return GlicSidePanelCoordinator::IsShowingOrBackgrounded(tab_.get());
}

void GlicInactiveSidePanelUi::Close(const CloseOptions& options) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  glic_side_panel_coordinator->Close(options);
}

void GlicInactiveSidePanelUi::Focus() {}

bool GlicInactiveSidePanelUi::HasFocus() {
  return IsShowing();
}

GlicSidePanelCoordinator* GlicInactiveSidePanelUi::GetGlicSidePanelCoordinator()
    const {
  return GlicSidePanelCoordinator::GetForTab(tab_.get());
}

void GlicInactiveSidePanelUi::OnSidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  if (state == GlicSidePanelCoordinator::State::kShown && tab_) {
    delegate_->Show(ShowOptions::ForSidePanel(*tab_));
  }
}

std::unique_ptr<GlicUiEmbedder>
GlicInactiveSidePanelUi::CreateInactiveEmbedder() const {
  NOTREACHED() << "The embedder is already inactive.";
}

mojom::PanelState GlicInactiveSidePanelUi::GetPanelState() const {
  mojom::PanelState state;
  state.kind = glic::mojom::PanelStateKind::kHidden;
  return state;
}

gfx::Size GlicInactiveSidePanelUi::GetPanelSize() {
  return gfx::Size();
}

void GlicInactiveSidePanelUi::InitializeAfterRegistration() {
  // When an active embedder is deactivated while its tab is still visible,
  // we transition the bottom sheet to the peek state rather than closing it.
  // This is called after the inactive embedder has been fully registered in the
  // instance, ensuring the state is completely consistent to avoid synchronous
  // reentrancy issues.
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (tab_ && tab_->IsActivated() && glic_side_panel_coordinator &&
      glic_side_panel_coordinator->SupportsPeek() &&
      glic_side_panel_coordinator->state() ==
          GlicSidePanelCoordinator::State::kShown) {
    SidePanelShowOptions side_panel_options{*tab_};
    side_panel_options.prefer_peek = true;
    Show(ShowOptions{side_panel_options});
  }
}

std::string GlicInactiveSidePanelUi::DescribeForTesting() {
  return "";
}

}  // namespace glic
