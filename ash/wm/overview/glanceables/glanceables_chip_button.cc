// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/glanceables/glanceables_chip_button.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The color and layout parameters of the chip.
constexpr int kBetweenChildSpacing = 8;
constexpr gfx::Insets kBorderInsetsWithoutActionButton(12);
constexpr gfx::Insets kBorderInsetsWithActionButton =
    gfx::Insets::TLBR(12, 12, 12, 8);
constexpr gfx::Size kChipSize(228, 62);
constexpr int kRoundedCornerRadius = 12;
constexpr ui::ColorId kBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBaseOpaque;

// The layout parameters of icon.
constexpr gfx::Insets kIconMargins = gfx::Insets::TLBR(0, 2, 0, 14);

// The colors and fonts of title and subtitle.
constexpr int kTitleSpacing = 2;
constexpr TypographyToken kTitleFont = TypographyToken::kCrosButton1;
constexpr ui::ColorId kTitleColorId = cros_tokens::kCrosSysOnSurface;
constexpr TypographyToken kSubtitleFont = TypographyToken::kCrosAnnotation2;
constexpr ui::ColorId kSubtitleColorId = cros_tokens::kCrosSysOnSurfaceVariant;

// The colors and layout parameters of remove panel.
constexpr ui::ColorId kRemoveIconColorId = cros_tokens::kCrosSysOnSurface;
constexpr int kRemoveIconSize = 20;

}  // namespace

//------------------------------------------------------------------------------
// GlanceablesChipButton::RemovalChipMenuController:
// The removal chip panel which contains one option to remove the chip.
class GlanceablesChipButton::RemovalChipMenuController
    : public views::ContextMenuController {
 public:
  explicit RemovalChipMenuController(ui::SimpleMenuModel::Delegate* delegate)
      : menu_model_(delegate), menu_adapter_(&menu_model_) {
    menu_model_.AddItemWithStringIdAndIcon(
        /*command_id=*/0, IDS_GLANCEABLES_OVERVIEW_REMOVE_CHIP,
        ui::ImageModel::FromVectorIcon(views::kCloseIcon, kRemoveIconColorId,
                                       kRemoveIconSize));
    int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                    views::MenuRunner::CONTEXT_MENU |
                    views::MenuRunner::FIXED_ANCHOR;
    menu_runner_ = std::make_unique<views::MenuRunner>(
        menu_adapter_.CreateMenu(), run_types);
  }

  RemovalChipMenuController(const RemovalChipMenuController&) = delete;
  RemovalChipMenuController& operator=(const RemovalChipMenuController&) =
      delete;
  ~RemovalChipMenuController() override = default;

 private:
  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    // Show the panel on top right of the cursor.
    menu_runner_->RunMenuAt(
        source->GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
        views::MenuAnchorPosition::kBubbleTopRight, source_type);
  }

  ui::SimpleMenuModel menu_model_;
  views::MenuModelAdapter menu_adapter_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

//------------------------------------------------------------------------------
// GlanceablesChipButton:
GlanceablesChipButton::GlanceablesChipButton()
    : removal_chip_menu_controller_(
          std::make_unique<RemovalChipMenuController>(this)) {
  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kBorderInsetsWithoutActionButton, kBetweenChildSpacing,
      /*collapse_margins_spacing=*/true);
  box_layout_ = box_layout.get();
  box_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  raw_ptr<views::BoxLayoutView> titles_container = nullptr;

  // Build up the chip's contents.
  views::Builder<views::Button>(this)
      .SetLayoutManager(std::move(box_layout))
      .SetBorder(std::make_unique<views::HighlightBorder>(
          kRoundedCornerRadius,
          views::HighlightBorder::Type::kHighlightBorderNoShadow))
      .SetBackground(views::CreateThemedRoundedRectBackground(
          kBackgroundColorId, kRoundedCornerRadius))
      .SetPreferredSize(kChipSize)
      // TODO(zxdan): verbalize all the contents in following changes.
      .SetAccessibleName(u"Glanceables Chip")
      .AddChildren(
          // Icon.
          views::Builder<views::ImageView>().CopyAddressTo(&icon_).SetProperty(
              views::kMarginsKey, kIconMargins),
          // Title and subtitle.
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&titles_container)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetBetweenChildSpacing(kTitleSpacing)
              .AddChildren(views::Builder<views::Label>()
                               .CopyAddressTo(&title_)
                               .SetAutoColorReadabilityEnabled(false)
                               .SetEnabledColorId(kTitleColorId)
                               .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                           views::Builder<views::Label>()
                               .CopyAddressTo(&subtitle_)
                               .SetAutoColorReadabilityEnabled(false)
                               .SetEnabledColorId(kSubtitleColorId)
                               .SetHorizontalAlignment(gfx::ALIGN_LEFT)))
      .BuildChildren();

  // Make the titles fit in the free space.
  box_layout_->SetFlexForView(titles_container, /*flex=*/1);

  // Stylize the titles.
  auto* typography_provider = TypographyProvider::Get();
  typography_provider->StyleLabel(kTitleFont, *title_);
  typography_provider->StyleLabel(kSubtitleFont, *subtitle_);

  // Add removal chip panel.
  set_context_menu_controller(removal_chip_menu_controller_.get());

  // Install and stylize the focus ring.
  StyleUtil::InstallRoundedCornerHighlightPathGenerator(
      this, gfx::RoundedCornersF(kRoundedCornerRadius));
  StyleUtil::SetUpFocusRingForView(this);
}

GlanceablesChipButton::~GlanceablesChipButton() = default;

void GlanceablesChipButton::SetIconImage(const ui::ImageModel& icon_image) {
  icon_->SetImage(icon_image);
}

void GlanceablesChipButton::SetTitleText(const std::u16string& title) {
  title_->SetText(title);
}

void GlanceablesChipButton::SetSubtitleText(const std::u16string& subtitle) {
  subtitle_->SetText(subtitle);
}

void GlanceablesChipButton::SetActionButton(
    const std::u16string& label,
    views::Button::PressedCallback action) {
  CHECK(!action_button_);
  box_layout_->set_inside_border_insets(kBorderInsetsWithActionButton);
  action_button_ = AddChildView(std::make_unique<PillButton>(
      std::move(action), label, PillButton::Type::kPrimaryWithoutIcon));
}

void GlanceablesChipButton::SetDelegate(Delegate* delegate) {
  CHECK(!delegate_);
  delegate_ = delegate;
}

void GlanceablesChipButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    // Show removal chip panel.
    gfx::Point screen_location(event->location());
    views::View::ConvertPointToScreen(this, &screen_location);
    ShowContextMenu(screen_location, ui::MENU_SOURCE_TOUCH);
    event->SetHandled();
  }
}

void GlanceablesChipButton::ExecuteCommand(int command_id, int event_flags) {
  // Remove the chip when the option is selected in the removal panel.
  OnRemoveComponentPressed();
}

void GlanceablesChipButton::OnRemoveComponentPressed() {
  if (delegate_) {
    delegate_->RemoveChip(this);
  }
}

BEGIN_METADATA(GlanceablesChipButton, views::Button)
END_METADATA

}  // namespace ash
