// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_button.h"

#include "ash/birch/birch_item.h"
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
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// The color and layout parameters of the chip.
constexpr gfx::Insets kInteriorMarginsNoAddon =
    gfx::Insets::TLBR(12, 0, 12, 20);
constexpr gfx::Insets kInteriorMarginsWithAddon = gfx::Insets::VH(12, 0);
constexpr int kRoundedCornerRadius = 12;
constexpr ui::ColorId kBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBaseOpaque;

// The layout parameters of icon.
constexpr gfx::Size kItemIconSize = gfx::Size(20, 20);
constexpr gfx::Insets kIconMargins = gfx::Insets::VH(0, 12);

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
// BirchChipButton::RemovalChipMenuController:
// The removal chip panel which contains one option to remove the chip.
class BirchChipButton::RemovalChipMenuController
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
// BirchChipButton:
BirchChipButton::BirchChipButton()
    : removal_chip_menu_controller_(
          std::make_unique<RemovalChipMenuController>(this)) {
  auto flex_layout = std::make_unique<views::FlexLayout>();
  flex_layout_ = flex_layout.get();
  flex_layout_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kInteriorMarginsNoAddon);

  raw_ptr<views::BoxLayoutView> titles_container = nullptr;

  // Build up the chip's contents.
  views::Builder<views::Button>(this)
      .SetLayoutManager(std::move(flex_layout))
      .SetBorder(std::make_unique<views::HighlightBorder>(
          kRoundedCornerRadius,
          views::HighlightBorder::Type::kHighlightBorderNoShadow))
      .SetBackground(views::CreateThemedRoundedRectBackground(
          kBackgroundColorId, kRoundedCornerRadius))
      // TODO(zxdan): verbalize all the contents in following changes.
      .SetAccessibleName(u"Birch Chip")
      .AddChildren(
          // Icon.
          views::Builder<views::ImageView>()
              .CopyAddressTo(&icon_)
              .SetProperty(views::kMarginsKey, kIconMargins)
              .SetImageSize(kItemIconSize),
          // Title and subtitle.
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&titles_container)
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded))
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

BirchChipButton::~BirchChipButton() = default;

void BirchChipButton::Init(BirchItem* item) {
  item_ = item;

  title_->SetText(item_->title());
  subtitle_->SetText(item_->subtitle());

  SetCallback(
      base::BindRepeating(&BirchItem::PerformAction, base::Unretained(item_)));
  if (item_->secondary_action().has_value()) {
    auto* button = SetAddon(std::make_unique<PillButton>(
        base::BindRepeating(&BirchItem::PerformSecondaryAction,
                            base::Unretained(item_)),
        *item_->secondary_action(), PillButton::Type::kPrimaryWithoutIcon));
    button->SetProperty(views::kMarginsKey, gfx::Insets::VH(0, 16));
  }
  item_->LoadIcon(base::BindOnce(&BirchChipButton::SetIconImage,
                                 weak_factory_.GetWeakPtr()));
}

void BirchChipButton::SetDelegate(Delegate* delegate) {
  CHECK(!delegate_);
  delegate_ = delegate;
}

void BirchChipButton::SetIconImage(const ui::ImageModel& icon_image) {
  icon_->SetImage(icon_image);
}

void BirchChipButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
    // Show removal chip panel.
    gfx::Point screen_location(event->location());
    views::View::ConvertPointToScreen(this, &screen_location);
    ShowContextMenu(screen_location, ui::MENU_SOURCE_TOUCH);
    event->SetHandled();
  }
}

void BirchChipButton::ExecuteCommand(int command_id, int event_flags) {
  // Remove the chip when the option is selected in the removal panel.
  OnRemoveComponentPressed();
}

void BirchChipButton::SetAddonInternal(
    std::unique_ptr<views::View> addon_view) {
  if (addon_view_) {
    RemoveChildViewT(addon_view_);
  } else {
    flex_layout_->SetInteriorMargin(kInteriorMarginsWithAddon);
  }
  addon_view_ = AddChildView(std::move(addon_view));
}

void BirchChipButton::OnRemoveComponentPressed() {
  if (delegate_) {
    delegate_->RemoveChip(this);
  }
}

BEGIN_METADATA(BirchChipButton)
END_METADATA

}  // namespace ash
