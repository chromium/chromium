// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_bubble.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/widget_finder.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Container view of UnifiedSystemTrayView to return fake preferred size for
// animation optimization. See UnifiedSystemTrayBubble::UpdateTransform().
// The fake size is specific to the structure of TrayBubbleView, so it is better
// to keep it separate from UnifiedSystemTrayView.
class ContainerView : public views::View {
 public:
  explicit ContainerView(UnifiedSystemTrayView* unified_view)
      : unified_view_(unified_view) {
    AddChildView(unified_view);
  }

  ~ContainerView() override = default;

  // views::View:
  void Layout() override { unified_view_->SetBoundsRect(GetContentsBounds()); }

  gfx::Size CalculatePreferredSize() const override {
    // If transform is used, always return the maximum height. Otherwise, return
    // the actual height.
    return gfx::Size(kTrayMenuWidth, unified_view_->IsTransformEnabled()
                                         ? unified_view_->GetExpandedHeight()
                                         : unified_view_->GetCurrentHeight());
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

 private:
  UnifiedSystemTrayView* const unified_view_;

  DISALLOW_COPY_AND_ASSIGN(ContainerView);
};

}  // namespace

// static
gfx::Insets UnifiedSystemTrayBubble::GetAdjustedAnchorInsets(
    UnifiedSystemTray* tray,
    TrayBubbleView* bubble_view) {
  gfx::Insets anchor_insets =
      tray->shelf()->GetSystemTrayAnchorView()->GetBubbleAnchorInsets();
  gfx::Insets bubble_insets = bubble_view->GetBorderInsets();
  if (tray->shelf()->IsHorizontalAlignment()) {
    anchor_insets -=
        gfx::Insets(kUnifiedMenuVerticalPadding - bubble_insets.bottom(), 0, 0,
                    bubble_insets.right() + anchor_insets.right());
  } else {
    anchor_insets -=
        gfx::Insets(0, 0, bubble_insets.bottom() + anchor_insets.bottom(), 0);
  }
  return anchor_insets;
}

UnifiedSystemTrayBubble::UnifiedSystemTrayBubble(UnifiedSystemTray* tray,
                                                 bool show_by_click)
    : controller_(
          std::make_unique<UnifiedSystemTrayController>(tray->model(), this)),
      tray_(tray) {
  if (show_by_click)
    time_shown_by_click_ = base::TimeTicks::Now();

  TrayBubbleView::InitParams init_params;
  init_params.anchor_alignment = tray_->GetAnchorAlignment();
  init_params.min_width = kTrayMenuWidth;
  init_params.max_width = kTrayMenuWidth;
  init_params.delegate = tray;
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_view = nullptr;
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = tray->shelf()->GetSystemTrayAnchorRect();
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;
  init_params.show_by_click = show_by_click;
  init_params.close_on_deactivate = false;

  bubble_view_ = new TrayBubbleView(init_params);

  unified_view_ = controller_->CreateView();
  time_to_click_recorder_ =
      std::make_unique<TimeToClickRecorder>(this, unified_view_);
  int max_height = CalculateMaxHeight();
  unified_view_->SetMaxHeight(max_height);
  bubble_view_->SetMaxHeight(max_height);
  bubble_view_->AddChildView(new ContainerView(unified_view_));

  bubble_view_->set_anchor_view_insets(
      GetAdjustedAnchorInsets(tray, bubble_view_));
  bubble_view_->set_color(SK_ColorTRANSPARENT);
  bubble_view_->layer()->SetFillsBoundsOpaquely(false);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  if (app_list_features::IsBackgroundBlurEnabled()) {
    bubble_widget_->client_view()->layer()->SetBackgroundBlur(
        kUnifiedMenuBackgroundBlur);
  }

  tray->tray_event_filter()->AddBubble(this);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
}

UnifiedSystemTrayBubble::~UnifiedSystemTrayBubble() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  tray_->tray_event_filter()->RemoveBubble(this);
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

gfx::Rect UnifiedSystemTrayBubble::GetBoundsInScreen() const {
  DCHECK(bubble_view_);
  return bubble_view_->GetBoundsInScreen();
}

bool UnifiedSystemTrayBubble::IsBubbleActive() const {
  return bubble_widget_ && bubble_widget_->IsActive();
}

void UnifiedSystemTrayBubble::ActivateBubble() {
  DCHECK(unified_view_);
  DCHECK(bubble_widget_);

  if (bubble_widget_->IsClosed())
    return;
  bubble_widget_->widget_delegate()->set_can_activate(true);
  bubble_widget_->Activate();
}

void UnifiedSystemTrayBubble::CloseNow() {
  if (!bubble_widget_)
    return;

  bubble_widget_->RemoveObserver(this);
  bubble_widget_->CloseNow();
  bubble_widget_ = nullptr;
}

void UnifiedSystemTrayBubble::EnsureExpanded() {
  if (!bubble_widget_)
    return;

  DCHECK(unified_view_);
  DCHECK(controller_);
  controller_->EnsureExpanded();
}

void UnifiedSystemTrayBubble::ShowAudioDetailedView() {
  if (!bubble_widget_)
    return;

  DCHECK(unified_view_);
  DCHECK(controller_);
  controller_->ShowAudioDetailedView();
}

void UnifiedSystemTrayBubble::UpdateBubble() {
  if (!bubble_widget_)
    return;
  DCHECK(bubble_view_);

  bubble_view_->UpdateBubble();
}

void UnifiedSystemTrayBubble::UpdateTransform() {
  if (!bubble_widget_)
    return;

  DCHECK(unified_view_);

  if (!unified_view_->IsTransformEnabled()) {
    unified_view_->SetTransform(gfx::Transform());
    DestroyBlurLayerForAnimation();
    SetFrameVisible(true);
    return;
  }

  SetFrameVisible(false);

  const int y_offset =
      unified_view_->GetExpandedHeight() - unified_view_->GetCurrentHeight();

  gfx::Transform transform;
  transform.Translate(0, y_offset);
  unified_view_->SetTransform(transform);

  CreateBlurLayerForAnimation();

  if (blur_layer_) {
    gfx::Rect blur_bounds = bubble_widget_->client_view()->layer()->bounds();
    blur_bounds.Inset(gfx::Insets(y_offset, 0, 0, 0));
    blur_layer_->layer()->SetBounds(blur_bounds);
  }
}

TrayBackgroundView* UnifiedSystemTrayBubble::GetTray() const {
  return tray_;
}

TrayBubbleView* UnifiedSystemTrayBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* UnifiedSystemTrayBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

int UnifiedSystemTrayBubble::CalculateMaxHeight() const {
  gfx::Rect anchor_bounds =
      tray_->shelf()->GetSystemTrayAnchorView()->GetBoundsInScreen();
  int bottom = tray_->shelf()->IsHorizontalAlignment() ? anchor_bounds.y()
                                                       : anchor_bounds.bottom();
  int free_space_height_above_anchor =
      bottom - tray_->shelf()->GetUserWorkAreaBounds().y();
  return free_space_height_above_anchor - kUnifiedMenuVerticalPadding * 2;
}

void UnifiedSystemTrayBubble::OnDisplayConfigurationChanged() {
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::OnWindowActivated(ActivationReason reason,
                                                aura::Window* gained_active,
                                                aura::Window* lost_active) {
  if (!gained_active)
    return;

  // Don't close the bubble if a transient child is gaining or losing
  // activation.
  if (bubble_widget_ == GetInternalWidgetForWindow(gained_active) ||
      ::wm::HasTransientAncestor(gained_active,
                                 bubble_widget_->GetNativeWindow()) ||
      (lost_active && ::wm::HasTransientAncestor(
                          lost_active, bubble_widget_->GetNativeWindow()))) {
    return;
  }

  tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::RecordTimeToClick() {
  // Ignore if the tray bubble is not opened by click.
  if (!time_shown_by_click_)
    return;

  UMA_HISTOGRAM_TIMES("ChromeOS.SystemTray.TimeToClick",
                      base::TimeTicks::Now() - time_shown_by_click_.value());

  time_shown_by_click_.reset();
}

void UnifiedSystemTrayBubble::OnTabletModeStarted() {
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::OnTabletModeEnded() {
  UpdateBubbleBounds();
}

void UnifiedSystemTrayBubble::UpdateBubbleBounds() {
  int max_height = CalculateMaxHeight();
  unified_view_->SetMaxHeight(max_height);
  bubble_view_->SetMaxHeight(max_height);
  bubble_view_->ChangeAnchorAlignment(tray_->GetAnchorAlignment());
  bubble_view_->ChangeAnchorRect(tray_->shelf()->GetSystemTrayAnchorRect());
}

void UnifiedSystemTrayBubble::CreateBlurLayerForAnimation() {
  if (!app_list_features::IsBackgroundBlurEnabled())
    return;

  if (blur_layer_)
    return;

  DCHECK(bubble_widget_);

  bubble_widget_->client_view()->layer()->SetBackgroundBlur(0);

  blur_layer_ = views::Painter::CreatePaintedLayer(
      views::Painter::CreateSolidRoundRectPainter(SK_ColorTRANSPARENT, 0));
  blur_layer_->layer()->SetFillsBoundsOpaquely(false);

  bubble_widget_->GetLayer()->Add(blur_layer_->layer());
  bubble_widget_->GetLayer()->StackAtBottom(blur_layer_->layer());

  blur_layer_->layer()->SetBounds(
      bubble_widget_->client_view()->layer()->bounds());
  blur_layer_->layer()->SetBackgroundBlur(kUnifiedMenuBackgroundBlur);
}

void UnifiedSystemTrayBubble::DestroyBlurLayerForAnimation() {
  if (!app_list_features::IsBackgroundBlurEnabled())
    return;

  if (!blur_layer_)
    return;

  blur_layer_.reset();

  bubble_widget_->client_view()->layer()->SetBackgroundBlur(
      kUnifiedMenuBackgroundBlur);
}

void UnifiedSystemTrayBubble::SetFrameVisible(bool visible) {
  DCHECK(bubble_widget_);
  bubble_widget_->non_client_view()->frame_view()->SetVisible(visible);
}

}  // namespace ash
