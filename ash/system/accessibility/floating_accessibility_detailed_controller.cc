// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_detailed_controller.h"

#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/accessibility/accessibility_detailed_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(150);
constexpr int kDetailedViewHeightDip = 350;

}  // namespace

class FloatingAccessibilityDetailedController::DetailedBubbleView
    : public TrayBubbleView {
  METADATA_HEADER(DetailedBubbleView, TrayBubbleView)

 public:
  explicit DetailedBubbleView(TrayBubbleView::InitParams init_params)
      : TrayBubbleView(init_params) {}

  void UpdateAnchorRect(gfx::Rect anchor_rect,
                        views::BubbleBorder::Arrow alignment) {
    SetArrowWithoutResizing(alignment);
    SetAnchorRect(anchor_rect);
  }
};

FloatingAccessibilityDetailedController::
    FloatingAccessibilityDetailedController(Delegate* delegate)
    : DetailedViewDelegate(/*tray_controller*/ nullptr), delegate_(delegate) {
  Shell::Get()->activation_client()->AddObserver(this);
}

FloatingAccessibilityDetailedController::
    ~FloatingAccessibilityDetailedController() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->CloseNow();
}

void FloatingAccessibilityDetailedController::Show(
    gfx::Rect anchor_rect,
    views::BubbleBorder::Arrow alignment) {
  if (bubble_view_)
    return;

  TrayBubbleView::InitParams init_params;
  init_params.delegate = GetWeakPtr();
  init_params.parent_window = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_SettingBubbleContainer);
  init_params.anchor_mode = TrayBubbleView::AnchorMode::kRect;
  init_params.anchor_rect = anchor_rect;
  init_params.insets = gfx::Insets::TLBR(
      0, kBubbleMenuPadding, kBubbleMenuPadding, kBubbleMenuPadding);
  init_params.close_on_deactivate = false;
  init_params.translucent = true;
  init_params.type = TrayBubbleView::TrayBubbleType::kAccessibilityBubble;

  bubble_view_ = new DetailedBubbleView(init_params);
  bubble_view_->SetArrowWithoutResizing(alignment);

  detailed_view_ = bubble_view_->AddChildView(
      std::make_unique<AccessibilityDetailedView>(this));
  bubble_view_->SetPreferredSize(
      gfx::Size(kTrayMenuWidth, kDetailedViewHeightDip));
  bubble_view_->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_view_->SetCanActivate(true);
  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();

  // Focus on the bubble whenever it is shown.
  bubble_view_->RequestFocus();
}

void FloatingAccessibilityDetailedController::UpdateAnchorRect(
    gfx::Rect anchor_rect,
    views::BubbleBorder::Arrow alignment) {
  ui::ScopedLayerAnimationSettings settings(
      bubble_widget_->GetLayer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(kAnimationDuration);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  bubble_view_->UpdateAnchorRect(anchor_rect, alignment);
}

void FloatingAccessibilityDetailedController::CloseBubble() {
  if (delegate_->GetBubbleWidget())
    delegate_->GetBubbleWidget()->Activate();
  if (!bubble_widget_ || bubble_widget_->IsClosed())
    return;
  bubble_widget_->Close();
}

void FloatingAccessibilityDetailedController::TransitionToMainView(
    bool restore_focus) {
  CloseBubble();
}

std::u16string
FloatingAccessibilityDetailedController::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_FLOATING_ACCESSIBILITY_DETAILED_MENU);
}

views::Button* FloatingAccessibilityDetailedController::CreateBackButton(
    views::Button::PressedCallback callback) {
  views::ImageButton* button = static_cast<views::ImageButton*>(
      DetailedViewDelegate::CreateBackButton(std::move(callback)));
  ui::ImageModel image = ui::ImageModel::FromVectorIcon(
      kAutoclickCloseIcon, kColorAshIconColorPrimary);
  button->SetImageModel(views::Button::STATE_NORMAL, image);
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_FLOATING_ACCESSIBILITY_DETAILED_MENU_CLOSE));

  return button;
}

views::Button* FloatingAccessibilityDetailedController::CreateHelpButton(
    views::Button::PressedCallback callback) {
  auto* button = DetailedViewDelegate::CreateHelpButton(std::move(callback));
  button->SetVisible(false);
  return button;
}

void FloatingAccessibilityDetailedController::BubbleViewDestroyed() {
  detailed_view_ = nullptr;
  bubble_view_ = nullptr;
  bubble_widget_ = nullptr;

  delegate_->OnDetailedMenuClosed();
  // Hammer time, |this| is destroyed in the previous call.
}

void FloatingAccessibilityDetailedController::HideBubble(
    const TrayBubbleView* bubble_view) {}

void FloatingAccessibilityDetailedController::OnAccessibilityStatusChanged() {
  if (detailed_view_)
    detailed_view_->OnAccessibilityStatusChanged();
}

void FloatingAccessibilityDetailedController::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active || !bubble_widget_)
    return;

  views::Widget* gained_widget =
      views::Widget::GetWidgetForNativeView(gained_active);
  // Do not close the view if our bubble was activated.
  // Also, do not close the view the floating menu was activated, since it
  // controls our visibility.
  if (gained_widget == bubble_widget_ ||
      gained_widget == delegate_->GetBubbleWidget())
    return;

  bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
}

BEGIN_METADATA(FloatingAccessibilityDetailedController, DetailedBubbleView)
END_METADATA

}  // namespace ash
