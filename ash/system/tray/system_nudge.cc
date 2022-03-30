// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The corner radius of the nudge view.
constexpr int kNudgeCornerRadius = 8;

// The blur radius for the nudge view's background.
constexpr int kNudgeBlurRadius = 30;

// The margin between the edge of the screen/shelf and the nudge widget bounds.
constexpr int kNudgeMargin = 8;

constexpr base::TimeDelta kNudgeBoundsAnimationTime = base::Milliseconds(250);

}  // namespace

class SystemNudge::SystemNudgeView : public views::View {
 public:
  explicit SystemNudgeView(std::unique_ptr<views::View> label_view,
                           const gfx::VectorIcon& icon_img,
                           const SystemNudgeParams& params) {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetColor(ShelfConfig::Get()->GetDefaultShelfColor());
    if (features::IsBackgroundBlurEnabled())
      layer()->SetBackgroundBlur(kNudgeBlurRadius);
    layer()->SetRoundedCornerRadius({kNudgeCornerRadius, kNudgeCornerRadius,
                                     kNudgeCornerRadius, kNudgeCornerRadius});

    SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorPrimary);

    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetPaintToLayer();
    icon_->layer()->SetFillsBoundsOpaquely(false);
    icon_->SetBounds(params.nudge_padding, params.nudge_padding,
                     params.icon_size, params.icon_size);
    icon_->SetImage(gfx::CreateVectorIcon(icon_img, icon_color));

    label_ = AddChildView(std::move(label_view));
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetPosition(gfx::Point(
        params.nudge_padding + params.icon_size + params.icon_label_spacing,
        params.nudge_padding));
  }

  ~SystemNudgeView() override = default;

  views::View* label_ = nullptr;
  views::ImageView* icon_ = nullptr;
};

SystemNudge::SystemNudge(const std::string& name,
                         int icon_size,
                         int icon_label_spacing,
                         int nudge_padding)
    : root_window_(Shell::GetRootWindowForNewWindows()) {
  params_.name = name;
  params_.icon_size = icon_size;
  params_.icon_label_spacing = icon_label_spacing;
  params_.nudge_padding = nudge_padding;
}

SystemNudge::~SystemNudge() = default;

void SystemNudge::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  CalculateAndSetWidgetBounds();
}

void SystemNudge::OnHotseatStateChanged(HotseatState old_state,
                                        HotseatState new_state) {
  CalculateAndSetWidgetBounds();
}

void SystemNudge::Show() {
  if (!widget_) {
    widget_ = std::make_unique<views::Widget>();

    shelf_observation_.Observe(
        RootWindowController::ForWindow(root_window_)->shelf());

    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.z_order = ui::ZOrderLevel::kFloatingWindow;
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    params.name = params_.name;
    params.layer_type = ui::LAYER_NOT_DRAWN;
    params.parent =
        root_window_->GetChildById(kShellWindowId_SettingBubbleContainer);
    widget_->Init(std::move(params));
  }

  nudge_view_ = widget_->SetContentsView(
      std::make_unique<SystemNudgeView>(CreateLabelView(), GetIcon(), params_));
  CalculateAndSetWidgetBounds();
  widget_->Show();

  const std::u16string accessibility_text = GetAccessibilityText();
  if (!accessibility_text.empty())
    nudge_view_->GetViewAccessibility().AnnounceText(accessibility_text);
}

void SystemNudge::Close() {
  widget_.reset();
}

void SystemNudge::CalculateAndSetWidgetBounds() {
  if (!widget_ || !root_window_ || !nudge_view_)
    return;

  DCHECK(nudge_view_->label_);

  gfx::Rect display_bounds = root_window_->bounds();
  ::wm::ConvertRectToScreen(root_window_, &display_bounds);
  gfx::Rect widget_bounds;

  // Calculate the nudge's size to ensure the label text and the icon accurately
  // fit.
  const int nudge_height =
      2 * params_.nudge_padding +
      std::max(nudge_view_->label_->bounds().height(), params_.icon_size);
  const int nudge_width = 2 * params_.nudge_padding + params_.icon_size +
                          params_.icon_label_spacing +
                          nudge_view_->label_->bounds().width();

  widget_bounds =
      gfx::Rect(display_bounds.x() + kNudgeMargin,
                display_bounds.bottom() - ShelfConfig::Get()->shelf_size() -
                    nudge_height - kNudgeMargin,
                nudge_width, nudge_height);

  Shelf* shelf = RootWindowController::ForWindow(root_window_)->shelf();
  bool shelf_hidden = shelf->GetVisibilityState() != SHELF_VISIBLE &&
                      shelf->GetAutoHideState() == SHELF_AUTO_HIDE_HIDDEN;

  if (base::i18n::IsRTL()) {
    if (shelf->alignment() == ShelfAlignment::kRight && !shelf_hidden) {
      widget_bounds.set_x(display_bounds.right() - nudge_width - kNudgeMargin -
                          ShelfConfig::Get()->shelf_size());
    } else {
      widget_bounds.set_x(display_bounds.right() - nudge_width - kNudgeMargin);
    }
  } else {
    if (shelf->alignment() == ShelfAlignment::kLeft && !shelf_hidden) {
      widget_bounds.set_x(display_bounds.x() +
                          ShelfConfig::Get()->shelf_size() + kNudgeMargin);
    }
  }

  if ((shelf->alignment() == ShelfAlignment::kBottom && shelf_hidden) ||
      shelf->alignment() == ShelfAlignment::kLeft ||
      shelf->alignment() == ShelfAlignment::kRight) {
    widget_bounds.set_y(display_bounds.bottom() - nudge_height - kNudgeMargin);
  }

  // Set the nudge's bounds above the hotseat when it is extended.
  HotseatWidget* hotseat_widget = shelf->hotseat_widget();
  if (hotseat_widget->state() == HotseatState::kExtended) {
    widget_bounds.set_y(hotseat_widget->GetTargetBounds().y() - nudge_height -
                        kNudgeMargin);
  }

  // Only run the widget bounds animation if the widget's bounds have already
  // been initialized.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (widget_->GetWindowBoundsInScreen().size() != gfx::Size()) {
    settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
        widget_->GetLayer()->GetAnimator());
    settings->SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings->SetTransitionDuration(kNudgeBoundsAnimationTime);
    settings->SetTweenType(gfx::Tween::EASE_OUT);
  }

  widget_->SetBounds(widget_bounds);
}

}  // namespace ash
