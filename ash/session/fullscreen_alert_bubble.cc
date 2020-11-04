// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_alert_bubble.h"

#include <memory>

#include "ash/login/ui/system_label_button.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr int kAlertBubbleWidthDp = 384;
constexpr int kBubblePaddingDp = 16;
constexpr int kBubbleBetweenChildSpacingDp = 16;
constexpr int kBubbleBorderRadius = 8;
constexpr int kButtonPaddingDp = 8;
constexpr int kOffsetFromEdge = 32;
constexpr base::TimeDelta kAlertDuration = base::TimeDelta::FromSeconds(4);
constexpr base::TimeDelta kBubbleAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);

constexpr SkColor kAlertTextColor =
    SkColorSetA(gfx::kGoogleGrey200, SK_AlphaOPAQUE);

}  // namespace

class FullscreenAlertBubbleView : public views::View {
 public:
  METADATA_HEADER(FullscreenAlertBubbleView);

  FullscreenAlertBubbleView(views::Button::PressedCallback on_dismiss,
                            views::Button::PressedCallback on_exit_fullscreen) {
    SetPaintToLayer();
    SkColor background_color = AshColorProvider::Get()->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80);
    layer()->SetBackgroundBlur(
        static_cast<float>(AshColorProvider::LayerBlurSigma::kBlurDefault));
    SetBackground(views::CreateRoundedRectBackground(background_color,
                                                     kBubbleBorderRadius));
    layer()->SetFillsBoundsOpaquely(false);

    auto* main_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(kBubblePaddingDp),
        kBubbleBetweenChildSpacingDp));
    main_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    alert_text_ = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(IDS_FULLSCREEN_ALERT)));
    alert_text_->SetEnabledColor(kAlertTextColor);
    alert_text_->SetAutoColorReadabilityEnabled(false);
    alert_text_->SetMultiLine(true);

    auto* button_container = AddChildView(std::make_unique<views::View>());
    auto* button_layout =
        button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
            kButtonPaddingDp));
    button_layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    auto* dismiss =
        button_container->AddChildView(std::make_unique<SystemLabelButton>(
            views::Button::PressedCallback(),
            l10n_util::GetStringUTF16(IDS_DISMISS_BUTTON),
            SystemLabelButton::DisplayType::DEFAULT));
    dismiss->SetCallback(on_dismiss);

    auto* exit_fullscreen =
        button_container->AddChildView(std::make_unique<SystemLabelButton>(
            views::Button::PressedCallback(),
            l10n_util::GetStringUTF16(IDS_EXIT_FULLSCREEN_BUTTON),
            SystemLabelButton::DisplayType::ALERT_NO_ICON));
    exit_fullscreen->SetCallback(on_exit_fullscreen);
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size;
    size.set_width(kAlertBubbleWidthDp);
    size.set_height(GetHeightForWidth(kAlertBubbleWidthDp));
    return size;
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kAlertDialog;
    node_data->SetName(alert_text_->GetText());
  }

 private:
  views::Label* alert_text_ = nullptr;
};

BEGIN_METADATA(FullscreenAlertBubbleView, views::View)
END_METADATA

FullscreenAlertBubble::FullscreenAlertBubble()
    : bubble_widget_(std::make_unique<views::Widget>()),
      timer_(std::make_unique<base::OneShotTimer>()) {
  bubble_view_ = std::make_unique<FullscreenAlertBubbleView>(
      views::Button::PressedCallback(base::BindRepeating(
          &FullscreenAlertBubble::Dismiss, base::Unretained(this))),
      views::Button::PressedCallback(base::BindRepeating(
          &FullscreenAlertBubble::ExitFullscreen, base::Unretained(this))));

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.name = "FullscreenAlertBubble";
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_SettingBubbleContainer);
  params.bounds = CalculateBubbleBounds();

  bubble_widget_->Init(std::move(params));

  bubble_view_->set_owned_by_client();
  bubble_widget_->SetContentsView(bubble_view_.get());

  bubble_widget_->SetVisibilityChangedAnimationsEnabled(true);

  aura::Window* native_window = bubble_widget_->GetNativeWindow();
  wm::SetWindowVisibilityChangesAnimated(native_window);
  wm::SetWindowVisibilityAnimationType(
      native_window, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  wm::SetWindowVisibilityAnimationDuration(native_window,
                                           kBubbleAnimationDuration);
}

FullscreenAlertBubble::~FullscreenAlertBubble() = default;

void FullscreenAlertBubble::Show() {
  bubble_widget_->Show();

  timer_->Start(FROM_HERE, kAlertDuration,
                base::BindOnce(&FullscreenAlertBubble::Hide,
                               weak_ptr_factory_.GetWeakPtr()));
}

void FullscreenAlertBubble::Hide() {
  bubble_widget_->Hide();
}

void FullscreenAlertBubble::Dismiss(const ui::Event& event) {
  Hide();
}

void FullscreenAlertBubble::ExitFullscreen(const ui::Event& event) {
  FullscreenController::MaybeExitFullscreen();
  Hide();
}

gfx::Rect FullscreenAlertBubble::CalculateBubbleBounds() {
  gfx::Rect work_area = WorkAreaInsets::ForWindow(Shell::GetPrimaryRootWindow())
                            ->user_work_area_bounds();
  int x = work_area.x() + (work_area.width() - kAlertBubbleWidthDp) / 2;
  int y = work_area.y() + kOffsetFromEdge;
  return gfx::Rect(gfx::Point(x, y), bubble_view_->CalculatePreferredSize());
}

}  // namespace ash
