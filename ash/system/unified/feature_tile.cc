// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/constants/chromeos_features.h"
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
constexpr int kFocusRingPadding = 2;

// Primary tile constants
constexpr int kPrimarySubtitleLineHeight = 18;
constexpr gfx::Size kDefaultSize(180, kFeatureTileHeight);
constexpr gfx::Size kIconContainerSize(48, kFeatureTileHeight);
constexpr gfx::Size kTitlesContainerSize(92, kFeatureTileHeight);
constexpr gfx::Size kDrillContainerSize(40, kFeatureTileHeight);

// Compact tile constants
constexpr int kCompactWidth = 86;
constexpr int kCompactTitleLineHeight = 14;
constexpr gfx::Size kCompactSize(kCompactWidth, kFeatureTileHeight);
constexpr gfx::Size kCompactIconContainerSize(kCompactWidth, 30);
constexpr gfx::Size kCompactTitleContainerSize(kCompactWidth, 34);
constexpr gfx::Size kCompactTitleLabelSize(kCompactWidth - 32,
                                           kCompactTitleLineHeight * 2);
constexpr gfx::Insets kCompactIconContainerInteriorMargin(
    gfx::Insets::TLBR(0, 0, 4, 0));

}  // namespace

FeatureTile::FeatureTile(base::RepeatingCallback<void()> callback,
                         bool is_togglable,
                         TileType type)
    : Button(callback), is_togglable_(is_togglable), type_(type) {
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(-kFocusRingPadding), kButtonRadius + kFocusRingPadding);
  CreateChildViews();
  UpdateColors();

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      [](FeatureTile* feature_tile) {
        feature_tile->UpdateColors();
        if (!feature_tile->drill_in_button_) {
          return;
        }
        feature_tile->drill_in_button_->SetEnabled(feature_tile->GetEnabled());
        feature_tile->drill_in_arrow_->SetEnabled(feature_tile->GetEnabled());
      },
      base::Unretained(this)));
}

FeatureTile::~FeatureTile() = default;

void FeatureTile::CreateChildViews() {
  const bool is_compact = type_ == TileType::kCompact;

  auto* layout_manager = SetLayoutManager(std::make_unique<FlexLayout>());
  layout_manager->SetOrientation(is_compact
                                     ? views::LayoutOrientation::kVertical
                                     : views::LayoutOrientation::kHorizontal);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  // Since the focus ring doesn't set a LayoutManager it won't get drawn unless
  // excluded by the tile's LayoutManager.
  // TODO(crbug/1385946): Modify LayoutManagerBase and FocusRing to always
  // exclude focus ring from the layout.
  layout_manager->SetChildViewIgnoredByLayout(focus_ring, true);

  SetPreferredSize(is_compact ? kCompactSize : kDefaultSize);

  auto* icon_container = AddChildView(std::make_unique<FlexLayoutView>());
  icon_container->SetCanProcessEventsWithinSubtree(false);
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
  title_container->SetCanProcessEventsWithinSubtree(false);
  title_container->SetOrientation(is_compact
                                      ? views::LayoutOrientation::kHorizontal
                                      : views::LayoutOrientation::kVertical);
  title_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  title_container->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  title_container->SetPreferredSize(is_compact ? kCompactTitleContainerSize
                                               : kTitlesContainerSize);

  label_ = title_container->AddChildView(std::make_unique<views::Label>());
  label_->SetAutoColorReadabilityEnabled(false);

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
    sub_label_ =
        title_container->AddChildView(std::make_unique<views::Label>());
    sub_label_->SetAutoColorReadabilityEnabled(false);
    // TODO(b/252873172): update FontList.
    sub_label_->SetFontList(views::Label::GetDefaultFontList().Derive(
        -1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
    sub_label_->SetLineHeight(kPrimarySubtitleLineHeight);
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

  auto drill_in_arrow = std::make_unique<IconButton>(
      callback,
      is_togglable_ ? IconButton::Type::kXSmall
                    : IconButton::Type::kXSmallFloating,
      &kQuickSettingsRightArrowIcon, tooltip_text,
      /*togglable=*/is_togglable_,
      /*has_border=*/true);

  // Focus behavior is set on this view, but we let its parent view
  // `drill_in_button_` handle the button events.
  drill_in_arrow->SetCanProcessEventsWithinSubtree(false);

  // Only buttons with Toggle + Drill-in behavior can focus the drill-in arrow
  // and process drill-in button events.
  if (!is_togglable_) {
    drill_in_button->SetCanProcessEventsWithinSubtree(false);
    drill_in_arrow->SetFocusBehavior(FocusBehavior::NEVER);
  }

  drill_in_button_ = AddChildView(std::move(drill_in_button));
  drill_in_arrow_ = drill_in_button_->AddChildView(std::move(drill_in_arrow));

  drill_in_arrow_->SetIconColorId(cros_tokens::kCrosSysSecondary);
  drill_in_arrow_->SetIconToggledColorId(
      cros_tokens::kCrosSysSystemOnPrimaryContainer);

  // TODO(b/262615213): Delete when Jelly launches.
  if (!chromeos::features::IsJellyEnabled()) {
    drill_in_arrow_->SetBackgroundColorId(
        kColorAshControlBackgroundColorInactive);
    drill_in_arrow_->SetBackgroundToggledColorId(
        static_cast<ui::ColorId>(kColorAshTileSmallCircle));
    return;
  }
  drill_in_arrow_->SetBackgroundColorId(cros_tokens::kCrosSysHoverOnSubtle);
  drill_in_arrow_->SetBackgroundToggledColorId(
      cros_tokens::kCrosSysHighlightShape);
}

void FeatureTile::UpdateColors() {
  ui::ColorId background_color;
  ui::ColorId foreground_color;
  ui::ColorId foreground_optional_color;

  if (GetEnabled()) {
    background_color = toggled_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                                : cros_tokens::kCrosSysSystemOnBase;
    foreground_color = toggled_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                                : cros_tokens::kCrosSysOnSurface;
    foreground_optional_color =
        toggled_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                 : cros_tokens::kCrosSysSecondary;
  } else {
    background_color = cros_tokens::kCrosSysDisabledContainer;
    foreground_color = cros_tokens::kCrosSysDisabled;
    foreground_optional_color = cros_tokens::kCrosSysDisabled;
  }

  SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                         kButtonRadius));
  icon_->SetImage(ui::ImageModel::FromVectorIcon(*vector_icon_,
                                                 foreground_color, kIconSize));
  label_->SetEnabledColorId(foreground_color);
  if (sub_label_) {
    sub_label_->SetEnabledColorId(foreground_optional_color);
  }
  if (drill_in_arrow_) {
    UpdateDrillInButtonFocusRingColor();
  }
}

void FeatureTile::SetToggled(bool toggled) {
  if (!is_togglable_ || toggled_ == toggled)
    return;

  toggled_ = toggled;
  if (drill_in_arrow_) {
    drill_in_arrow_->SetToggled(toggled_);
  }

  UpdateColors();
}

bool FeatureTile::IsToggled() const {
  return toggled_;
}

void FeatureTile::SetVectorIcon(const gfx::VectorIcon& icon) {
  vector_icon_ = &icon;
  ui::ColorId color_id;
  if (GetEnabled()) {
    color_id = toggled_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                        : cros_tokens::kCrosSysOnSurface;
  } else {
    color_id = cros_tokens::kCrosSysDisabled;
  }
  icon_->SetImage(ui::ImageModel::FromVectorIcon(icon, color_id, kIconSize));
}

void FeatureTile::SetImage(gfx::ImageSkia image) {
  icon_->SetImage(ui::ImageModel::FromImageSkia(image));
}

void FeatureTile::SetLabel(const std::u16string& label) {
  label_->SetText(label);
}

void FeatureTile::SetSubLabel(const std::u16string& sub_label) {
  sub_label_->SetText(sub_label);
}

void FeatureTile::SetSubLabelVisibility(bool visible) {
  // Only primary tiles have a sub-label.
  DCHECK(sub_label_);
  sub_label_->SetVisible(visible);
}

void FeatureTile::SetDrillInButtonTooltipText(const std::u16string& text) {
  // Only primary tiles have a drill-in button.
  DCHECK(drill_in_button_);
  drill_in_button_->SetTooltipText(text);
}

void FeatureTile::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (drill_in_arrow_) {
    UpdateDrillInButtonFocusRingColor();
  }
}

void FeatureTile::UpdateDrillInButtonFocusRingColor() {
  views::FocusRing::Get(drill_in_arrow_)
      ->SetColorId(toggled_ ? cros_tokens::kCrosSysFocusRingOnPrimaryContainer
                            : cros_tokens::kCrosSysFocusRing);
}

BEGIN_METADATA(FeatureTile, views::Button)
END_METADATA

}  // namespace ash
