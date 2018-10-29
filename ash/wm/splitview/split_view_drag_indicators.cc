// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_drag_indicators.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_highlight_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// When animating, this is the location of the split view label as a ratio of
// the width or height.
constexpr double kSplitviewLabelExpandTranslationPrimaryAxisRatio = 0.20;
constexpr double kSplitviewLabelShrinkTranslationPrimaryAxisRatio = 0.05;

// When a preview is shown, the opposite highlight will shrink to this length.
constexpr int kOtherHighlightLengthDp = 20;

// Creates the widget responsible for displaying the indicators.
std::unique_ptr<views::Widget> CreateWidget() {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent = Shell::GetContainer(Shell::Get()->GetPrimaryRootWindow(),
                                      kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(params);
  return widget;
}

// Computes the transform which rotates the labels |angle| degrees. The point
// of rotation is the relative center point of |bounds|.
gfx::Transform ComputeRotateAroundCenterTransform(const gfx::Rect& bounds,
                                                  double angle) {
  gfx::Transform transform;
  const gfx::Vector2dF center_point_vector =
      bounds.CenterPoint() - bounds.origin();
  transform.Translate(center_point_vector);
  transform.Rotate(angle);
  transform.Translate(-center_point_vector);
  return transform;
}

// Helper function to compute the transform for the indicator labels when the
// view changes states. |main_transform| determines what ratio of the highlight
// we want to shift to. |non_transformed_bounds| represents the bounds of the
// label before its transform is applied; the centerpoint is used to calculate
// the amount of shift. |highlight_length| will be used to calculate the amount
// of shift as well. If the label is not |left_or_top| (right or bottom) we
// will translate in the other direction.
gfx::Transform ComputeLabelTransform(bool main_transform,
                                     const gfx::Rect& non_transformed_bounds,
                                     int highlight_length,
                                     bool landscape,
                                     bool left_or_top) {
  // Compute the distance of the translation.
  const float ratio = main_transform
                          ? kSplitviewLabelExpandTranslationPrimaryAxisRatio
                          : kSplitviewLabelShrinkTranslationPrimaryAxisRatio;
  const gfx::Point center_point = non_transformed_bounds.CenterPoint();
  const int primary_axis_center =
      landscape ? center_point.x() : center_point.y();
  const float translate =
      std::fabs(ratio * highlight_length - primary_axis_center);

  // Translate along x for landscape, along y for portrait.
  gfx::Vector2dF translation(landscape ? translate : 0,
                             landscape ? 0 : translate);
  // Translate in other direction if right or bottom label.
  if (!left_or_top)
    translation = -translation;
  gfx::Transform transform;
  transform.Translate(translation);
  return transform;
}

}  // namespace

// static
bool SplitViewDragIndicators::IsPreviewAreaState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kPreviewAreaLeft ||
         indicator_state == IndicatorState::kPreviewAreaRight;
}

// static
bool SplitViewDragIndicators::IsLeftIndicatorState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kDragAreaLeft ||
         indicator_state == IndicatorState::kCannotSnapLeft;
}

// static
bool SplitViewDragIndicators::IsRightIndicatorState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kDragAreaRight ||
         indicator_state == IndicatorState::kCannotSnapRight;
}

// static
bool SplitViewDragIndicators::IsCannotSnapState(
    IndicatorState indicator_state) {
  return indicator_state == IndicatorState::kCannotSnap ||
         indicator_state == IndicatorState::kCannotSnapLeft ||
         indicator_state == IndicatorState::kCannotSnapRight;
}

// static
bool SplitViewDragIndicators::IsPreviewAreaOnLeftTopOfScreen(
    IndicatorState indicator_state) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  // kPreviewAreaLeft and kPreviewAreaRight correspond with LEFT_SNAPPED and
  // RIGHT_SNAPPED which do not always correspond to the physical left and right
  // of the screen. See split_view_controller.h for more details.
  return (indicator_state == IndicatorState::kPreviewAreaLeft &&
          split_view_controller->IsCurrentScreenOrientationPrimary()) ||
         (indicator_state == IndicatorState::kPreviewAreaRight &&
          !split_view_controller->IsCurrentScreenOrientationPrimary());
}

// View which contains a label and can be rotated. Used by and rotated by
// SplitViewDragIndicatorsView.
class SplitViewDragIndicators::RotatedImageLabelView : public views::View {
 public:
  explicit RotatedImageLabelView(bool is_right_or_bottom)
      : is_right_or_bottom_(is_right_or_bottom) {
    label_ = new views::Label(base::string16(), views::style::CONTEXT_LABEL);
    label_->SetPaintToLayer();
    label_->layer()->SetFillsBoundsOpaquely(false);
    label_->SetEnabledColor(kSplitviewLabelEnabledColor);
    label_->SetBackgroundColor(kSplitviewLabelBackgroundColor);

    // Use |label_parent_| to add padding and rounded edges to the text. Create
    // this extra view so that we can rotate the label, while having a slide
    // animation at times on the whole thing.
    label_parent_ = new RoundedRectView(kSplitviewLabelRoundRectRadiusDp,
                                        kSplitviewLabelBackgroundColor);
    label_parent_->SetPaintToLayer();
    label_parent_->layer()->SetFillsBoundsOpaquely(false);
    label_parent_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(kSplitviewLabelVerticalInsetDp,
                    kSplitviewLabelHorizontalInsetDp)));
    label_parent_->AddChildView(label_);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    AddChildView(label_parent_);
  }

  ~RotatedImageLabelView() override = default;

  void SetLabelText(const base::string16& text) { label_->SetText(text); }

  // Called when the view's bounds are altered. Rotates the view by |angle|
  // degrees.
  void OnBoundsUpdated(const gfx::Rect& bounds, double angle) {
    SetBoundsRect(bounds);
    label_parent_->SetBoundsRect(gfx::Rect(bounds.size()));
    label_parent_->SetTransform(
        ComputeRotateAroundCenterTransform(bounds, angle));
  }

  // Called to update the opacity of the labels view on |indicator_state|.
  void OnIndicatorTypeChanged(IndicatorState indicator_state,
                              IndicatorState previous_indicator_state) {
    if (indicator_state == IndicatorState::kNone ||
        IsPreviewAreaState(indicator_state)) {
      DoSplitviewOpacityAnimation(layer(), SPLITVIEW_ANIMATION_TEXT_FADE_OUT);
      return;
    }

    // No need to update left/top label view for right indicator state and also
    // no need to update right/bottom label view for left indicator state.
    if ((!is_right_or_bottom_ && IsRightIndicatorState(indicator_state)) ||
        (is_right_or_bottom_ && IsLeftIndicatorState(indicator_state))) {
      return;
    }

    SetLabelText(l10n_util::GetStringUTF16(IsCannotSnapState(indicator_state)
                                               ? IDS_ASH_SPLIT_VIEW_CANNOT_SNAP
                                               : IDS_ASH_SPLIT_VIEW_GUIDANCE));
    SplitviewAnimationType animation_type;
    if (Shell::Get()->split_view_controller()->state() ==
        SplitViewController::NO_SNAP) {
      animation_type = IsPreviewAreaState(previous_indicator_state)
                           ? SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT
                           : SPLITVIEW_ANIMATION_TEXT_FADE_IN;
    } else {
      animation_type = SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT;
    }
    DoSplitviewOpacityAnimation(layer(), animation_type);
  }

 protected:
  gfx::Size CalculatePreferredSize() const override {
    return label_parent_->GetPreferredSize();
  }

 private:
  // True if the label view is the right/bottom side one, false if it is the
  // left/top one.
  const bool is_right_or_bottom_;

  RoundedRectView* label_parent_ = nullptr;
  views::Label* label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RotatedImageLabelView);
};

// View which contains two highlights on each side indicator where a user should
// drag a selected window in order to initiate splitview. Each highlight has a
// label with instructions to further guide users. The highlights are on the
// left and right of the display in landscape mode, and on the top and bottom of
// the display in landscape mode. The highlights can expand and shrink if a
// window has entered a snap region to display the bounds of the window, if it
// were to get snapped.
class SplitViewDragIndicators::SplitViewDragIndicatorsView
    : public views::View {
 public:
  SplitViewDragIndicatorsView() {
    left_highlight_view_ =
        new SplitViewHighlightView(/*is_right_or_bottom=*/false);
    right_highlight_view_ =
        new SplitViewHighlightView(/*is_right_or_bottom=*/true);

    left_highlight_view_->SetPaintToLayer();
    right_highlight_view_->SetPaintToLayer();
    left_highlight_view_->layer()->SetFillsBoundsOpaquely(false);
    right_highlight_view_->layer()->SetFillsBoundsOpaquely(false);

    AddChildView(left_highlight_view_);
    AddChildView(right_highlight_view_);

    left_rotated_view_ =
        new RotatedImageLabelView(/*is_right_or_bottom=*/false);
    right_rotated_view_ =
        new RotatedImageLabelView(/*is_right_or_bottom=*/true);

    AddChildView(left_rotated_view_);
    AddChildView(right_rotated_view_);

    // Nothing is shown initially.
    left_highlight_view_->layer()->SetOpacity(0.f);
    right_highlight_view_->layer()->SetOpacity(0.f);
    left_rotated_view_->layer()->SetOpacity(0.f);
    right_rotated_view_->layer()->SetOpacity(0.f);
  }

  ~SplitViewDragIndicatorsView() override {}

  // Called by parent widget when the state machine changes. Handles setting the
  // opacity of the highlights and labels based on the state.
  void OnIndicatorTypeChanged(IndicatorState indicator_state) {
    DCHECK_NE(indicator_state_, indicator_state);

    previous_indicator_state_ = indicator_state_;
    indicator_state_ = indicator_state;

    left_rotated_view_->OnIndicatorTypeChanged(indicator_state,
                                               previous_indicator_state_);
    right_rotated_view_->OnIndicatorTypeChanged(indicator_state,
                                                previous_indicator_state_);
    left_highlight_view_->OnIndicatorTypeChanged(indicator_state);
    right_highlight_view_->OnIndicatorTypeChanged(indicator_state);

    if (indicator_state != IndicatorState::kNone)
      Layout(previous_indicator_state_ != IndicatorState::kNone);
  }

  views::View* GetViewForIndicatorType(IndicatorType type) {
    switch (type) {
      case IndicatorType::kLeftHighlight:
        return left_highlight_view_;
      case IndicatorType::kLeftText:
        return left_rotated_view_;
      case IndicatorType::kRightHighlight:
        return right_highlight_view_;
      case IndicatorType::kRightText:
        return right_rotated_view_;
    }

    NOTREACHED();
    return nullptr;
  }

  // views::View:
  void Layout() override { Layout(/*animate=*/false); }

 private:
  // Layout the bounds of the highlight views and helper labels. One should
  // animate when changing states, but not when bounds or orientation is
  // changed.
  void Layout(bool animate) {
    const bool landscape = Shell::Get()
                               ->split_view_controller()
                               ->IsCurrentScreenOrientationLandscape();

    const int display_width = landscape ? width() : height();
    const int display_height = landscape ? height() : width();

    // Calculate the bounds of the two highlight regions.
    const int highlight_width =
        display_width * kHighlightScreenPrimaryAxisRatio;
    const int highlight_height =
        display_height - 2 * kHighlightScreenEdgePaddingDp;
    gfx::Size highlight_size(highlight_width, highlight_height);

    // The origin of the right highlight view in landscape, or the bottom
    // highlight view in portrait.
    gfx::Point right_bottom_origin(
        display_width - highlight_width - kHighlightScreenEdgePaddingDp,
        kHighlightScreenEdgePaddingDp);

    const gfx::Point hightlight_padding_point(kHighlightScreenEdgePaddingDp,
                                              kHighlightScreenEdgePaddingDp);
    gfx::Rect left_highlight_bounds(hightlight_padding_point, highlight_size);
    gfx::Rect right_highlight_bounds(right_bottom_origin, highlight_size);
    if (!landscape) {
      left_highlight_bounds.Transpose();
      right_highlight_bounds.Transpose();
    }
    const bool preview_left =
        (indicator_state_ == IndicatorState::kPreviewAreaLeft);
    if (IsPreviewAreaState(indicator_state_)) {
      // Get the preview area bounds from the split view controller.
      gfx::Rect preview_area_bounds =
          Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInScreen(
              GetWidget()->GetNativeWindow(), preview_left
                                                  ? SplitViewController::LEFT
                                                  : SplitViewController::RIGHT);
      gfx::Rect work_area_bounds = Shell::Get()
                                       ->split_view_controller()
                                       ->GetDisplayWorkAreaBoundsInScreen(
                                           GetWidget()->GetNativeWindow());
      preview_area_bounds.set_y(preview_area_bounds.y() - work_area_bounds.y());
      preview_area_bounds.Inset(kHighlightScreenEdgePaddingDp,
                                kHighlightScreenEdgePaddingDp);

      // Calculate the bounds of the other highlight, which is the one that
      // shrinks and fades away, while the other one, the preview area, expands
      // and takes up half the screen.
      gfx::Rect other_bounds(
          display_width - kOtherHighlightLengthDp -
              kHighlightScreenEdgePaddingDp,
          kHighlightScreenEdgePaddingDp, kOtherHighlightLengthDp,
          display_height - 2 * kHighlightScreenEdgePaddingDp);
      if (!landscape)
        other_bounds.Transpose();

      if (IsPreviewAreaOnLeftTopOfScreen(indicator_state_)) {
        left_highlight_bounds = preview_area_bounds;
        right_highlight_bounds = other_bounds;
      } else {
        other_bounds.set_origin(hightlight_padding_point);
        left_highlight_bounds = other_bounds;
        right_highlight_bounds = preview_area_bounds;
      }
    }

    left_highlight_view_->SetBounds(GetMirroredRect(left_highlight_bounds),
                                    landscape, animate);
    right_highlight_view_->SetBounds(GetMirroredRect(right_highlight_bounds),
                                     landscape, animate);

    // Calculate the bounds of the views which contain the guidance text and
    // icon. Rotate the two views in landscape mode.
    const int size_width =
        indicator_state_ == IndicatorState::kDragAreaLeft
            ? left_rotated_view_->GetPreferredSize().width()
            : right_rotated_view_->GetPreferredSize().width();
    gfx::Size size(size_width, kSplitviewLabelPreferredHeightDp);
    if (!landscape)
      highlight_size.SetSize(highlight_size.height(), highlight_size.width());
    gfx::Rect left_rotated_bounds(
        highlight_size.width() / 2 - size.width() / 2,
        highlight_size.height() / 2 - size.height() / 2, size.width(),
        size.height());
    gfx::Rect right_rotated_bounds = left_rotated_bounds;
    left_rotated_bounds.Offset(hightlight_padding_point.x(),
                               hightlight_padding_point.y());
    if (!landscape) {
      right_bottom_origin.SetPoint(right_bottom_origin.y(),
                                   right_bottom_origin.x());
    }
    right_rotated_bounds.Offset(right_bottom_origin.x(),
                                right_bottom_origin.y());

    // In portrait mode, there is no need to rotate the text and warning icon.
    // In landscape mode, rotate the left text 90 degrees clockwise in rtl and
    // 90 degress anti clockwise in ltr. The right text is rotated 90 degrees in
    // the opposite direction of the left text.
    double left_rotation_angle = 0.0;
    if (landscape)
      left_rotation_angle = 90.0 * (base::i18n::IsRTL() ? 1 : -1);

    left_rotated_view_->OnBoundsUpdated(left_rotated_bounds,
                                        /*angle=*/left_rotation_angle);
    right_rotated_view_->OnBoundsUpdated(right_rotated_bounds,
                                         /*angle=*/-left_rotation_angle);

    // Compute the transform for the labels. The labels slide in and out when
    // moving between states.
    gfx::Transform main_rotated_transform, other_rotated_transform;
    SplitviewAnimationType animation = SPLITVIEW_ANIMATION_TEXT_SLIDE_IN;
    if (IsPreviewAreaState(indicator_state_)) {
      animation = SPLITVIEW_ANIMATION_TEXT_SLIDE_OUT;
      main_rotated_transform =
          ComputeLabelTransform(preview_left, left_rotated_bounds,
                                highlight_width, landscape, preview_left);
      other_rotated_transform =
          ComputeLabelTransform(!preview_left, left_rotated_bounds,
                                highlight_width, landscape, preview_left);
    }

    DoSplitviewTransformAnimation(
        left_rotated_view_->layer(), animation,
        preview_left ? main_rotated_transform : other_rotated_transform);
    DoSplitviewTransformAnimation(
        right_rotated_view_->layer(), animation,
        preview_left ? other_rotated_transform : main_rotated_transform);
  }

  SplitViewHighlightView* left_highlight_view_ = nullptr;
  SplitViewHighlightView* right_highlight_view_ = nullptr;
  RotatedImageLabelView* left_rotated_view_ = nullptr;
  RotatedImageLabelView* right_rotated_view_ = nullptr;

  IndicatorState indicator_state_ = IndicatorState::kNone;
  IndicatorState previous_indicator_state_ = IndicatorState::kNone;

  DISALLOW_COPY_AND_ASSIGN(SplitViewDragIndicatorsView);
};

SplitViewDragIndicators::SplitViewDragIndicators() {
  indicators_view_ = new SplitViewDragIndicatorsView();
  widget_ = CreateWidget();
  widget_->SetContentsView(indicators_view_);
  widget_->Show();
}

SplitViewDragIndicators::~SplitViewDragIndicators() = default;

void SplitViewDragIndicators::SetIndicatorState(
    IndicatorState indicator_state,
    const gfx::Point& event_location) {
  if (indicator_state == current_indicator_state_)
    return;

  // Reparent the widget if needed.
  aura::Window* target = ash::wm::GetRootWindowAt(event_location);
  aura::Window* root_window = target->GetRootWindow();
  if (widget_->GetNativeView()->GetRootWindow() != root_window) {
    views::Widget::ReparentNativeView(
        widget_->GetNativeView(),
        Shell::GetContainer(root_window, kShellWindowId_OverlayContainer));
    widget_->SetContentsView(indicators_view_);
  }
  gfx::Rect bounds = screen_util::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_OverlayContainer));
  ::wm::ConvertRectToScreen(root_window, &bounds);
  widget_->SetBounds(bounds);

  current_indicator_state_ = indicator_state;
  indicators_view_->OnIndicatorTypeChanged(current_indicator_state_);
}

void SplitViewDragIndicators::OnDisplayBoundsChanged() {
  aura::Window* root_window = widget_->GetNativeView()->GetRootWindow();
  gfx::Rect bounds = screen_util::GetDisplayWorkAreaBoundsInParent(
      root_window->GetChildById(kShellWindowId_OverlayContainer));
  ::wm::ConvertRectToScreen(root_window, &bounds);
  widget_->SetBounds(bounds);
}

bool SplitViewDragIndicators::GetIndicatorTypeVisibilityForTesting(
    IndicatorType type) const {
  return indicators_view_->GetViewForIndicatorType(type)->layer()->opacity() >
         0.f;
}

}  // namespace ash
