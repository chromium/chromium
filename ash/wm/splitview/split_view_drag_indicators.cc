// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_drag_indicators.h"

#include <utility>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_highlight_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

// When a preview is shown, the opposite highlight shall contract to this ratio
// of the screen length.
constexpr float kOtherHighlightScreenPrimaryAxisRatio = 0.03f;

// Creates the widget responsible for displaying the indicators.
std::unique_ptr<views::Widget> CreateWidget(aura::Window* root_window) {
  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.accept_events = false;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);
  widget->set_focus_on_creation(false);
  widget->Init(std::move(params));
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

// Returns the work area bounds that has no overlap with shelf.
gfx::Rect GetWorkAreaBoundsNoOverlapWithShelf(aura::Window* root_window) {
  aura::Window* window =
      root_window->GetChildById(kShellWindowId_OverlayContainer);
  gfx::Rect bounds = screen_util::GetDisplayWorkAreaBoundsInParent(window);
  ::wm::ConvertRectToScreen(root_window, &bounds);

  bounds.Subtract(Shelf::ForWindow(root_window)->GetIdealBounds());
  return bounds;
}

}  // namespace

// static
SplitViewController::SnapPosition SplitViewDragIndicators::GetSnapPosition(
    WindowDraggingState window_dragging_state) {
  switch (window_dragging_state) {
    case WindowDraggingState::kToSnapLeft:
      return SplitViewController::LEFT;
    case WindowDraggingState::kToSnapRight:
      return SplitViewController::RIGHT;
    default:
      return SplitViewController::NONE;
  }
}

// static
SplitViewDragIndicators::WindowDraggingState
SplitViewDragIndicators::ComputeWindowDraggingState(
    bool is_dragging,
    WindowDraggingState non_snap_state,
    SplitViewController::SnapPosition snap_position) {
  if (!is_dragging || !ShouldAllowSplitView())
    return WindowDraggingState::kNoDrag;
  switch (snap_position) {
    case SplitViewController::NONE:
      return non_snap_state;
    case SplitViewController::LEFT:
      return WindowDraggingState::kToSnapLeft;
    case SplitViewController::RIGHT:
      return WindowDraggingState::kToSnapRight;
  }
}

// View which contains a label and can be rotated. Used by and rotated by
// SplitViewDragIndicatorsView.
class SplitViewDragIndicators::RotatedImageLabelView : public views::View {
 public:
  explicit RotatedImageLabelView(bool is_right_or_bottom)
      : is_right_or_bottom_(is_right_or_bottom) {
    // TODO(sammiequon): Remove this extra intermediate layer.
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    layer()->SetFillsBoundsOpaquely(false);

    // Use |label_parent_| to add padding and rounded edges to the text. Create
    // this extra view so that we can rotate the label, while having a slide
    // animation at times on the whole thing.
    label_parent_ = AddChildView(std::make_unique<RoundedRectView>(
        kSplitviewLabelRoundRectRadiusDp, kSplitviewLabelBackgroundColor));
    label_parent_->SetPaintToLayer();
    label_parent_->layer()->SetFillsBoundsOpaquely(false);
    label_parent_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(kSplitviewLabelVerticalInsetDp,
                    kSplitviewLabelHorizontalInsetDp)));

    label_ = label_parent_->AddChildView(std::make_unique<views::Label>(
        base::string16(), views::style::CONTEXT_LABEL));
    label_->SetEnabledColor(kSplitviewLabelEnabledColor);
    label_->SetBackgroundColor(kSplitviewLabelBackgroundColor);
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
  void OnWindowDraggingStateChanged(
      WindowDraggingState window_dragging_state,
      WindowDraggingState previous_window_dragging_state,
      bool can_dragged_window_be_snapped) {
    // No top label for dragging from the top in portrait orientation.
    if (window_dragging_state == WindowDraggingState::kFromTop &&
        !IsCurrentScreenOrientationLandscape() && !is_right_or_bottom_) {
      return;
    }

    // If there is no drag currently in this display, any label that is showing
    // shall fade out with the corresponding indicator.
    if (window_dragging_state == WindowDraggingState::kNoDrag ||
        window_dragging_state == WindowDraggingState::kOtherDisplay) {
      DoSplitviewOpacityAnimation(
          layer(), SPLITVIEW_ANIMATION_TEXT_FADE_OUT_WITH_HIGHLIGHT);
      return;
    }

    // When a snap preview is shown, any label that is showing shall fade out.
    if (GetSnapPosition(window_dragging_state) != SplitViewController::NONE) {
      DoSplitviewOpacityAnimation(layer(), SPLITVIEW_ANIMATION_TEXT_FADE_OUT);
      return;
    }

    // Set the text according to |can_dragged_window_be_snapped|.
    SetLabelText(l10n_util::GetStringUTF16(
        can_dragged_window_be_snapped ? IDS_ASH_SPLIT_VIEW_GUIDANCE
                                      : IDS_ASH_SPLIT_VIEW_CANNOT_SNAP));

    // When dragging begins in this display or comes in from another display, if
    // there is now no snap preview, fade in with an indicator.
    if (previous_window_dragging_state == WindowDraggingState::kNoDrag ||
        previous_window_dragging_state == WindowDraggingState::kOtherDisplay) {
      DoSplitviewOpacityAnimation(
          layer(), SPLITVIEW_ANIMATION_TEXT_FADE_IN_WITH_HIGHLIGHT);
      return;
    }

    // If a snap preview was shown, the labels shall now fade in.
    if (GetSnapPosition(previous_window_dragging_state) !=
        SplitViewController::NONE) {
      DoSplitviewOpacityAnimation(layer(), SPLITVIEW_ANIMATION_TEXT_FADE_IN);
      return;
    }
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
    : public views::View,
      public aura::WindowObserver {
 public:
  SplitViewDragIndicatorsView() {
    left_highlight_view_ = AddChildView(
        std::make_unique<SplitViewHighlightView>(/*is_right_or_bottom=*/false));
    right_highlight_view_ = AddChildView(
        std::make_unique<SplitViewHighlightView>(/*is_right_or_bottom=*/true));

    left_rotated_view_ = AddChildView(
        std::make_unique<RotatedImageLabelView>(/*is_right_or_bottom=*/false));
    right_rotated_view_ = AddChildView(
        std::make_unique<RotatedImageLabelView>(/*is_right_or_bottom=*/true));

    // Nothing is shown initially.
    left_highlight_view_->layer()->SetOpacity(0.f);
    right_highlight_view_->layer()->SetOpacity(0.f);
    left_rotated_view_->layer()->SetOpacity(0.f);
    right_rotated_view_->layer()->SetOpacity(0.f);
  }

  ~SplitViewDragIndicatorsView() override {
    if (dragged_window_)
      dragged_window_->RemoveObserver(this);
  }

  SplitViewHighlightView* left_highlight_view() { return left_highlight_view_; }

  // Called by parent widget when the state machine changes. Handles setting the
  // opacity and bounds of the highlights and labels.
  void OnWindowDraggingStateChanged(WindowDraggingState window_dragging_state) {
    DCHECK_NE(window_dragging_state_, window_dragging_state);
    previous_window_dragging_state_ = window_dragging_state_;
    window_dragging_state_ = window_dragging_state;

    SplitViewController* split_view_controller =
        SplitViewController::Get(GetWidget()->GetNativeWindow());
    const bool previews_only =
        window_dragging_state == WindowDraggingState::kFromShelf ||
        window_dragging_state == WindowDraggingState::kFromTop ||
        split_view_controller->InSplitViewMode();
    const bool can_dragged_window_be_snapped =
        dragged_window_ &&
        split_view_controller->CanSnapWindow(dragged_window_);
    if (!previews_only) {
      left_rotated_view_->OnWindowDraggingStateChanged(
          window_dragging_state, previous_window_dragging_state_,
          can_dragged_window_be_snapped);
      right_rotated_view_->OnWindowDraggingStateChanged(
          window_dragging_state, previous_window_dragging_state_,
          can_dragged_window_be_snapped);
    }
    left_highlight_view_->OnWindowDraggingStateChanged(
        window_dragging_state, previous_window_dragging_state_, previews_only,
        can_dragged_window_be_snapped);
    right_highlight_view_->OnWindowDraggingStateChanged(
        window_dragging_state, previous_window_dragging_state_, previews_only,
        can_dragged_window_be_snapped);

    if (window_dragging_state != WindowDraggingState::kNoDrag ||
        GetSnapPosition(previous_window_dragging_state_) !=
            SplitViewController::NONE) {
      Layout(previous_window_dragging_state_ != WindowDraggingState::kNoDrag);
    }
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

  void SetDraggedWindow(aura::Window* dragged_window) {
    if (dragged_window_)
      dragged_window_->RemoveObserver(this);
    dragged_window_ = dragged_window;
    if (dragged_window)
      dragged_window->AddObserver(this);
  }

  // views::View:
  void Layout() override { Layout(/*animate=*/false); }

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    DCHECK_EQ(dragged_window_, window);
    dragged_window_ = nullptr;
  }

 private:
  // Layout the bounds of the highlight views and helper labels. One should
  // animate when changing states, but not when bounds or orientation is
  // changed.
  void Layout(bool animate) {
    // TODO(xdai|afakhry): Attempt to simplify this logic.
    const bool horizontal = SplitViewController::IsLayoutHorizontal();
    const int display_width = horizontal ? width() : height();
    const int display_height = horizontal ? height() : width();

    // Calculate the bounds of the two highlight regions.
    const int highlight_width =
        display_width * kHighlightScreenPrimaryAxisRatio;
    const int highlight_height =
        display_height - 2 * kHighlightScreenEdgePaddingDp;
    gfx::Size highlight_size(highlight_width, highlight_height);

    // When one highlight expands to become a preview area, the other highlight
    // contracts to this width.
    const int other_highlight_width =
        display_width * kOtherHighlightScreenPrimaryAxisRatio;

    // The origin of the right highlight view in horizontal split view layout,
    // or the bottom highlight view in vertical split view layout.
    gfx::Point right_bottom_origin(
        display_width - highlight_width - kHighlightScreenEdgePaddingDp,
        kHighlightScreenEdgePaddingDp);

    const gfx::Point highlight_padding_point(kHighlightScreenEdgePaddingDp,
                                             kHighlightScreenEdgePaddingDp);
    gfx::Rect left_highlight_bounds(highlight_padding_point, highlight_size);
    gfx::Rect right_highlight_bounds(right_bottom_origin, highlight_size);
    if (!horizontal) {
      left_highlight_bounds.Transpose();
      right_highlight_bounds.Transpose();
    }

    // True when the drag ends in a snap area, meaning that the dragged window
    // actually becomes snapped.
    const bool drag_ending_in_snap =
        window_dragging_state_ == WindowDraggingState::kNoDrag &&
        GetSnapPosition(previous_window_dragging_state_) !=
            SplitViewController::NONE;

    SplitViewController::SnapPosition snap_position =
        GetSnapPosition(window_dragging_state_);
    if (snap_position == SplitViewController::NONE)
      snap_position = GetSnapPosition(previous_window_dragging_state_);

    gfx::Rect preview_area_bounds;
    base::Optional<SplitviewAnimationType> left_highlight_animation_type;
    base::Optional<SplitviewAnimationType> right_highlight_animation_type;
    if (GetSnapPosition(window_dragging_state_) != SplitViewController::NONE ||
        drag_ending_in_snap) {
      // Get the preview area bounds from the split view controller.
      preview_area_bounds =
          SplitViewController::Get(GetWidget()->GetNativeWindow())
              ->GetSnappedWindowBoundsInScreen(snap_position, dragged_window_);

      aura::Window* root_window =
          GetWidget()->GetNativeWindow()->GetRootWindow();
      wm::ConvertRectFromScreen(root_window, &preview_area_bounds);

      // Preview area should have no overlap with the shelf.
      preview_area_bounds.Subtract(
          Shelf::ForWindow(root_window)->GetIdealBounds());

      gfx::Rect work_area_bounds =
          GetWorkAreaBoundsNoOverlapWithShelf(root_window);
      wm::ConvertRectFromScreen(root_window, &work_area_bounds);
      preview_area_bounds.set_y(preview_area_bounds.y() - work_area_bounds.y());
      if (!drag_ending_in_snap) {
        preview_area_bounds.Inset(kHighlightScreenEdgePaddingDp,
                                  kHighlightScreenEdgePaddingDp);
      }

      // Calculate the bounds of the other highlight, which is the one that
      // shrinks and fades away, while the other one, the preview area, expands
      // and takes up half the screen.
      gfx::Rect other_bounds(
          display_width - other_highlight_width - kHighlightScreenEdgePaddingDp,
          kHighlightScreenEdgePaddingDp, other_highlight_width,
          display_height - 2 * kHighlightScreenEdgePaddingDp);
      if (!horizontal)
        other_bounds.Transpose();

      if (SplitViewController::IsPhysicalLeftOrTop(snap_position)) {
        left_highlight_bounds = preview_area_bounds;
        right_highlight_bounds = other_bounds;
        if (animate) {
          if (drag_ending_in_snap) {
            left_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET;
          } else {
            left_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN;
            right_highlight_animation_type =
                SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT;
          }
        }
      } else {
        other_bounds.set_origin(highlight_padding_point);
        left_highlight_bounds = other_bounds;
        right_highlight_bounds = preview_area_bounds;
        if (animate) {
          if (drag_ending_in_snap) {
            right_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET;
          } else {
            left_highlight_animation_type =
                SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT;
            right_highlight_animation_type =
                SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_IN;
          }
        }
      }
    } else if (GetSnapPosition(previous_window_dragging_state_) !=
                   SplitViewController::NONE &&
               animate) {
      if (SplitViewController::IsPhysicalLeftOrTop(snap_position)) {
        left_highlight_animation_type =
            SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT;
        right_highlight_animation_type =
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN;
      } else {
        left_highlight_animation_type =
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN;
        right_highlight_animation_type =
            SPLITVIEW_ANIMATION_PREVIEW_AREA_SLIDE_OUT;
      }
    }

    left_highlight_view_->SetBounds(GetMirroredRect(left_highlight_bounds),
                                    left_highlight_animation_type);
    right_highlight_view_->SetBounds(GetMirroredRect(right_highlight_bounds),
                                     right_highlight_animation_type);

    // Calculate the bounds of the views which contain the guidance text and
    // icon. Rotate the two views in horizontal split view layout.
    const gfx::Size size(right_rotated_view_->GetPreferredSize().width(),
                         kSplitviewLabelPreferredHeightDp);
    if (!horizontal)
      highlight_size.SetSize(highlight_size.height(), highlight_size.width());
    gfx::Rect left_rotated_bounds(
        highlight_size.width() / 2 - size.width() / 2,
        highlight_size.height() / 2 - size.height() / 2, size.width(),
        size.height());
    gfx::Rect right_rotated_bounds = left_rotated_bounds;
    left_rotated_bounds.Offset(highlight_padding_point.x(),
                               highlight_padding_point.y());
    if (!horizontal) {
      right_bottom_origin.SetPoint(right_bottom_origin.y(),
                                   right_bottom_origin.x());
    }
    right_rotated_bounds.Offset(right_bottom_origin.x(),
                                right_bottom_origin.y());

    // In portrait mode, there is no need to rotate the text and warning icon.
    // In horizontal split view layout, rotate the left text 90 degrees
    // clockwise in rtl and 90 degress anti clockwise in ltr. The right text is
    // rotated 90 degrees in the opposite direction of the left text.
    double left_rotation_angle = 0.0;
    if (horizontal)
      left_rotation_angle = 90.0 * (base::i18n::IsRTL() ? 1 : -1);

    left_rotated_view_->OnBoundsUpdated(left_rotated_bounds,
                                        /*angle=*/left_rotation_angle);
    right_rotated_view_->OnBoundsUpdated(right_rotated_bounds,
                                         /*angle=*/-left_rotation_angle);

    if (drag_ending_in_snap) {
      // Reset the label transforms, in preparation for the next drag (if any).
      left_rotated_view_->layer()->SetTransform(gfx::Transform());
      right_rotated_view_->layer()->SetTransform(gfx::Transform());
      return;
    }

    ui::Layer* preview_label_layer;
    ui::Layer* other_highlight_label_layer;
    if (snap_position == SplitViewController::NONE) {
      preview_label_layer = nullptr;
      other_highlight_label_layer = nullptr;
    } else if (SplitViewController::IsPhysicalLeftOrTop(snap_position)) {
      preview_label_layer = left_rotated_view_->layer();
      other_highlight_label_layer = right_rotated_view_->layer();
    } else {
      preview_label_layer = right_rotated_view_->layer();
      other_highlight_label_layer = left_rotated_view_->layer();
    }

    // Slide out the labels when a snap preview appears. This code also adjusts
    // the label transforms for things like display rotation while there is a
    // snap preview.
    if (GetSnapPosition(window_dragging_state_) != SplitViewController::NONE) {
      // How far each label shall slide to stay centered in the corresponding
      // highlight as it expands/contracts. Include distance traveled with zero
      // opacity (whence a label still slides, not only for simplicity in
      // calculating the values below, but also to facilitate that the label
      // transform and the highlight transform have matching easing).
      const float preview_label_distance =
          0.5f * (preview_area_bounds.width() - highlight_width);
      const float other_highlight_label_distance =
          0.5f * (highlight_width - other_highlight_width);

      // Positive for right or down; negative for left or up.
      float preview_label_delta, other_highlight_label_delta;
      if (SplitViewController::IsPhysicalLeftOrTop(snap_position)) {
        preview_label_delta = preview_label_distance;
        other_highlight_label_delta = other_highlight_label_distance;
      } else {
        preview_label_delta = -preview_label_distance;
        other_highlight_label_delta = -other_highlight_label_distance;
      }

      // x-axis if |horizontal|; else y-axis.
      gfx::Transform preview_label_transform, other_highlight_label_transform;
      if (horizontal) {
        preview_label_transform.Translate(preview_label_delta, 0.f);
        other_highlight_label_transform.Translate(other_highlight_label_delta,
                                                  0.f);
      } else {
        preview_label_transform.Translate(0.f, preview_label_delta);
        other_highlight_label_transform.Translate(0.f,
                                                  other_highlight_label_delta);
      }

      if (animate) {
        // Animate the labels sliding out.
        DoSplitviewTransformAnimation(
            preview_label_layer,
            SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_OUT,
            preview_label_transform, /*animation_observer=*/nullptr);
        DoSplitviewTransformAnimation(
            other_highlight_label_layer,
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_OUT,
            other_highlight_label_transform, /*animation_observer=*/nullptr);
      } else {
        // Put the labels where they belong.
        preview_label_layer->SetTransform(preview_label_transform);
        other_highlight_label_layer->SetTransform(
            other_highlight_label_transform);
      }
      return;
    }

    // Slide in the labels when a snap preview disappears because you drag
    // inward. (Having reached this code, we know that the window is not
    // becoming snapped, because that case is handled earlier and we bail out.)
    if (GetSnapPosition(previous_window_dragging_state_) !=
        SplitViewController::NONE) {
      if (animate) {
        // Animate the labels sliding in.
        DoSplitviewTransformAnimation(
            preview_label_layer, SPLITVIEW_ANIMATION_PREVIEW_AREA_TEXT_SLIDE_IN,
            gfx::Transform(), /*animation_observer=*/nullptr);
        DoSplitviewTransformAnimation(
            other_highlight_label_layer,
            SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_TEXT_SLIDE_IN, gfx::Transform(),
            /*animation_observer=*/nullptr);
      } else {
        // Put the labels where they belong.
        preview_label_layer->SetTransform(gfx::Transform());
        other_highlight_label_layer->SetTransform(gfx::Transform());
      }
      return;
    }
  }

  SplitViewHighlightView* left_highlight_view_ = nullptr;
  SplitViewHighlightView* right_highlight_view_ = nullptr;
  RotatedImageLabelView* left_rotated_view_ = nullptr;
  RotatedImageLabelView* right_rotated_view_ = nullptr;

  WindowDraggingState window_dragging_state_ = WindowDraggingState::kNoDrag;
  WindowDraggingState previous_window_dragging_state_ =
      WindowDraggingState::kNoDrag;

  aura::Window* dragged_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SplitViewDragIndicatorsView);
};

SplitViewDragIndicators::SplitViewDragIndicators(aura::Window* root_window) {
  widget_ = CreateWidget(root_window);
  widget_->SetBounds(GetWorkAreaBoundsNoOverlapWithShelf(root_window));
  indicators_view_ =
      widget_->SetContentsView(std::make_unique<SplitViewDragIndicatorsView>());
  widget_->Show();
}

SplitViewDragIndicators::~SplitViewDragIndicators() {
  // Allow some extra time for animations to finish.
  aura::Window* window = widget_->GetNativeWindow();
  if (window == nullptr)
    return;
  wm::SetWindowVisibilityAnimationType(
      window, WINDOW_VISIBILITY_ANIMATION_TYPE_STEP_END);
  AnimateOnChildWindowVisibilityChanged(window, /*visible=*/false);
}

void SplitViewDragIndicators::SetDraggedWindow(aura::Window* dragged_window) {
  DCHECK_EQ(WindowDraggingState::kNoDrag, current_window_dragging_state_);
  indicators_view_->SetDraggedWindow(dragged_window);
}

void SplitViewDragIndicators::SetWindowDraggingState(
    WindowDraggingState window_dragging_state) {
  if (window_dragging_state == current_window_dragging_state_)
    return;
  current_window_dragging_state_ = window_dragging_state;
  indicators_view_->OnWindowDraggingStateChanged(window_dragging_state);
}

void SplitViewDragIndicators::OnDisplayBoundsChanged() {
  aura::Window* root_window = widget_->GetNativeView()->GetRootWindow();
  widget_->SetBounds(GetWorkAreaBoundsNoOverlapWithShelf(root_window));
}

bool SplitViewDragIndicators::GetIndicatorTypeVisibilityForTesting(
    IndicatorType type) const {
  return indicators_view_->GetViewForIndicatorType(type)->layer()->opacity() >
         0.f;
}

gfx::Rect SplitViewDragIndicators::GetLeftHighlightViewBounds() const {
  return indicators_view_->left_highlight_view()->bounds();
}

}  // namespace ash
