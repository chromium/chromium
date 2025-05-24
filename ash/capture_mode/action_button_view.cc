// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Action button insets when it is shown in full (text and icon).
constexpr auto kFullActionButtonInsets = gfx::Insets::TLBR(8, 12, 8, 16);

// Action button insets when it only has a text label (no icon).
constexpr auto kTextOnlyActionButtonInsets = gfx::Insets::VH(8, 16);

// Action button insets when it is collapsed (icon only).
constexpr auto kCollapsedActionButtonInsets = gfx::Insets(8);

// The horizontal spacing between the icon and label in an action button.
constexpr int kActionButtonIconLabelSpacing = 8;

// The corner radius for an action button.
constexpr int kActionButtonRadius = 18;

// The size of the icon in an action button.
constexpr int kActionButtonIconSize = 20;

}  // namespace

ActionButtonView::ActionButtonView(views::Button::PressedCallback callback,
                                   std::u16string text,
                                   const gfx::VectorIcon* icon,
                                   ActionButtonRank rank)
    : views::Button(std::move(callback)),
      rank_(rank),
      // Since this view has fully circular rounded corners, we can't use a
      // nine patch layer for the shadow. We have to use the
      // `ShadowOnTextureLayer`. For more info, see https://crbug.com/1308800.
      shadow_(SystemShadow::CreateShadowOnTextureLayer(
          SystemShadow::Type::kElevation12)) {
  box_layout_ = SetLayoutManager(
      icon ? std::make_unique<views::BoxLayout>(
                 views::BoxLayout::Orientation::kHorizontal,
                 kFullActionButtonInsets, kActionButtonIconLabelSpacing)
           : std::make_unique<views::BoxLayout>(
                 views::BoxLayout::Orientation::kHorizontal,
                 kTextOnlyActionButtonInsets));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kActionButtonRadius));
  shadow_->SetRoundedCornerRadius(kActionButtonRadius);
  capture_mode_util::SetHighlightBorder(
      this, kActionButtonRadius,
      views::HighlightBorder::Type::kHighlightBorderNoShadow);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kActionButtonRadius);

  StyleUtil::ConfigureInkDropAttributes(
      this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
  StyleUtil::SetUpInkDropForButton(this);
  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());
  // The container should adjust its bounds if we collapse to an icon button.
  ink_drop_container_->SetAutoMatchParentBounds(true);

  if (icon) {
    image_view_ = AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            *icon, kColorAshButtonIconColor, kActionButtonIconSize)));
  }
  label_ = AddChildView(std::make_unique<views::Label>(text));
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *label_);

  CaptureModeSessionFocusCycler::HighlightHelper::Install(this);
  SetAccessibleName(text);
}

ActionButtonView::~ActionButtonView() {
  views::InkDrop::Remove(this);
}

void ActionButtonView::AddedToWidget() {
  views::Button::AddedToWidget();

  // Since the layer of the shadow has to be added as a sibling to this view's
  // layer, we need to wait until the view is added to the widget.
  auto* parent = layer()->parent();
  ui::Layer* shadow_layer = shadow_->GetLayer();
  parent->Add(shadow_layer);
  parent->StackAtBottom(shadow_layer);

  // Make the shadow observe the color provider source change to update the
  // colors.
  shadow_->ObserveColorProviderSource(GetWidget());
}

void ActionButtonView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The shadow layer is a sibling of this view's layer, and should have the
  // same bounds.
  shadow_->SetContentBounds(layer()->bounds());
}

void ActionButtonView::AddLayerToRegion(ui::Layer* layer,
                                        views::LayerRegion region) {
  // This routes background layers to `ink_drop_container_` instead of `this` to
  // avoid painting effects underneath our background.
  ink_drop_container_->AddLayerToRegion(layer, region);
}

void ActionButtonView::RemoveLayerFromRegions(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerFromRegions(layer);
}

void ActionButtonView::CollapseToIconButton() {
  if (!label_->GetVisible()) {
    return;
  }
  label_->SetVisible(false);
  const std::u16string label_text(label_->GetText());
  label_->SetTooltipText(label_text);
  box_layout_->set_inside_border_insets(kCollapsedActionButtonInsets);
}

void ActionButtonView::PerformFadeInAnimation(
    base::TimeDelta fade_in_duration) {
  CHECK(layer());
  layer()->SetOpacity(0.0f);
  shadow_->GetLayer()->SetOpacity(0.0f);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(fade_in_duration)
      .SetOpacity(layer(), 1.0f, gfx::Tween::LINEAR)
      .SetOpacity(shadow_->GetLayer(), 1.0f, gfx::Tween::LINEAR);
}

BEGIN_METADATA(ActionButtonView)
END_METADATA

}  // namespace ash
