// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The corner radius of the nudge view.
constexpr int kNudgeCornerRadius = 8;

// The blur radius for the nudge view's background.
constexpr int kNudgeBlurRadius = 30;

// The size of the clipboard icon.
constexpr int kClipboardIconSize = 20;

// The size of the keyboard shortcut icon.
constexpr int kKeyboardShortcutIconSize = 14;

// The minimum width of the label.
constexpr int kMinLabelWidth = 200;

// The margin between the edge of the screen/shelf and the nudge widget bounds.
constexpr int kNudgeMargin = 8;

// The spacing between the icon and label in the nudge view.
constexpr int kIconLabelSpacing = 16;

// The padding which separates the nudge's border with its inner contents.
constexpr int kNudgePadding = 16;

constexpr base::TimeDelta kNudgeBoundsAnimationTime =
    base::TimeDelta::FromMilliseconds(250);

bool IsAssistantAvailable() {
  AssistantStateBase* state = AssistantState::Get();
  return state->allowed_state() ==
             chromeos::assistant::AssistantAllowedState::ALLOWED &&
         state->settings_enabled().value_or(false);
}

}  // namespace

class ClipboardNudge::ClipboardNudgeView : public views::View {
 public:
  explicit ClipboardNudgeView(ClipboardNudgeType nudge_type) {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    layer()->SetColor(ShelfConfig::Get()->GetDefaultShelfColor());
    if (features::IsBackgroundBlurEnabled())
      layer()->SetBackgroundBlur(kNudgeBlurRadius);
    layer()->SetRoundedCornerRadius({kNudgeCornerRadius, kNudgeCornerRadius,
                                     kNudgeCornerRadius, kNudgeCornerRadius});

    SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorPrimary);

    clipboard_icon_ = AddChildView(std::make_unique<views::ImageView>());
    clipboard_icon_->SetPaintToLayer();
    clipboard_icon_->layer()->SetFillsBoundsOpaquely(false);
    clipboard_icon_->SetBounds(kNudgePadding, kNudgePadding, kClipboardIconSize,
                               kClipboardIconSize);
    clipboard_icon_->SetImage(gfx::CreateVectorIcon(
        nudge_type == kZeroStateNudge ? kClipboardEmptyIcon : kClipboardIcon,
        icon_color));

    label_ = AddChildView(std::make_unique<views::StyledLabel>());
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetPosition(gfx::Point(
        kNudgePadding + kClipboardIconSize + kIconLabelSpacing, kNudgePadding));

    bool use_launcher_key = ui::DeviceUsesKeyboardLayout2();

    // Set the keyboard shortcut icon depending on whether search button or
    // launcher button is being used.
    gfx::ImageSkia shortcut_icon;
    if (use_launcher_key) {
      if (IsAssistantAvailable()) {
        shortcut_icon = gfx::CreateVectorIcon(gfx::IconDescription(
            kClipboardLauncherOuterIcon, kKeyboardShortcutIconSize, icon_color,
            &kClipboardLauncherInnerIcon));
      } else {
        shortcut_icon =
            gfx::CreateVectorIcon(kClipboardLauncherNoAssistantIcon,
                                  kKeyboardShortcutIconSize, icon_color);
      }
    } else {
      shortcut_icon = gfx::CreateVectorIcon(
          kClipboardSearchIcon, kKeyboardShortcutIconSize, icon_color);
    }
    std::unique_ptr<views::ImageView> keyboard_shortcut_icon;
    keyboard_shortcut_icon = std::make_unique<views::ImageView>();
    keyboard_shortcut_icon->SetImage(shortcut_icon);
    keyboard_shortcut_icon->SetBorder(views::CreateEmptyBorder(2, 4, 0, -2));

    // Set the text for |label_|.
    base::string16 shortcut_key = l10n_util::GetStringUTF16(
        use_launcher_key ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
                         : IDS_ASH_SHORTCUT_MODIFIER_SEARCH);
    size_t offset;
    base::string16 label_text = l10n_util::GetStringFUTF16(
        nudge_type == kZeroStateNudge
            ? IDS_ASH_MULTIPASTE_ZERO_STATE_CONTEXTUAL_NUDGE
            : IDS_ASH_MULTIPASTE_CONTEXTUAL_NUDGE,
        shortcut_key, &offset);
    offset = offset + shortcut_key.length();
    label_->SetText(label_text);

    // Set the color of the text surrounding the shortcut icon.
    views::StyledLabel::RangeStyleInfo text_color;
    text_color.override_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary);
    label_->AddStyleRange(gfx::Range(0, offset), text_color);
    label_->AddStyleRange(gfx::Range(offset + 1, label_text.length()),
                          text_color);

    // Add the shortcut icon to |label_|.
    views::StyledLabel::RangeStyleInfo icon_style;
    icon_style.custom_view = keyboard_shortcut_icon.get();
    label_->AddCustomView(std::move(keyboard_shortcut_icon));
    label_->AddStyleRange(gfx::Range(offset, offset + 1), icon_style);

    label_->SizeToFit(kMinLabelWidth);
    label_->SetDisplayedOnBackgroundColor(SK_ColorTRANSPARENT);
  }

  ~ClipboardNudgeView() override = default;

  views::StyledLabel* label_ = nullptr;
  views::ImageView* clipboard_icon_ = nullptr;
};

ClipboardNudge::ClipboardNudge(ClipboardNudgeType nudge_type)
    : widget_(std::make_unique<views::Widget>()),
      nudge_type_(nudge_type),
      root_window_(Shell::GetRootWindowForNewWindows()) {
  shelf_observation_.Observe(
      RootWindowController::ForWindow(root_window_)->shelf());

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.name = "ClipboardContextualNudge";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent =
      root_window_->GetChildById(kShellWindowId_SettingBubbleContainer);
  widget_->Init(std::move(params));

  nudge_view_ = widget_->SetContentsView(
      std::make_unique<ClipboardNudgeView>(nudge_type));
  CalculateAndSetWidgetBounds();
  widget_->Show();
}

ClipboardNudge::~ClipboardNudge() = default;

void ClipboardNudge::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  CalculateAndSetWidgetBounds();
}

void ClipboardNudge::OnHotseatStateChanged(HotseatState old_state,
                                           HotseatState new_state) {
  CalculateAndSetWidgetBounds();
}

void ClipboardNudge::Close() {
  widget_.reset();
}

void ClipboardNudge::CalculateAndSetWidgetBounds() {
  gfx::Rect display_bounds = root_window_->bounds();
  ::wm::ConvertRectToScreen(root_window_, &display_bounds);
  gfx::Rect widget_bounds;

  // Calculate the nudge's size to ensure the label text accurately fits.
  const int nudge_height =
      2 * kNudgePadding + nudge_view_->label_->bounds().height();
  const int nudge_width = 2 * kNudgePadding + kClipboardIconSize +
                          kIconLabelSpacing +
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
