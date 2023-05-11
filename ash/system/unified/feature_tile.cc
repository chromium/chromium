// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"

using views::FlexLayout;
using views::FlexLayoutView;
using views::InkDropHost;

namespace ash {

namespace {

// Tile constants
constexpr int kIconSize = 20;
constexpr int kButtonRadius = 16;
constexpr float kFocusRingPadding = 3.0f;

// Primary tile constants
constexpr int kPrimarySubtitleLineHeight = 18;
constexpr gfx::Size kDefaultSize(180, kFeatureTileHeight);
constexpr gfx::Size kIconContainerSize(48, kFeatureTileHeight);
constexpr gfx::Size kIconButtonSize(36, 52);
constexpr int kIconButtonCornerRadius = 12;
constexpr gfx::Size kTitlesContainerSize(92, kFeatureTileHeight);
constexpr gfx::Size kDrillContainerSize(40, kFeatureTileHeight);

// Compact tile constants
constexpr int kCompactWidth = 86;
constexpr int kCompactTitleLineHeight = 14;
constexpr gfx::Size kCompactSize(kCompactWidth, kFeatureTileHeight);
constexpr gfx::Size kCompactIconContainerSize(kCompactWidth, 30);
constexpr gfx::Size kCompactIconButtonSize(kIconSize, kIconSize);
constexpr gfx::Size kCompactTitleContainerSize(kCompactWidth, 34);
constexpr gfx::Size kCompactTitleLabelSize(kCompactWidth - 32,
                                           kCompactTitleLineHeight * 2);
constexpr gfx::Insets kCompactIconContainerInteriorMargin(
    gfx::Insets::TLBR(0, 0, 4, 0));

// Creates an ink drop hover highlight for `host` with `color_id`.
std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight(
    views::View* host,
    ui::ColorId color_id) {
  SkColor color = host->GetColorProvider()->GetColor(color_id);
  auto highlight = std::make_unique<views::InkDropHighlight>(
      gfx::SizeF(host->size()), color);
  // The color has the opacity baked in.
  highlight->set_visible_opacity(1.0f);
  return highlight;
}

}  // namespace

FeatureTile::FeatureTile(base::RepeatingCallback<void()> callback,
                         bool is_togglable,
                         TileType type)
    : Button(callback), is_togglable_(is_togglable), type_(type) {
  // Set up ink drop on click. The corner radius must match the button
  // background corner radius, see UpdateColors().
  // TODO(jamescook): Consider adding support for highlight-path-based
  // backgrounds so we don't have to match the shape manually. For example, add
  // something like CreateThemedHighlightPathBackground() to
  // ui/views/background.h.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kButtonRadius);
  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetMode(InkDropHost::InkDropMode::ON);
  ink_drop->GetInkDrop()->SetShowHighlightOnHover(false);
  ink_drop->SetVisibleOpacity(1.0f);  // The colors already contain opacity.

  // The focus ring appears slightly outside the tile bounds.
  views::FocusRing::Get(this)->SetHaloInset(-kFocusRingPadding);

  CreateChildViews();
  UpdateColors();

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      [](FeatureTile* feature_tile) {
        feature_tile->UpdateColors();
        if (feature_tile->is_icon_clickable_) {
          feature_tile->icon_button_->SetEnabled(feature_tile->GetEnabled());
        }
      },
      base::Unretained(this)));
}

FeatureTile::~FeatureTile() {
  // Remove the InkDrop explicitly so FeatureTile::RemoveLayerFromRegions() is
  // called before views::View teardown.
  views::InkDrop::Remove(this);
}

void FeatureTile::CreateChildViews() {
  const bool is_compact = type_ == TileType::kCompact;

  auto* layout_manager = SetLayoutManager(std::make_unique<FlexLayout>());
  layout_manager->SetOrientation(is_compact
                                     ? views::LayoutOrientation::kVertical
                                     : views::LayoutOrientation::kHorizontal);

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  layout_manager->SetChildViewIgnoredByLayout(ink_drop_container_, true);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  // Since the focus ring doesn't set a LayoutManager it won't get drawn unless
  // excluded by the tile's LayoutManager.
  // TODO(crbug/1385946): Modify LayoutManagerBase and FocusRing to always
  // exclude focus ring from the layout.
  layout_manager->SetChildViewIgnoredByLayout(focus_ring, true);

  SetPreferredSize(is_compact ? kCompactSize : kDefaultSize);

  icon_container_ = AddChildView(std::make_unique<FlexLayoutView>());
  icon_container_->SetCanProcessEventsWithinSubtree(false);
  icon_container_->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  icon_container_->SetCrossAxisAlignment(is_compact
                                             ? views::LayoutAlignment::kEnd
                                             : views::LayoutAlignment::kCenter);
  icon_container_->SetPreferredSize(is_compact ? kCompactIconContainerSize
                                               : kIconContainerSize);
  if (is_compact) {
    icon_container_->SetInteriorMargin(kCompactIconContainerInteriorMargin);
  }

  icon_button_ =
      icon_container_->AddChildView(std::make_unique<views::ImageButton>());
  icon_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  icon_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  icon_button_->SetPreferredSize(is_compact ? kCompactIconButtonSize
                                            : kIconButtonSize);
  // By default the icon button is not separately clickable.
  icon_button_->SetEnabled(false);
  icon_button_->SetCanProcessEventsWithinSubtree(false);

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
  label_->SetFontList(ash::TypographyProvider::Get()->ResolveTypographyToken(
      ash::TypographyToken::kCrosButton2));

  if (is_compact) {
    label_->SetPreferredSize(kCompactTitleLabelSize);
    // TODO(b/259459827): verify multi-line text is rendering correctly, not
    // clipping and center aligned.
    label_->SetMultiLine(true);
    label_->SetLineHeight(kCompactTitleLineHeight);
    label_->SetFontList(ash::TypographyProvider::Get()->ResolveTypographyToken(
        ash::TypographyToken::kCrosAnnotation2));
  } else {
    sub_label_ =
        title_container->AddChildView(std::make_unique<views::Label>());
    sub_label_->SetAutoColorReadabilityEnabled(false);
    sub_label_->SetFontList(
        ash::TypographyProvider::Get()->ResolveTypographyToken(
            ash::TypographyToken::kCrosAnnotation1));
    sub_label_->SetLineHeight(kPrimarySubtitleLineHeight);
    if (chromeos::features::IsJellyEnabled()) {
      TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                            *sub_label_);
    }
  }
  if (chromeos::features::IsJellyEnabled()) {
    TypographyProvider::Get()->StyleLabel(
        is_compact ? TypographyToken::kCrosAnnotation2
                   : TypographyToken::kCrosButton2,
        *label_);
  }
}

void FeatureTile::SetIconClickable(bool clickable) {
  CHECK_EQ(type_, TileType::kPrimary);
  is_icon_clickable_ = clickable;
  // Allow `icon_button_` to receive hover and click events. This results in a
  // tiny area inside `icon_container_` near the button's edge that does not
  // have a tooltip, but it's unlikely users will notice.
  icon_container_->SetCanProcessEventsWithinSubtree(clickable);
  icon_button_->SetCanProcessEventsWithinSubtree(clickable);
  icon_button_->SetEnabled(clickable);

  if (clickable) {
    views::InstallRoundRectHighlightPathGenerator(icon_button_, gfx::Insets(),
                                                  kIconButtonCornerRadius);
    UpdateIconButtonFocusRingColor();

    views::InkDrop::Get(icon_button_)->SetMode(InkDropHost::InkDropMode::ON);
    icon_button_->SetHasInkDropActionOnClick(true);
    UpdateIconButtonRippleColors();
  } else {
    views::HighlightPathGenerator::Install(icon_button_, nullptr);
    views::InkDrop::Get(icon_button_)->SetMode(InkDropHost::InkDropMode::OFF);
  }
}

void FeatureTile::SetIconClickCallback(
    base::RepeatingCallback<void()> callback) {
  icon_button_->SetCallback(std::move(callback));
}

void FeatureTile::CreateDecorativeDrillInArrow() {
  CHECK_EQ(type_, TileType::kPrimary);

  drill_in_arrow_ = AddChildView(std::make_unique<views::ImageView>());
  // The icon is set in UpdateDrillArrowColor().
  drill_in_arrow_->SetPreferredSize(kDrillContainerSize);
  // Allow hover events to fall through to show tooltips from the main view.
  drill_in_arrow_->SetCanProcessEventsWithinSubtree(false);
  UpdateDrillInArrowColor();
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
                 : cros_tokens::kCrosSysOnSurfaceVariant;
  } else {
    background_color = cros_tokens::kCrosSysDisabledContainer;
    foreground_color = cros_tokens::kCrosSysDisabled;
    foreground_optional_color = cros_tokens::kCrosSysDisabled;
  }

  SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                         kButtonRadius));
  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetBaseColorId(toggled_
                               ? cros_tokens::kCrosSysRipplePrimary
                               : cros_tokens::kCrosSysRippleNeutralOnSubtle);

  auto icon_image_model = ui::ImageModel::FromVectorIcon(
      *vector_icon_, foreground_color, kIconSize);
  icon_button_->SetImageModel(views::Button::STATE_NORMAL, icon_image_model);
  icon_button_->SetImageModel(views::Button::STATE_DISABLED, icon_image_model);
  if (is_icon_clickable_) {
    UpdateIconButtonRippleColors();
    UpdateIconButtonFocusRingColor();
  }

  label_->SetEnabledColorId(foreground_color);
  if (sub_label_) {
    sub_label_->SetEnabledColorId(foreground_optional_color);
  }
  if (drill_in_arrow_) {
    UpdateDrillInArrowColor();
  }
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
  vector_icon_ = &icon;
  ui::ColorId color_id = GetIconColorId();
  auto image_model = ui::ImageModel::FromVectorIcon(icon, color_id, kIconSize);
  icon_button_->SetImageModel(views::Button::STATE_NORMAL, image_model);
  icon_button_->SetImageModel(views::Button::STATE_DISABLED, image_model);
}

void FeatureTile::SetImage(gfx::ImageSkia image) {
  auto image_model = ui::ImageModel::FromImageSkia(image);
  icon_button_->SetImageModel(views::Button::STATE_NORMAL, image_model);
  icon_button_->SetImageModel(views::Button::STATE_DISABLED, image_model);
}

void FeatureTile::SetIconButtonTooltipText(const std::u16string& tooltip_text) {
  CHECK(is_icon_clickable_);
  icon_button_->SetTooltipText(tooltip_text);
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

void FeatureTile::AddLayerToRegion(ui::Layer* layer,
                                   views::LayerRegion region) {
  // This routes background layers to `ink_drop_container_` instead of `this` to
  // avoid painting effects underneath our background.
  ink_drop_container_->AddLayerToRegion(layer, region);
}

void FeatureTile::RemoveLayerFromRegions(ui::Layer* layer) {
  // This routes background layers to `ink_drop_container_` instead of `this` to
  // avoid painting effects underneath our background.
  ink_drop_container_->RemoveLayerFromRegions(layer);
}

ui::ColorId FeatureTile::GetIconColorId() const {
  if (!GetEnabled()) {
    return cros_tokens::kCrosSysDisabled;
  }
  return toggled_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                  : cros_tokens::kCrosSysOnSurface;
}

void FeatureTile::UpdateIconButtonRippleColors() {
  CHECK(is_icon_clickable_);
  auto* ink_drop = views::InkDrop::Get(icon_button_);
  // Set up the hover highlight.
  ink_drop->SetCreateHighlightCallback(
      base::BindRepeating(&CreateInkDropHighlight, icon_button_,
                          toggled_ ? cros_tokens::kCrosSysHighlightShape
                                   : cros_tokens::kCrosSysHoverOnSubtle));
  // Set up the ripple color.
  ink_drop->SetBaseColorId(toggled_
                               ? cros_tokens::kCrosSysRipplePrimary
                               : cros_tokens::kCrosSysRippleNeutralOnSubtle);
  // The ripple base color includes opacity.
  ink_drop->SetVisibleOpacity(1.0f);

  // Ensure the new color applies even if the hover highlight or ripple is
  // already showing.
  ink_drop->GetInkDrop()->HostViewThemeChanged();
}

void FeatureTile::UpdateIconButtonFocusRingColor() {
  CHECK(is_icon_clickable_);
  views::FocusRing::Get(icon_button_)
      ->SetColorId(toggled_ ? cros_tokens::kCrosSysFocusRingOnPrimaryContainer
                            : cros_tokens::kCrosSysFocusRing);
}

void FeatureTile::UpdateDrillInArrowColor() {
  CHECK(drill_in_arrow_);
  drill_in_arrow_->SetImage(ui::ImageModel::FromVectorIcon(
      kQuickSettingsRightArrowIcon, GetIconColorId()));
}

BEGIN_METADATA(FeatureTile, views::Button)
END_METADATA

}  // namespace ash
