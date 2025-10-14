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
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/common/chrome_features.h"
#include "ui/views/widget/widget_delegate.h"

namespace glic {

// static
gfx::Size GlicFloatingUi::GetDefaultSize() {
  return {features::kGlicMultiInstanceFloatyWidth.Get(),
          features::kGlicMultiInstanceFloatyHeight.Get()};
}
// end static

GlicFloatingUi::GlicFloatingUi(Profile* profile,
                               gfx::Rect initial_bounds,
                               GlicUiEmbedder::Delegate& delegate)
    : profile_(profile), delegate_(delegate) {
  CreateAndSetupWidget(initial_bounds);
  panel_state_.kind = mojom::PanelState_Kind::kDetached;
  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  tracker->OnPictureInPictureWidgetOpened(glic_widget_.get());
}

GlicFloatingUi::~GlicFloatingUi() {
  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  tracker->RemovePictureInPictureWidget(glic_widget_.get());
}

Host::EmbedderDelegate* GlicFloatingUi::GetHostEmbedderDelegate() {
  return this;
}

mojom::PanelState GlicFloatingUi::GetPanelState() const {
  return panel_state_;
}

gfx::Size GlicFloatingUi::GetPanelSize() {
  if (auto* glic_widget = GetGlicWidget()) {
    return glic_widget->GetSize();
  }
  return gfx::Size();
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

void GlicFloatingUi::CreateAndSetupWidget(gfx::Rect initial_bounds) {
  glic_widget_ =
      GlicWidget::Create(profile_, initial_bounds, nullptr, user_resizable_);
  // TODO: Setup Hotkeys and AccessibilityText.

  GetGlicWidget()->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#if BUILDFLAG(IS_MAC)
  GetGlicWidget()->SetActivationIndependence(true);
  GetGlicWidget()->SetVisibleOnAllWorkspaces(true);
  GetGlicWidget()->SetCanAppearInExistingFullscreenSpaces(true);
#endif

  glic_window_animator_ = std::make_unique<GlicWindowAnimator>(
      glic_widget_->GetWeakPtr(),
      base::BindRepeating(&GlicFloatingUi::MaybeSetWidgetCanResize,
                          weak_ptr_factory_.GetWeakPtr()));
  window_event_observer_ = std::make_unique<GlicWindowEventObserver>(
      glic_widget_->GetWeakPtr(), this);
}

void GlicFloatingUi::Resize(const gfx::Size& size,
                            base::TimeDelta duration,
                            base::OnceClosure callback) {
  // TODO: Don't animate while the user is manually resizing the widget.
  if (glic_window_animator_ && IsShowing()) {
    glic_window_animator_->AnimateSize(
        GlicWidget::ClampSize(size, GetGlicWidget()), duration,
        std::move(callback));
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

GlicWindowAnimator* GlicFloatingUi::window_animator() {
  return glic_window_animator_.get();
}

void GlicFloatingUi::OnDragComplete() {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::EnableDragResize(bool enabled) {
  user_resizable_ = enabled;

  MaybeSetWidgetCanResize();
  GetGlicView()->UpdateBackgroundColor();
  glic_window_animator_->MaybeAnimateToTargetSize();
}

void GlicFloatingUi::MaybeSetWidgetCanResize() {
  if (GetGlicWidget()->widget_delegate()->CanResize() == user_resizable_ ||
      glic_window_animator_->IsAnimating()) {
    // If the resize state is already correct or the widget is animating do not
    // update the resize state.
    return;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows when resize is enabled there is an invisible border added
  // around the client area. We need to make the widget larger or smaller to
  // keep the visible client area the same size.
  gfx::Rect previous_client_bounds =
      GetGlicWidget()->GetClientAreaBoundsInScreen();
#endif  // BUILDFLAG(IS_WIN)

  // Update resize state on widget delegate.
  GetGlicWidget()->widget_delegate()->SetCanResize(user_resizable_);

#if BUILDFLAG(IS_WIN)
  if (user_resizable_) {
    // Resizable so the widget area is larger than the client area.
    gfx::Rect new_widget_bounds =
        GetGlicWidget()->VisibleToWidgetBounds(previous_client_bounds);
    GetGlicWidget()->SetBoundsConstrained(new_widget_bounds);
  } else {
    // Not resizable so the client and widget areas are the same.
    GetGlicWidget()->SetBoundsConstrained(previous_client_bounds);
  }
#endif  // BUILDFLAG(IS_WIN)
}

void GlicFloatingUi::Attach() {
  NOTIMPLEMENTED();
}

void GlicFloatingUi::Detach() {
  // Floaty UI is already detached.
  NOTREACHED();
}

void GlicFloatingUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool GlicFloatingUi::IsShowing() const {
  return glic_widget_ != nullptr;
}

void GlicFloatingUi::Show() {
  GetGlicWidget()->Show();
  GetGlicView()->SetWebContents(delegate_->host().webui_contents());
  GetGlicView()->UpdateBackgroundColor();
  // TODO: Set up manual resize.
  window_event_observer_->SetDraggingAreasAndWatchForMouseEvents();
}

void GlicFloatingUi::Close() {
  window_event_observer_.reset();
  glic_window_animator_.reset();
  glic_widget_.reset();
  delegate_->WillCloseFor(FloatingEmbedderKey{});
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
  delegate_->SwitchConversation(
      ShowOptions::ForFloating(GetGlicWidget()->GetWindowBoundsInScreen()),
      std::move(info), std::move(callback));
}

}  // namespace glic
