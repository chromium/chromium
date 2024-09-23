// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/feature_tile.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_constants.h"
#include "base/auto_reset.h"
#include "base/strings/string_number_conversions.h"
#include "cc/paint/paint_flags.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
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
constexpr gfx::Size kIconButtonSize(36, 52);
constexpr int kIconButtonCornerRadius = 12;
constexpr gfx::Insets kIconButtonMargins = gfx::Insets::VH(6, 6);
constexpr gfx::Insets kDrillInArrowMargins = gfx::Insets::TLBR(0, 4, 0, 10);
constexpr gfx::Insets kTitleContainerWithoutDiveInButtonMargins =
    gfx::Insets::TLBR(0, 0, 0, 10);
constexpr gfx::Insets kTitleContainerWithDiveInButtonMargins = gfx::Insets();

// Compact tile constants
constexpr int kCompactTitleLineHeight = 14;
constexpr gfx::Size kCompactIconButtonSize(kIconSize, kIconSize);
constexpr gfx::Insets kCompactIconButtonMargins =
    gfx::Insets::TLBR(6, 22, 4, 22);
constexpr gfx::Insets kCompactTitlesContainerMargins =
    gfx::Insets::TLBR(0, 12, 6, 12);

// Download progress constants.
constexpr int kDownloadProgressBorderThickness = 4;
constexpr int kDownloadProgressLeadingEdgeRadius = 2;

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

FeatureTile::ProgressBackground::ProgressBackground(
    const ui::ColorId progress_color_id,
    const ui::ColorId background_color_id)
    : progress_color_id_(progress_color_id),
      background_color_id_(background_color_id) {}

void FeatureTile::ProgressBackground::Paint(gfx::Canvas* canvas,
                                            views::View* view) const {
  FeatureTile* tile = static_cast<FeatureTile*>(view);

  // Start with a simple rounded-rect background as the base. This part is
  // symmetric so the canvas does not need to be flipped for RTL.
  cc::PaintFlags background_flags;
  background_flags.setAntiAlias(true);
  background_flags.setStyle(cc::PaintFlags::kFill_Style);
  background_flags.setColor(
      view->GetColorProvider()->GetColor(background_color_id_));
  canvas->DrawRoundRect(view->GetLocalBounds(), tile->corner_radius_,
                        background_flags);

  // Then draw the progress bar on top. This part is NOT symmetric, so first
  // flip the canvas (if necessary) to handle RTL layouts. Do this using a
  // `gfx::ScopedCanvas` so that the canvas does not stay flipped for other
  // paint commands later in the pipeline that aren't expecting a flipped
  // canvas.
  gfx::ScopedCanvas scoped_canvas(canvas);
  if (!view->GetFlipCanvasOnPaintForRTLUI()) {
    scoped_canvas.FlipIfRTL(view->width());
  }

  // The progress bar is a rounded-rect (of radius
  // `kDownloadProgressLeadingEdgeRadius`) that is clipped to a slightly smaller
  // rounded-rect. The clip's radius is set such that there appears to be a
  // border of thickness `kDownloadProgressBorderThickness` all around the tile
  // (note that this is not an actual `views::Border`).

  // Set the clip.
  gfx::Rect clip_bounds(view->GetLocalBounds());
  clip_bounds.Inset(kDownloadProgressBorderThickness);
  int clip_radius =
      std::max(0, tile->corner_radius_ - kDownloadProgressBorderThickness);
  SkScalar clip_radii[8];
  std::fill_n(clip_radii, 8, clip_radius);
  SkPath clip;
  clip.addRoundRect(gfx::RectToSkRect(clip_bounds), clip_radii);
  canvas->ClipPath(clip, /*do_anti_alias=*/true);

  // Shrink the width of the progress bar according to the tile's current
  // download progress.
  float percent = static_cast<float>(tile->download_progress_percent_) / 100.0f;
  gfx::Rect progress_bounds(clip_bounds);
  progress_bounds.set_width(percent * progress_bounds.width());

  // Draw the progress bar.
  cc::PaintFlags progress_flags;
  progress_flags.setAntiAlias(true);
  progress_flags.setStyle(cc::PaintFlags::kFill_Style);
  progress_flags.setColor(
      view->GetColorProvider()->GetColor(progress_color_id_));
  canvas->DrawRoundRect(progress_bounds, kDownloadProgressLeadingEdgeRadius,
                        progress_flags);
}

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
  UpdateAccessibilityProperties();

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
  title_container_->RemoveObserver(this);
}

void FeatureTile::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FeatureTile::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FeatureTile::CreateChildViews() {
  const bool is_compact = type_ == TileType::kCompact;

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(is_compact ? views::LayoutOrientation::kVertical
                                  : views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  SetLayoutManagerUseConstrainedSpace(false);

  // Set `MaximumFlexSizeRule` to `kUnbounded` so the view takes up all of the
  // available space in its parent container.
  SetProperty(views::kFlexBehaviorKey,
              views::FlexSpecification(views::FlexSpecification(
                  views::MinimumFlexSizeRule::kScaleToZero,
                  views::MaximumFlexSizeRule::kUnbounded,
                  /*adjust_height_for_width=*/true)));

  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);

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

  title_container_ =
      AddChildView(views::Builder<FlexLayoutView>()
                       .SetCanProcessEventsWithinSubtree(false)
                       .SetOrientation(views::LayoutOrientation::kVertical)
                       .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                       .Build());
  title_container_->AddObserver(this);
  // Set `MaximumFlexSizeRule` to `kUnbounded` so that `title_container_` takes
  // up all of the available space in the middle of the primary tile.
  title_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::FlexSpecification(
          is_compact ? views::LayoutOrientation::kVertical
                     : views::LayoutOrientation::kHorizontal,
          views::MinimumFlexSizeRule::kScaleToZero,
          views::MaximumFlexSizeRule::kUnbounded,
          /*adjust_height_for_width=*/true)));

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
    SetCompactTileLabelPreferences(/*has_sub_label=*/false);

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
  UpdateAccessibilityProperties();

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

void FeatureTile::SetOnTitleBoundsChangedCallback(
    base::RepeatingCallback<void()> callback) {
  on_title_container_bounds_changed_ = std::move(callback);
}

void FeatureTile::SetTitleContainerMargins(const gfx::Insets& insets) {
  title_container_->SetProperty(views::kMarginsKey, insets);
}

void FeatureTile::CreateDecorativeDrillInArrow() {
  CHECK_EQ(type_, TileType::kPrimary)
      << "Drill-in arrows are just used in Primary tiles";

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
  if (GetEnabled()) {
    background_color =
        toggled_
            ? background_toggled_color_.value_or(
                  cros_tokens::kCrosSysSystemPrimaryContainer)
            : background_color_.value_or(cros_tokens::kCrosSysSystemOnBase);
  } else {
    background_color = background_disabled_color_.value_or(
        cros_tokens::kCrosSysDisabledContainer);
  }

  ui::ColorId foreground_color;
  ui::ColorId foreground_optional_color;
  // The `DownloadState::kPending` state should have the same colors on the
  // labels and images as an enabled button, per the spec. The labels and images
  // will only look disabled if the button was disabled for other reasons.
  if (GetEnabled() || download_state_ == DownloadState::kPending) {
    foreground_color =
        toggled_ ? foreground_toggled_color_.value_or(
                       cros_tokens::kCrosSysSystemOnPrimaryContainer)
                 : foreground_color_.value_or(cros_tokens::kCrosSysOnSurface);
    foreground_optional_color =
        toggled_ ? foreground_optional_toggled_color_.value_or(
                       cros_tokens::kCrosSysSystemOnPrimaryContainer)
                 : foreground_optional_color_.value_or(
                       cros_tokens::kCrosSysOnSurfaceVariant);
  } else {
    foreground_color =
        foreground_disabled_color_.value_or(cros_tokens::kCrosSysDisabled);
    foreground_optional_color =
        foreground_disabled_color_.value_or(cros_tokens::kCrosSysDisabled);
  }

  SetBackground(
      features::IsVcDlcUiEnabled() &&
              download_state_ == DownloadState::kDownloading
          ? std::make_unique<ProgressBackground>(
                /*progress_color_id=*/cros_tokens::kCrosSysHighlightShape,
                /*background_color_id=*/background_color)
          : views::CreateThemedRoundedRectBackground(background_color,
                                                     corner_radius_));

  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetBaseColorId(toggled_
                               ? ink_drop_toggled_base_color_.value_or(
                                     cros_tokens::kCrosSysRipplePrimary)
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
  UpdateAccessibilityProperties();

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
void FeatureTile::SetBackgroundDisabledColorId(
    ui::ColorId background_disabled_color_id) {
  if (background_disabled_color_ == background_disabled_color_id) {
    return;
  }
  background_disabled_color_ = background_disabled_color_id;
  if (!GetEnabled()) {
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

void FeatureTile::SetForegroundDisabledColorId(
    ui::ColorId foreground_disabled_color_id) {
  if (foreground_disabled_color_ == foreground_disabled_color_id) {
    return;
  }
  foreground_disabled_color_ = foreground_disabled_color_id;
  if (!GetEnabled()) {
    UpdateColors();
  }
}

void FeatureTile::SetForegroundOptionalColorId(
    ui::ColorId foreground_optional_color_id) {
  if (foreground_optional_color_ == foreground_optional_color_id) {
    return;
  }
  foreground_optional_color_ = foreground_optional_color_id;
  if (!GetEnabled()) {
    UpdateColors();
  }
}

void FeatureTile::SetForegroundOptionalToggledColorId(
    ui::ColorId foreground_optional_toggled_color_id) {
  if (foreground_optional_toggled_color_ ==
      foreground_optional_toggled_color_id) {
    return;
  }
  foreground_optional_toggled_color_ = foreground_optional_toggled_color_id;
  if (!GetEnabled()) {
    UpdateColors();
  }
}

void FeatureTile::SetInkDropToggledBaseColorId(
    ui::ColorId ink_drop_toggled_base_color_id) {
  if (ink_drop_toggled_base_color_ == ink_drop_toggled_base_color_id) {
    return;
  }
  ink_drop_toggled_base_color_ = ink_drop_toggled_base_color_id;
  if (!GetEnabled()) {
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
  // If `VcDlcUi` is enabled and the tile is currently in a download state that
  // requires a non-client-specified label to be shown then store the new
  // client-specified label but don't immediately update the UI. The UI will be
  // updated to show the new label when the download finishes.
  if (features::IsVcDlcUiEnabled()) {
    client_specified_label_text_ = label;
    if (download_state_ == DownloadState::kPending ||
        download_state_ == DownloadState::kDownloading) {
      return;
    }
  }

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
    SetCompactTileLabelPreferences(/*has_sub_label=*/visible);
  }
}

void FeatureTile::SetDownloadState(DownloadState state, int progress) {
  // Download state is only supported when `VcDlcUi` is enabled.
  CHECK(features::IsVcDlcUiEnabled())
      << "Download states are not supported when `VcDlcUi` is disabled";

  // Check if this tile is already in a download state such that we can bail out
  // without doing any updates.
  if (download_state_ == state) {
    // We can always bail out early if we're already in the given
    // non-downloading state.
    if (state != DownloadState::kDownloading) {
      return;
    }

    // We can only bail out early from a downloading state if we're already at
    // the given download progress.
    if (download_progress_percent_ == progress) {
      return;
    }
  }
  download_state_ = state;

  switch (download_state_) {
    case DownloadState::kDownloading:
      CHECK_GE(progress, 0)
          << "Expected download progress to be in the range [0, 100], actual: "
          << progress;
      CHECK_LE(progress, 100)
          << "Expected download progress to be in the range [0, 100], actual: "
          << progress;
      SetEnabled(true);
      download_progress_percent_ = progress;
      break;
    case DownloadState::kError:
    case DownloadState::kPending:
      SetEnabled(false);
      download_progress_percent_ = 0;
      break;
    case DownloadState::kDownloaded:
      SetEnabled(true);
      download_progress_percent_ = 0;
      break;
    case DownloadState::kNone:
      SetEnabled(true);
      download_progress_percent_ = 0;
      break;
  }

  UpdateColors();
  UpdateLabelForDownloadState();

  // Once the tile's UI has been updated, notify any observers of the download
  // state change.
  NotifyDownloadStateChanged();
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

void FeatureTile::OnViewBoundsChanged(views::View* observed_view) {
  if (observed_view == title_container_ && on_title_container_bounds_changed_) {
    on_title_container_bounds_changed_.Run();
  }
}

void FeatureTile::OnSetTooltipText(const std::u16string& tooltip_text) {
  if (!features::IsVcDlcUiEnabled() || updating_download_state_labels_) {
    return;
  }

  // Keep track of the client set tooltip, so it can be restored if a
  // `DownloadState` tooltip has been temporarily set.
  client_specified_tooltip_text_ = tooltip_text;

  // If the tooltip was set while a temporary downloading tooltip text was set,
  // restore it. This will be reset to the `client_specified_tooltip_text_` once
  // the download state changes back to the final state (if it's not
  // `DownloadState::kError`).
  if (download_state_ != DownloadState::kDownloaded &&
      download_state_ != DownloadState::kNone) {
    UpdateLabelForDownloadState();
  }
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

void FeatureTile::UpdateAccessibilityProperties() {
  // If the icon is clickable then the main feature tile usually takes the user
  // to a detailed page (like Network or Bluetooth). Those tiles act more like a
  // regular button than a toggle button.
  if (is_togglable_ && !is_icon_clickable_) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    GetViewAccessibility().SetCheckedState(
        toggled_ ? ax::mojom::CheckedState::kTrue
                 : ax::mojom::CheckedState::kFalse);
  } else {
    GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    GetViewAccessibility().RemoveCheckedState();
  }
}

void FeatureTile::SetCompactTileLabelPreferences(bool has_sub_label) {
  label_->SetMultiLine(!has_sub_label);
  // Elide after 2 lines if there's no sub-label. Otherwise, 1 line.
  label_->SetMaxLines(has_sub_label ? 1 : 2);
}

void FeatureTile::SetDownloadLabel(const std::u16string& download_label,
                                   std::optional<std::u16string> tooltip) {
  // Download state is only supported when `VcDlcUi` is enabled.
  CHECK(features::IsVcDlcUiEnabled())
      << "Download states are not supported when `VcDlcUi` is disabled";
  label_->SetText(download_label);
  SetTooltipText(tooltip.value_or(download_label));
}

void FeatureTile::UpdateLabelForDownloadState() {
  // Download state is only supported when `VcDlcUi` is enabled.
  CHECK(features::IsVcDlcUiEnabled())
      << "Download states are not supported when `VcDlcUi` is disabled";

  base::AutoReset<bool> reset(&updating_download_state_labels_, true);

  switch (download_state_) {
    case DownloadState::kError:
      SetDownloadLabel(client_specified_label_text_,
                       /*tooltip=*/l10n_util::GetStringFUTF16(
                           IDS_ASH_FEATURE_TILE_DOWNLOAD_ERROR,
                           client_specified_label_text_));
      break;
    case DownloadState::kNone:
    case DownloadState::kDownloaded:
      // If a download happened, `SetLabel()` saved the previous label
      // in `client_specified_label_text_`, so re-set those here.
      label_->SetText(client_specified_label_text_);
      SetTooltipText(client_specified_tooltip_text_.empty()
                         ? client_specified_label_text_
                         : client_specified_tooltip_text_);
      break;
    case DownloadState::kPending:
      SetDownloadLabel(l10n_util::GetStringUTF16(
          IDS_ASH_FEATURE_TILE_DOWNLOAD_PENDING_TITLE));
      break;
    case DownloadState::kDownloading:
      SetDownloadLabel(l10n_util::GetStringFUTF16(
          IDS_ASH_FEATURE_TILE_DOWNLOAD_IN_PROGRESS_TITLE,
          base::NumberToString16(download_progress_percent_)));
      break;
  }
}

void FeatureTile::NotifyDownloadStateChanged() {
  for (Observer& observer : observers_) {
    observer.OnDownloadStateChanged(download_state_,
                                    download_progress_percent_);
  }
}

BEGIN_METADATA(FeatureTile)
END_METADATA

}  // namespace ash
