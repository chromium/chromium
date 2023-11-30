// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

using views::FlexLayout;
using views::FlexLayoutView;
using views::InkDropHost;

namespace ash {

namespace {

// Tile constants
constexpr int kIconSize = 20;
constexpr int kDefaultCornerRadius = 16;
constexpr float kFocusRingPadding = 3.0f;

// Primary tile constants
constexpr gfx::Size kDefaultSize(180, kFeatureTileHeight);
constexpr gfx::Size kIconButtonSize(36, 52);
constexpr int kIconButtonCornerRadius = 12;
constexpr gfx::Insets kIconButtonMargins = gfx::Insets::VH(6, 6);
constexpr gfx::Insets kDrillInArrowMargins = gfx::Insets::TLBR(0, 4, 0, 10);
constexpr gfx::Insets kTitleContainerWithoutDiveInButtonMargins =
    gfx::Insets::TLBR(0, 0, 0, 10);
constexpr gfx::Insets kTitleContainerWithDiveInButtonMargins = gfx::Insets();

// Compact tile constants
constexpr int kCompactWidth = 86;
constexpr int kCompactTitleLineHeight = 14;
constexpr gfx::Size kCompactSize(kCompactWidth, kFeatureTileHeight);
constexpr gfx::Size kCompactIconButtonSize(kIconSize, kIconSize);
constexpr gfx::Insets kCompactIconButtonMargins =
    gfx::Insets::TLBR(6, 22, 4, 22);
constexpr gfx::Size kCompactOneRowTitleLabelSize(kCompactWidth - 24,
                                                 kCompactTitleLineHeight);
constexpr gfx::Size kCompactTwoRowTitleLabelSize(kCompactWidth - 24,
                                                 kCompactTitleLineHeight * 2);
constexpr gfx::Insets kCompactTitlesContainerMargins =
    gfx::Insets::TLBR(0, 12, 6, 12);

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

FeatureTile::FeatureTile(PressedCallback callback,
                         bool is_togglable,
                         TileType type)
    : Button(std::move(callback)),
      corner_radius_(kDefaultCornerRadius),
      is_togglable_(is_togglable),
      type_(type) {
  // Set up ink drop on click. The corner radius must match the button
  // background corner radius, see UpdateColors().
  // TODO(jamescook): Consider adding support for highlight-path-based
  // backgrounds so we don't have to match the shape manually. For example, add
  // something like CreateThemedHighlightPathBackground() to
  // ui/views/background.h.
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                corner_radius_);
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

  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->SetOrientation(
      is_compact ? views::BoxLayout::Orientation::kVertical
                 : views::BoxLayout::Orientation::kHorizontal);

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);

  SetPreferredSize(is_compact ? kCompactSize : kDefaultSize);

  icon_button_ = AddChildView(std::make_unique<views::ImageButton>());
  icon_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  icon_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  icon_button_->SetPreferredSize(is_compact ? kCompactIconButtonSize
                                            : kIconButtonSize);
  icon_button_->SetProperty(views::kMarginsKey, is_compact
                                                    ? kCompactIconButtonMargins
                                                    : kIconButtonMargins);
  // By default the icon button is not separately clickable.
  icon_button_->SetEnabled(false);
  icon_button_->SetCanProcessEventsWithinSubtree(false);

  title_container_ = AddChildView(std::make_unique<FlexLayoutView>());
  title_container_->SetCanProcessEventsWithinSubtree(false);
  title_container_->SetOrientation(views::LayoutOrientation::kVertical);
  title_container_->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  title_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  label_ = title_container_->AddChildView(std::make_unique<views::Label>());
  label_->SetAutoColorReadabilityEnabled(false);

  sub_label_ = title_container_->AddChildView(std::make_unique<views::Label>());
  sub_label_->SetHorizontalAlignment(is_compact ? gfx::ALIGN_CENTER
                                                : gfx::ALIGN_LEFT);
  sub_label_->SetAutoColorReadabilityEnabled(false);

  if (is_compact) {
    title_container_->SetProperty(views::kMarginsKey,
                                  kCompactTitlesContainerMargins);
    label_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
    label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);

    // By default, assume compact tiles will not support sub-labels.
    SetCompactTileLabelPreferences(/*add_sub_label=*/false);

    // Compact labels use kCrosAnnotation2 with a shorter custom line height.
    const auto font_list = TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosAnnotation2);
    label_->SetFontList(font_list);
    label_->SetLineHeight(kCompactTitleLineHeight);
    sub_label_->SetFontList(font_list);
    sub_label_->SetLineHeight(kCompactTitleLineHeight);
    sub_label_->SetVisible(false);
  } else {
    // `title_container_` will take all the remaining space of the tile.
    layout_manager->SetFlexForView(title_container_, 1);
    title_container_->SetProperty(views::kMarginsKey,
                                  kTitleContainerWithoutDiveInButtonMargins);
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label_);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                          *sub_label_);
  }
}

void FeatureTile::SetIconClickable(bool clickable) {
  CHECK_EQ(type_, TileType::kPrimary);
  is_icon_clickable_ = clickable;
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

  title_container_->SetProperty(views::kMarginsKey,
                                kTitleContainerWithDiveInButtonMargins);
  drill_in_arrow_ = AddChildView(std::make_unique<views::ImageView>());
  // The icon is set in UpdateDrillArrowColor().
  drill_in_arrow_->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  drill_in_arrow_->SetProperty(views::kMarginsKey, kDrillInArrowMargins);
  // Allow hover events to fall through to show tooltips from the main view.
  drill_in_arrow_->SetCanProcessEventsWithinSubtree(false);
  drill_in_arrow_->SetFlipCanvasOnPaintForRTLUI(true);
  UpdateDrillInArrowColor();
}

void FeatureTile::UpdateColors() {
  ui::ColorId background_color;
  ui::ColorId foreground_color;
  ui::ColorId foreground_optional_color;

  if (GetEnabled()) {
    background_color =
        toggled_
            ? background_toggled_color_.value_or(
                  cros_tokens::kCrosSysSystemPrimaryContainer)
            : background_color_.value_or(cros_tokens::kCrosSysSystemOnBase);
    foreground_color =
        toggled_ ? foreground_toggled_color_.value_or(
                       cros_tokens::kCrosSysSystemOnPrimaryContainer)
                 : foreground_color_.value_or(cros_tokens::kCrosSysOnSurface);
    foreground_optional_color =
        toggled_ ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                 : cros_tokens::kCrosSysOnSurfaceVariant;
  } else {
    background_color = cros_tokens::kCrosSysDisabledContainer;
    foreground_color = cros_tokens::kCrosSysDisabled;
    foreground_optional_color = cros_tokens::kCrosSysDisabled;
  }

  SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                         corner_radius_));
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
  if (!is_togglable_ || toggled_ == toggled) {
    return;
  }

  toggled_ = toggled;
  UpdateColors();
  views::InkDrop::Get(this)->GetInkDrop()->SnapToHidden();
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

void FeatureTile::SetBackgroundColorId(ui::ColorId background_color_id) {
  if (background_color_ == background_color_id) {
    return;
  }
  background_color_ = background_color_id;
  if (!toggled_) {
    UpdateColors();
  }
}
void FeatureTile::SetBackgroundToggledColorId(
    ui::ColorId background_toggled_color_id) {
  if (background_toggled_color_ == background_toggled_color_id) {
    return;
  }
  background_toggled_color_ = background_toggled_color_id;
  if (toggled_) {
    UpdateColors();
  }
}

void FeatureTile::SetButtonCornerRadius(const int radius) {
  corner_radius_ = radius;
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                corner_radius_);
  UpdateColors();
}

void FeatureTile::SetForegroundColorId(ui::ColorId foreground_color_id) {
  if (foreground_color_ == foreground_color_id) {
    return;
  }
  foreground_color_ = foreground_color_id;
  if (!toggled_) {
    UpdateColors();
  }
}

void FeatureTile::SetForegroundToggledColorId(
    ui::ColorId foreground_toggled_color_id) {
  if (foreground_toggled_color_ == foreground_toggled_color_id) {
    return;
  }
  foreground_toggled_color_ = foreground_toggled_color_id;
  if (toggled_) {
    UpdateColors();
  }
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

int FeatureTile::GetSubLabelMaxWidth() const {
  return title_container_->size().width();
}

void FeatureTile::SetSubLabel(const std::u16string& sub_label) {
  DCHECK(!sub_label.empty())
      << "Attempting to set an empty sub-label. Did you mean to call "
         "SubLabelVisibility(false) instead?";
  sub_label_->SetText(sub_label);
}

void FeatureTile::SetSubLabelVisibility(bool visible) {
  const bool is_compact = type_ == TileType::kCompact;
  DCHECK(!(is_compact && visible && sub_label_->GetText().empty()))
      << "Attempting to make the compact tile's sub-label visible when it "
         "wasn't set.";
  sub_label_->SetVisible(visible);
  if (is_compact) {
    // When updating a compact tile's `sub_label_` visibility, `label_` needs to
    // also be changed to make room for the sub-label. If making a sub-label
    // visible, the primary label and sub-label have one line each to display
    // text. If disabling sub-label visibility, reset `label_` to allow its text
    // to display on two lines.
    SetCompactTileLabelPreferences(/*add_sub_label=*/visible);
  }
}

void FeatureTile::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);
  // If the icon is clickable then the main feature tile usually takes the user
  // to a detailed page (like Network or Bluetooth). Those tiles act more like a
  // regular button than a toggle button.
  if (is_togglable_ && !is_icon_clickable_) {
    node_data->role = ax::mojom::Role::kToggleButton;
    node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  } else {
    node_data->role = ax::mojom::Role::kButton;
  }
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
  return toggled_ ? foreground_toggled_color_.value_or(
                        cros_tokens::kCrosSysSystemOnPrimaryContainer)
                  : foreground_color_.value_or(cros_tokens::kCrosSysOnSurface);
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

void FeatureTile::SetCompactTileLabelPreferences(bool has_sub_label) {
  label_->SetPreferredSize(has_sub_label ? kCompactOneRowTitleLabelSize
                                         : kCompactTwoRowTitleLabelSize);
  label_->SetMultiLine(!has_sub_label);
  // Elide after 2 lines if there's no sub-label. Otherwise, 1 line.
  label_->SetMaxLines(has_sub_label ? 1 : 2);
}

BEGIN_METADATA(FeatureTile, views::Button)
END_METADATA

}  // namespace ash
