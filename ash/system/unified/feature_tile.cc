// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

using views::FlexLayout;
using views::FlexLayoutView;

namespace ash {

namespace {

// Tile constants
constexpr int kIconSize = 20;
constexpr int kButtonRadius = 16;

// Primary tile constants
constexpr int kPrimarySubtitleLineHeight = 18;
constexpr gfx::Size kDefaultSize(200, kFeatureTileHeight);
constexpr gfx::Size kIconContainerSize(48, kFeatureTileHeight);
constexpr gfx::Size kTitlesContainerSize(112, kFeatureTileHeight);
constexpr gfx::Size kDrillContainerSize(40, kFeatureTileHeight);

// Compact tile constants
constexpr int kCompactWidth = 96;
constexpr int kCompactTitleLineHeight = 14;
constexpr gfx::Size kCompactSize(kCompactWidth, kFeatureTileHeight);
constexpr gfx::Size kCompactIconContainerSize(kCompactWidth, 30);
constexpr gfx::Size kCompactTitleContainerSize(kCompactWidth, 34);
constexpr gfx::Size kCompactTitleLabelSize(kCompactWidth - 32,
                                           kCompactTitleLineHeight * 2);
constexpr gfx::Insets kCompactIconContainerInteriorMargin(
    gfx::Insets::TLBR(0, 0, 4, 0));

}  // namespace

// Constructor for prototype tiles without a callback.
// TODO(b/252871301): Remove when having implemented each feature tile.
FeatureTile::FeatureTile(TileType type)
    : Button(PressedCallback()), type_(type) {
  UpdateColors();
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kButtonRadius);
  SetAccessibleName(u"Placeholder Tile");

  CreateChildViews();
  if (type == TileType::kPrimary) {
    label_->SetText(u"Title");
    sub_label_->SetText(u"Subtitle");
  } else {
    label_->SetText(u"Two line\ntitle");
  }
  SetVectorIcon(vector_icons::kDogfoodIcon);
}

FeatureTile::FeatureTile(base::RepeatingCallback<void()> callback,
                         bool is_togglable,
                         TileType type)
    : Button(callback), is_togglable_(is_togglable), type_(type) {
  UpdateColors();
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kButtonRadius);
  CreateChildViews();
}

void FeatureTile::CreateChildViews() {
  const bool is_compact = type_ == TileType::kCompact;

  auto* layout_manager = SetLayoutManager(std::make_unique<FlexLayout>());
  layout_manager->SetOrientation(is_compact
                                     ? views::LayoutOrientation::kVertical
                                     : views::LayoutOrientation::kHorizontal);
  // Since the focus ring doesn't set a LayoutManager it won't get drawn unless
  // excluded by the tile's LayoutManager.
  // TODO(crbug/1385946): Modify LayoutManagerBase and FocusRing to always
  // exclude focus ring from the layout.
  layout_manager->SetChildViewIgnoredByLayout(views::FocusRing::Get(this),
                                              true);

  SetPreferredSize(is_compact ? kCompactSize : kDefaultSize);

  auto* icon_container = AddChildView(std::make_unique<FlexLayoutView>());
  icon_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container->SetCrossAxisAlignment(is_compact
                                            ? views::LayoutAlignment::kEnd
                                            : views::LayoutAlignment::kCenter);
  icon_container->SetPreferredSize(is_compact ? kCompactIconContainerSize
                                              : kIconContainerSize);
  if (is_compact)
    icon_container->SetInteriorMargin(kCompactIconContainerInteriorMargin);
  icon_ = icon_container->AddChildView(std::make_unique<views::ImageView>());

  auto* title_container = AddChildView(std::make_unique<FlexLayoutView>());
  title_container->SetOrientation(is_compact
                                      ? views::LayoutOrientation::kHorizontal
                                      : views::LayoutOrientation::kVertical);
  title_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  title_container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  title_container->SetPreferredSize(is_compact ? kCompactTitleContainerSize
                                               : kTitlesContainerSize);

  label_ = AddChildView(std::make_unique<views::Label>());
  title_container->AddChildView(label_);

  if (is_compact) {
    label_->SetPreferredSize(kCompactTitleLabelSize);
    // TODO(b/259459827): verify multi-line text is rendering correctly, not
    // clipping and center aligned.
    label_->SetMultiLine(true);
    label_->SetLineHeight(kCompactTitleLineHeight);
    // TODO(b/252873172): update FontList.
    label_->SetFontList(views::Label::GetDefaultFontList().Derive(
        -1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  } else {
    sub_label_ = AddChildView(std::make_unique<views::Label>());
    // TODO(b/252873172): update FontList.
    sub_label_->SetFontList(views::Label::GetDefaultFontList().Derive(
        -1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
    sub_label_->SetLineHeight(kPrimarySubtitleLineHeight);
    title_container->AddChildView(sub_label_);
  }
}

void FeatureTile::CreateDrillInButton(base::RepeatingCallback<void()> callback,
                                      const std::u16string& tooltip_text) {
  DCHECK_EQ(type_, TileType::kPrimary);

  auto drill_in_button = std::make_unique<views::LabelButton>(callback);
  drill_in_button->SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  drill_in_button->SetPreferredSize(kDrillContainerSize);
  drill_in_button->SetFocusBehavior(FocusBehavior::NEVER);
  drill_in_button->SetTooltipText(tooltip_text);

  auto drill_in_arrow =
      std::make_unique<IconButton>(callback, IconButton::Type::kXSmall,
                                   &kQuickSettingsRightArrowIcon, tooltip_text,
                                   /*togglable=*/false,
                                   /*has_border=*/false);

  // Focus behavior is set on this view, but we let its parent view
  // `drill_in_button_` handle the button events.
  drill_in_arrow->SetCanProcessEventsWithinSubtree(false);

  // Only buttons with Toggle + Drill-in behavior can focus the drill-in arrow.
  if (!is_togglable_)
    drill_in_arrow->SetFocusBehavior(FocusBehavior::NEVER);

  drill_in_button_ = AddChildView(std::move(drill_in_button));
  drill_in_button_->AddChildView(std::move(drill_in_arrow));
}

void FeatureTile::UpdateColors() {
  ui::ColorId background_color_id =
      toggled_ ? cros_tokens::kCrosSysSystemPrimaryContainer
               : cros_tokens::kCrosSysSystemOnBase;

  SetBackground(views::CreateThemedRoundedRectBackground(background_color_id,
                                                         kButtonRadius));
}

void FeatureTile::SetToggled(bool toggled) {
  if (!is_togglable_ || toggled_ == toggled)
    return;

  toggled_ = toggled;

  UpdateColors();
}

bool FeatureTile::IsToggled() const {
  return toggled_;
}

void FeatureTile::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      icon, cros_tokens::kCrosSysOnSurface, kIconSize));
}

void FeatureTile::SetLabel(const std::u16string& label) {
  label_->SetText(label);
}

void FeatureTile::SetSubLabel(const std::u16string& sub_label) {
  sub_label_->SetText(sub_label);
}

void FeatureTile::SetSubLabelVisibility(bool visible) {
  sub_label_->SetVisible(visible);
}

void FeatureTile::SetDrillInButtonTooltipText(const std::u16string& text) {
  // Only primary tiles have a drill-in button.
  DCHECK(drill_in_button_);
  drill_in_button_->SetTooltipText(text);
}

BEGIN_METADATA(FeatureTile, views::Button)
END_METADATA

}  // namespace ash
