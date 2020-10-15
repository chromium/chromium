// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_highlight_view.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"
#include "ash/style/default_color_constants.h"
#include "ash/style/default_colors.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

// The amount of round applied to the corners of the highlight views.
constexpr gfx::RoundedCornersF kHighlightScreenRoundRectRadii(4.f);

// Self deleting animation observer that removes clipping on View's layer and
// optionally sets bounds after the animation ends.
class ClippingObserver : public ui::ImplicitAnimationObserver,
                         public views::ViewObserver {
 public:
  ClippingObserver(views::View* view, base::Optional<gfx::Rect> bounds)
      : view_(view), bounds_(bounds) {
    view_->AddObserver(this);
  }
  ~ClippingObserver() override { view_->RemoveObserver(this); }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    view_->layer()->SetClipRect(gfx::Rect());
    if (bounds_)
      view_->SetBoundsRect(*bounds_);
    delete this;
  }

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override {
    DCHECK_EQ(view_, observed_view);
    delete this;
  }

 private:
  views::View* const view_;
  base::Optional<gfx::Rect> bounds_;
};

}  // namespace

SplitViewHighlightView::SplitViewHighlightView(bool is_right_or_bottom)
    : is_right_or_bottom_(is_right_or_bottom) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetColor(
      DeprecatedGetBackgroundColor(kSplitviewHighlightViewBackgroundColor));
  layer()->SetRoundedCornerRadius(kHighlightScreenRoundRectRadii);
  layer()->SetIsFastRoundedCorner(true);
}

SplitViewHighlightView::~SplitViewHighlightView() = default;

void SplitViewHighlightView::SetBounds(
    const gfx::Rect& bounds,
    const base::Optional<SplitviewAnimationType>& animation_type) {
  if (bounds == this->bounds())
    return;

  if (!animation_type) {
    SetBoundsRect(bounds);
    return;
  }

  const gfx::Rect old_bounds = this->bounds();
  // Note: This is passed on the assumption that the highlights either.
  // 1) Slide out - x or y increases and other dimension stays the same.
  // 2) Slide in - x or y decreases and other dimension stays the same.
  // 3) Expands(Nix inset) - x and y both increase by a small amount.
  const bool grows = bounds.size().GetArea() > old_bounds.size().GetArea();

  // If the highlight grows, set the final bounds and clip the rect to the
  // current bounds and animate. Otherwise, start the clip animation and set the
  // bounds after the animation is complete.
  if (grows)
    SetBoundsRect(bounds);

  // The origin of the clip rect needs to be shifted depending on whether we are
  // growing or shrinking for right/bottom views since their animations are
  // mirrored.
  gfx::Point start_origin, end_origin;
  const bool nix_animation =
      *animation_type == SPLITVIEW_ANIMATION_PREVIEW_AREA_NIX_INSET;
  if (is_right_or_bottom_ || nix_animation) {
    gfx::Vector2d clip_offset = bounds.origin() - old_bounds.origin();

    // RTL is a special case since for the right highlight we will receive a
    // mirrored rect whose origin will not change. In this case the clip rect
    // offset should be the change in width. Portrait mode does not care since
    // it is unaffected by RTL and the nix inset animation will supply the
    // current bounds offset.
    if (base::i18n::IsRTL() && SplitViewController::IsLayoutHorizontal() &&
        !nix_animation) {
      clip_offset = gfx::Vector2d(bounds.width() - old_bounds.width(), 0);
    }

    clip_offset.set_x(std::abs(clip_offset.x()));
    clip_offset.set_y(std::abs(clip_offset.y()));
    if (grows)
      start_origin += clip_offset;
    else
      end_origin += clip_offset;
  }

  layer()->SetClipRect(gfx::Rect(start_origin, old_bounds.size()));
  DoSplitviewClipRectAnimation(
      layer(), *animation_type, gfx::Rect(end_origin, bounds.size()),
      std::make_unique<ClippingObserver>(
          this, grows ? base::nullopt : base::make_optional(bounds)));
}

void SplitViewHighlightView::OnWindowDraggingStateChanged(
    SplitViewDragIndicators::WindowDraggingState window_dragging_state,
    SplitViewDragIndicators::WindowDraggingState previous_window_dragging_state,
    bool previews_only,
    bool can_dragged_window_be_snapped) {
  // No top indicator for dragging from the top in portrait orientation.
  if (window_dragging_state ==
          SplitViewDragIndicators::WindowDraggingState::kFromTop &&
      !IsCurrentScreenOrientationLandscape() && !is_right_or_bottom_) {
    return;
  }

  if (window_dragging_state ==
      SplitViewDragIndicators::WindowDraggingState::kOtherDisplay) {
    DoSplitviewOpacityAnimation(layer(),
                                SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT);
    return;
  }

  const SplitViewController::SnapPosition preview_position =
      SplitViewDragIndicators::GetSnapPosition(window_dragging_state);
  const SplitViewController::SnapPosition previous_preview_position =
      SplitViewDragIndicators::GetSnapPosition(previous_window_dragging_state);

  if (window_dragging_state ==
      SplitViewDragIndicators::WindowDraggingState::kNoDrag) {
    if (previous_preview_position == SplitViewController::NONE) {
      DoSplitviewOpacityAnimation(layer(),
                                  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT);
      return;
    }
    if (is_right_or_bottom_ !=
        SplitViewController::IsPhysicalLeftOrTop(previous_preview_position)) {
      DoSplitviewOpacityAnimation(layer(),
                                  SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_OUT);
    }
    return;
  }

  layer()->SetColor(DeprecatedGetBackgroundColor(
      can_dragged_window_be_snapped
          ? kSplitviewHighlightViewBackgroundColor
          : kSplitviewHighlightViewBackgroundCannotSnapColor));

  if (preview_position != SplitViewController::NONE) {
    DoSplitviewOpacityAnimation(
        layer(),
        is_right_or_bottom_ !=
                SplitViewController::IsPhysicalLeftOrTop(preview_position)
            ? SPLITVIEW_ANIMATION_PREVIEW_AREA_FADE_IN
            : SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT);
    return;
  }

  if (previous_preview_position != SplitViewController::NONE) {
    // There was a snap preview showing, but now the user has dragged away from
    // the edge of the screen, so that the preview should go away.
    if (is_right_or_bottom_ !=
        SplitViewController::IsPhysicalLeftOrTop(previous_preview_position)) {
      // This code is for the preview. If |previews_only|, just fade out. Else
      // fade in from |kPreviewAreaHighlightOpacity| to |kHighlightOpacity|.
      DoSplitviewOpacityAnimation(
          layer(),
          previews_only
              ? SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT
              : can_dragged_window_be_snapped
                    ? SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN
                    : SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN_CANNOT_SNAP);
    } else {
      // This code is for the other highlight. If |previews_only|, just stay
      // hidden (in other words, do nothing). Else fade in.
      DCHECK_EQ(0.f, layer()->GetTargetOpacity());
      if (!previews_only) {
        DoSplitviewOpacityAnimation(
            layer(),
            can_dragged_window_be_snapped
                ? SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN
                : SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN_CANNOT_SNAP);
      }
    }
    return;
  }

  // The drag just started or came in from another display, and is not currently
  // in a snap area. If |previews_only|, there is nothing to do. Else fade in.
  DCHECK_EQ(0.f, layer()->GetTargetOpacity());
  if (!previews_only) {
    DoSplitviewOpacityAnimation(
        layer(), can_dragged_window_be_snapped
                     ? SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN
                     : SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN_CANNOT_SNAP);
    return;
  }
}

}  // namespace ash
