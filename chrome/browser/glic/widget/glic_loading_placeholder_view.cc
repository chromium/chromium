// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_loading_placeholder_view.h"

#include <cmath>
#include <memory>
#include <utility>

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace glic {

namespace {

constexpr base::TimeDelta kPulseDuration = base::Milliseconds(1500);
constexpr int kPulseFrameRateHz = 60;
constexpr double kPulseCycleMs = 1500.0;
constexpr float kHighOpacity = 1.0f;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicLoadingPlaceholderView,
                                      kElementIdForTesting);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicLoadingPlaceholderView,
                                      kCloseButtonElementIdForTesting);

GlicLoadingPlaceholderView::GlicLoadingPlaceholderView(
    Mode mode,
    base::RepeatingClosure close_callback)
    : mode_(mode),
      close_callback_(std::move(close_callback)),
      pulse_animation_(kPulseDuration, kPulseFrameRateHz, this) {
  SetProperty(views::kElementIdentifierKey, kElementIdForTesting);
  // Make the placeholder container focusable (without a `views::FocusRing`) so
  // floating window activation focuses the panel surface—keeping `Escape`
  // accelerators active without auto-focusing the Close button—while Tab still
  // reaches the Close button for keyboard navigation.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  GetViewAccessibility().SetRole(ax::mojom::Role::kPane);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_GLIC_WINDOW_TITLE));
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  UpdateBackgroundLayer();

  switch (mode_) {
    case Mode::kSidePanel:
      BuildSidePanelLayout();
      break;
    case Mode::kFloating:
      BuildFloatingLayout();
      break;
  }
}

GlicLoadingPlaceholderView::~GlicLoadingPlaceholderView() {
  pulse_animation_.Stop();
}

void GlicLoadingPlaceholderView::SetBackgroundRoundedCorners(
    const gfx::RoundedCornersF& radii) {
  if (radii_ == radii) {
    return;
  }
  radii_ = radii;
  UpdateBackgroundLayer();
}

bool GlicLoadingPlaceholderView::IsPointWithinDraggableHeader(
    const gfx::Point& point_in_placeholder) const {
  if (mode_ != Mode::kFloating || !GetVisible() || !header_view_) {
    return false;
  }
  const gfx::Point point_in_header = views::View::ConvertPointToTarget(
      this, header_view_, point_in_placeholder);
  if (!header_view_->GetLocalBounds().Contains(point_in_header)) {
    return false;
  }
  if (close_button_) {
    const gfx::Point point_in_close_button = views::View::ConvertPointToTarget(
        this, close_button_, point_in_placeholder);
    if (close_button_->GetLocalBounds().Contains(point_in_close_button)) {
      return false;
    }
  }
  return true;
}

void GlicLoadingPlaceholderView::VisibilityChanged(views::View* starting_from,
                                                   bool is_visible) {
  views::View::VisibilityChanged(starting_from, is_visible);
  UpdateAnimationState();
}

void GlicLoadingPlaceholderView::AddedToWidget() {
  views::View::AddedToWidget();
  UpdateAnimationState();
}

void GlicLoadingPlaceholderView::RemovedFromWidget() {
  UpdateAnimationState();
  views::View::RemovedFromWidget();
}

void GlicLoadingPlaceholderView::AnimationEnded(
    const gfx::Animation* animation) {
  if (IsDrawn() && gfx::Animation::ShouldRenderRichAnimation()) {
    ++cycle_count_;
    pulse_animation_.Start();
  } else {
    cycle_count_ = 0;
    ResetBlockOpacities();
  }
}

void GlicLoadingPlaceholderView::AnimationProgressed(
    const gfx::Animation* animation) {
  const double total_elapsed_ms =
      cycle_count_ * kPulseCycleMs +
      pulse_animation_.GetCurrentValue() * kPulseCycleMs;
  UpdateBlockOpacities(total_elapsed_ms);
}

void GlicLoadingPlaceholderView::ApplyAnimationElapsedForTesting(
    base::TimeDelta elapsed) {
  UpdateBlockOpacities(elapsed.InMillisecondsF());
}

void GlicLoadingPlaceholderView::BuildSidePanelLayout() {
  auto* outer_layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
  outer_layout->SetOrientation(views::LayoutOrientation::kVertical);
  outer_layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  auto header = std::make_unique<views::View>();
  auto* header_layout =
      header->SetLayoutManager(std::make_unique<views::FlexLayout>());
  header_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_layout->SetInteriorMargin(gfx::Insets::TLBR(12, 12, 8, 20));

  header->AddChildView(CreatePlaceholderBlock(
      gfx::Size(78, 17), /*corner_radius=*/12.0f, base::Milliseconds(0)));

  auto spacer = std::make_unique<views::View>();
  spacer->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  header->AddChildView(std::move(spacer));

  std::unique_ptr<views::View> p2 = CreatePlaceholderBlock(
      gfx::Size(78, 17), /*corner_radius=*/12.0f, base::Milliseconds(100));
  p2->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 8));
  header->AddChildView(std::move(p2));

  header->AddChildView(CreateCloseButton());
  header_view_ = AddChildView(std::move(header));

  auto body = std::make_unique<views::View>();
  auto* body_layout =
      body->SetLayoutManager(std::make_unique<views::FlexLayout>());
  body_layout->SetOrientation(views::LayoutOrientation::kVertical);
  body_layout->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  body_layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  body_layout->SetInteriorMargin(gfx::Insets::TLBR(0, 10, 10, 10));
  body->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kVertical,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  std::unique_ptr<views::View> p3 = CreatePlaceholderBlock(
      gfx::Size(100, 244), /*corner_radius=*/12.0f, base::Milliseconds(300));
  p3->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kVertical,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
  p3->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(48, 0, 18, 0));
  body->AddChildView(std::move(p3));

  body->AddChildView(CreatePlaceholderBlock(
      gfx::Size(100, 112), /*corner_radius=*/12.0f, base::Milliseconds(500)));

  AddChildView(std::move(body));
}

void GlicLoadingPlaceholderView::BuildFloatingLayout() {
  auto* outer_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  outer_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  outer_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  auto header = std::make_unique<views::View>();
  auto* header_layout =
      header->SetLayoutManager(std::make_unique<views::FlexLayout>());
  header_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_layout->SetInteriorMargin(gfx::Insets::TLBR(12, 12, 8, 20));

  std::unique_ptr<views::View> p1 = CreatePlaceholderBlock(
      gfx::Size(60, 17), /*corner_radius=*/5.0f, base::Milliseconds(0));
  p1->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  p1->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 60));
  header->AddChildView(std::move(p1));
  header->AddChildView(CreateCloseButton());
  header_view_ = AddChildView(std::move(header));

  auto second_row = std::make_unique<views::View>();
  auto* second_row_layout =
      second_row->SetLayoutManager(std::make_unique<views::FlexLayout>());
  second_row_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  second_row_layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  second_row_layout->SetInteriorMargin(gfx::Insets::TLBR(0, 10, 10, 10));

  second_row->AddChildView(CreatePlaceholderBlock(
      gfx::Size(80, 32), /*corner_radius=*/16.0f, base::Milliseconds(100)));

  auto spacer = std::make_unique<views::View>();
  spacer->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  second_row->AddChildView(std::move(spacer));

  second_row->AddChildView(CreatePlaceholderBlock(
      gfx::Size(32, 32), /*corner_radius=*/8.0f, base::Milliseconds(300)));

  AddChildView(std::move(second_row));
}

std::unique_ptr<views::View> GlicLoadingPlaceholderView::CreatePlaceholderBlock(
    const gfx::Size& preferred_size,
    float corner_radius,
    base::TimeDelta delay) {
  auto block = std::make_unique<views::View>();
  block->SetPreferredSize(preferred_size);
  block->SetPaintToLayer();
  block->layer()->SetFillsBoundsOpaquely(false);
  block->SetBackground(views::CreateRoundedRectBackground(
      kColorGlicPlaceholderElem, corner_radius));
  blocks_.push_back(AnimatedBlock{.view = block.get(), .delay = delay});
  return block;
}

std::unique_ptr<views::ImageButton>
GlicLoadingPlaceholderView::CreateCloseButton() {
  static constexpr int kCloseButtonSizeDip = 24;
  static constexpr int kCloseIconSizeDip = 20;

  auto button = std::make_unique<views::ImageButton>(
      close_callback_ ? views::Button::PressedCallback(close_callback_)
                      : views::Button::PressedCallback());
  button->SetProperty(views::kElementIdentifierKey,
                      kCloseButtonElementIdForTesting);
  button->SetPreferredSize(gfx::Size(kCloseButtonSizeDip, kCloseButtonSizeDip));
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseIcon, ui::kColorIcon,
                                     kCloseIconSizeDip));
  const std::u16string close_label =
      l10n_util::GetStringUTF16(IDS_GLIC_NOTICE_CLOSE_BUTTON_LABEL);
  button->GetViewAccessibility().SetName(close_label);
  button->SetTooltipText(close_label);
  views::InstallCircleHighlightPathGenerator(button.get());
  // Paint to its own layer so it renders above the solid-color background
  // layer created by `CreateLayerBasedRoundedBackground`.
  button->SetPaintToLayer();
  button->layer()->SetFillsBoundsOpaquely(false);
  close_button_ = button.get();
  return button;
}

void GlicLoadingPlaceholderView::UpdateBackgroundLayer() {
  SetBackground(
      views::CreateLayerBasedRoundedBackground(kColorGlicBackground, radii_));
  background()->SetInternalName("GlicLoadingPlaceholderView/background");
  if (layer()) {
    layer()->SetMasksToBounds(!radii_.IsEmpty());
  }
}

void GlicLoadingPlaceholderView::UpdateAnimationState() {
  const bool should_animate =
      IsDrawn() && gfx::Animation::ShouldRenderRichAnimation();
  if (should_animate) {
    if (!pulse_animation_.is_animating()) {
      cycle_count_ = 0;
      pulse_animation_.Start();
    }
  } else {
    if (pulse_animation_.is_animating()) {
      pulse_animation_.Stop();
    }
    cycle_count_ = 0;
    ResetBlockOpacities();
  }
}

void GlicLoadingPlaceholderView::ResetBlockOpacities() {
  for (const AnimatedBlock& block : blocks_) {
    if (block.view && block.view->layer()) {
      block.view->layer()->SetOpacity(kHighOpacity);
    }
  }
}

void GlicLoadingPlaceholderView::UpdateBlockOpacities(double total_elapsed_ms) {
  static constexpr double kFadeOutEndPhase = 0.66;
  static constexpr float kLowOpacity = 0.6f;

  for (const AnimatedBlock& block : blocks_) {
    if (!block.view || !block.view->layer()) {
      continue;
    }
    const double delay_ms = block.delay.InMillisecondsF();
    float opacity = kHighOpacity;
    if (total_elapsed_ms >= delay_ms) {
      const double phase =
          std::fmod(total_elapsed_ms - delay_ms, kPulseCycleMs) / kPulseCycleMs;
      if (phase <= kFadeOutEndPhase) {
        const double t = gfx::Tween::CalculateValue(gfx::Tween::EASE_IN_OUT,
                                                    phase / kFadeOutEndPhase);
        opacity = gfx::Tween::FloatValueBetween(t, kHighOpacity, kLowOpacity);
      } else {
        const double t = gfx::Tween::CalculateValue(
            gfx::Tween::EASE_IN_OUT,
            (phase - kFadeOutEndPhase) / (1.0 - kFadeOutEndPhase));
        opacity = gfx::Tween::FloatValueBetween(t, kLowOpacity, kHighOpacity);
      }
    }
    block.view->layer()->SetOpacity(opacity);
  }
}

BEGIN_METADATA(GlicLoadingPlaceholderView)
END_METADATA

}  // namespace glic
