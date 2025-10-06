// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_floating_ui.h"

#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/glic/widget/glic_inactive_floating_ui.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"

namespace {

// constexpr static int kDraggableAreaHeight = 44;

}  // namespace

namespace glic {

GlicFloatingUi::GlicFloatingUi(Profile* profile,
                               GlicUiEmbedder::Delegate& delegate)
    : profile_(profile), delegate_(delegate) {
  LOG(ERROR) << "tnp: Floating UI created";
  CreateAndSetupWidget();
  panel_state_.kind = mojom::PanelState_Kind::kDetached;
}

GlicFloatingUi::~GlicFloatingUi() = default;

Host::EmbedderDelegate* GlicFloatingUi::GetHostEmbedderDelegate() {
  return this;
}

const mojom::PanelState& GlicFloatingUi::GetPanelState() const {
  return panel_state_;
}

GlicWidget* GlicFloatingUi::GetGlicWidget() const {
  return glic_widget_.get();
}

GlicView* GlicFloatingUi::GetGlicView() const {
  if (auto* glic_widget = GetGlicWidget()) {
    return glic_widget->GetGlicView();
  }
  return nullptr;
}

void GlicFloatingUi::CreateAndSetupWidget() {
  glic_widget_ =
      GlicWidget::Create(profile_, gfx::Rect(10, 10, 400, 400), nullptr, true);
  // TODO: Setup Hotkeys and AccessibilityText

  GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(true);
  GetGlicWidget()->SetVisibleOnAllWorkspaces(true);
  GetGlicWidget()->SetCanAppearInExistingFullscreenSpaces(true);
#endif
}

void GlicFloatingUi::Resize(const gfx::Size& size,
                            base::TimeDelta duration,
                            base::OnceClosure callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

void GlicFloatingUi::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  if (auto* glic_view = GetGlicView()) {
    glic_view->SetDraggableAreas(draggable_areas);
  }
}

// void GlicFloatingUi::SetDraggingAreasAndWatchForMouseEvents() {
//   if (window_event_observer_) {
//     return;
//   }

//   window_event_observer_ =
//       std::make_unique<WindowEventObserver>(this, GetGlicView());

//   if (!draggable_area_) {
//     // Set the draggable area to the top bar of the window.
//     GetGlicView()->SetDraggableAreas(
//         {{0, 0, GetGlicView()->width(), kDraggableAreaHeight}});
//   }
// }

void GlicFloatingUi::EnableDragResize(bool enabled) {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::Attach() {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::Detach() {
  // Floaty UI is already detached
  NOTREACHED();
}

void GlicFloatingUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool GlicFloatingUi::IsShowing() const {
  return glic_widget_ != nullptr;
}

void GlicFloatingUi::Show() {
  LOG(ERROR) << "tnp: Floating UI show";
  GetGlicWidget()->Show();
  GetGlicView()->SetWebContents(delegate_->host().webui_contents());
  GetGlicView()->UpdateBackgroundColor();
  // TODO: Setup resize and drag
  // SetDraggableAreasAndWatchForMouseEvents();
}

void GlicFloatingUi::Close() {
  glic_widget_.reset();
  delegate_->WillCloseFor(/*tab=*/nullptr);
}

void GlicFloatingUi::ClosePanel() {
  Close();
}

std::unique_ptr<GlicUiEmbedder> GlicFloatingUi::CreateInactiveEmbedder() const {
  return GlicInactiveFloatingUi::From(*this);
}

views::View* GlicFloatingUi::GetViewForTesting() {
  NOTIMPLEMENTED();
  return nullptr;
}

void GlicFloatingUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace glic
