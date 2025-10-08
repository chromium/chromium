// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_floating_ui.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "chrome/browser/glic/widget/glic_inactive_floating_ui.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"

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

mojom::PanelState GlicFloatingUi::GetPanelState() const {
  return panel_state_;
}

GlicWindowAnimator* GlicFloatingUi::window_animator() {
  return glic_window_animator_.get();
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

  glic_window_animator_ = std::make_unique<GlicWindowAnimator>(
      glic_widget_->GetWeakPtr(), base::DoNothing());
}

void GlicFloatingUi::Resize(const gfx::Size& size,
                            base::TimeDelta duration,
                            base::OnceClosure callback) {
  glic_size_ = size;

  // TODO: Don't animate while the user is manually resizing the widget.
  if (glic_window_animator_ && IsShowing()) {
    glic_window_animator_->AnimateSize(
        GlicWidget::GetLastRequestedSizeClamped(GetGlicWidget(), glic_size_),
        duration, std::move(callback));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }
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
  glic_window_animator_.reset();
  glic_widget_.reset();
  delegate_->WillCloseFor(/*tab=*/nullptr);
}

void GlicFloatingUi::ClosePanel() {
  Close();
}

void GlicFloatingUi::Focus() {
  NOTIMPLEMENTED();
}

std::unique_ptr<GlicUiEmbedder> GlicFloatingUi::CreateInactiveEmbedder() const {
  return GlicInactiveFloatingUi::From(*this);
}

views::View* GlicFloatingUi::GetView() {
  return GetGlicView();
}

void GlicFloatingUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace glic
