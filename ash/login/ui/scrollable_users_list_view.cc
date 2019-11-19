// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/scrollable_users_list_view.h"

#include <limits>
#include <memory>

#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/numerics/ranges.h"
#include "base/timer/timer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Vertical padding between user rows in the small display style.
constexpr int kSmallVerticalDistanceBetweenUsersDp = 53;

// Padding around user list.
constexpr int kSmallPaddingLeftRightOfUserListDp = 45;
constexpr int kSmallPaddingTopBottomOfUserListDp = 60;
constexpr int kExtraSmallPaddingAroundUserListLandscapeDp = 72;
constexpr int kExtraSmallPaddingLeftOfUserListPortraitDp = 46;
constexpr int kExtraSmallPaddingRightOfUserListPortraitDp = 12;
constexpr int kExtraSmallPaddingTopBottomOfUserListPortraitDp = 66;

// Vertical padding between user rows in extra small display style.
constexpr int kExtraSmallVerticalDistanceBetweenUsersDp = 32;

// Height of gradient shown at the top/bottom of the user list in the extra
// small display style.
constexpr int kExtraSmallGradientHeightDp = 112;

// Thickness of scroll bar thumb.
constexpr int kScrollThumbThicknessDp = 6;
// Padding on the right of scroll bar thumb.
constexpr int kScrollThumbPaddingDp = 8;
// Radius of the scroll bar thumb.
constexpr int kScrollThumbRadiusDp = 8;
// Alpha of scroll bar thumb (43/255 = 17%).
constexpr int kScrollThumbAlpha = 43;
// How long for the scrollbar to hide after no scroll events have been received?
constexpr base::TimeDelta kScrollThumbHideTimeout =
    base::TimeDelta::FromMilliseconds(500);
// How long for the scrollbar to fade away?
constexpr base::TimeDelta kScrollThumbFadeDuration =
    base::TimeDelta::FromMilliseconds(240);

constexpr char kScrollableUsersListContentViewName[] =
    "ScrollableUsersListContent";

// A view that is at least as tall as its parent.
class EnsureMinHeightView : public NonAccessibleView {
 public:
  EnsureMinHeightView()
      : NonAccessibleView(kScrollableUsersListContentViewName) {}
  ~EnsureMinHeightView() override = default;

  // NonAccessibleView:
  void Layout() override {
    // Make sure our height is at least as tall as the parent, so the layout
    // manager will center us properly.
    int min_height = parent()->height();
    if (size().height() < min_height) {
      gfx::Size new_size = size();
      new_size.set_height(min_height);
      SetSize(new_size);
    }
    NonAccessibleView::Layout();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EnsureMinHeightView);
};

class ScrollBarThumb : public views::BaseScrollBarThumb {
 public:
  explicit ScrollBarThumb(views::ScrollBar* scroll_bar)
      : BaseScrollBarThumb(scroll_bar) {}
  ~ScrollBarThumb() override = default;

  // views::BaseScrollBarThumb:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kScrollThumbThicknessDp, kScrollThumbThicknessDp);
  }
  void OnPaint(gfx::Canvas* canvas) override {
    cc::PaintFlags fill_flags;
    fill_flags.setStyle(cc::PaintFlags::kFill_Style);
    fill_flags.setColor(SkColorSetA(SK_ColorWHITE, kScrollThumbAlpha));
    canvas->DrawRoundRect(GetLocalBounds(), kScrollThumbRadiusDp, fill_flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollBarThumb);
};

struct LayoutParams {
  // Spacing between user entries on users list.
  int between_child_spacing;
  // Insets around users list used in landscape orientation.
  gfx::Insets insets_landscape;
  // Insets around users list used in portrait orientation.
  gfx::Insets insets_portrait;
};

// static
LayoutParams BuildLayoutForStyle(LoginDisplayStyle style) {
  switch (style) {
    case LoginDisplayStyle::kExtraSmall: {
      LayoutParams params;
      params.between_child_spacing = kExtraSmallVerticalDistanceBetweenUsersDp;
      params.insets_landscape =
          gfx::Insets(kExtraSmallPaddingAroundUserListLandscapeDp);
      params.insets_portrait =
          gfx::Insets(kExtraSmallPaddingTopBottomOfUserListPortraitDp,
                      kExtraSmallPaddingLeftOfUserListPortraitDp,
                      kExtraSmallPaddingTopBottomOfUserListPortraitDp,
                      kExtraSmallPaddingRightOfUserListPortraitDp);
      return params;
    }
    case LoginDisplayStyle::kSmall: {
      LayoutParams params;
      params.insets_landscape = gfx::Insets(kSmallPaddingTopBottomOfUserListDp,
                                            kSmallPaddingLeftRightOfUserListDp);
      params.insets_portrait = gfx::Insets(kSmallPaddingTopBottomOfUserListDp,
                                           kSmallPaddingLeftRightOfUserListDp);
      params.between_child_spacing = kSmallVerticalDistanceBetweenUsersDp;
      return params;
    }
    default: {
      NOTREACHED();
      return LayoutParams();
    }
  }
}

// Shows a scrollbar that automatically displays and hides itself when content
// is scrolled.
class UsersListScrollBar : public views::ScrollBar {
 public:
  explicit UsersListScrollBar(bool horizontal)
      : ScrollBar(horizontal),
        hide_scrollbar_timer_(
            FROM_HERE,
            kScrollThumbHideTimeout,
            base::BindRepeating(&UsersListScrollBar::HideScrollBar,
                                base::Unretained(this))) {
    SetThumb(new ScrollBarThumb(this));
    GetThumb()->SetPaintToLayer();
    GetThumb()->layer()->SetFillsBoundsOpaquely(false);
    // The thumb is hidden by default.
    GetThumb()->layer()->SetOpacity(0);
  }
  ~UsersListScrollBar() override = default;

  // views::ScrollBar:
  gfx::Rect GetTrackBounds() const override { return GetLocalBounds(); }
  bool OverlapsContent() const override { return true; }
  int GetThickness() const override {
    return kScrollThumbThicknessDp + kScrollThumbPaddingDp;
  }
  void OnMouseEntered(const ui::MouseEvent& event) override {
    mouse_over_scrollbar_ = true;
    ShowScrollbar();
  }
  void OnMouseExited(const ui::MouseEvent& event) override {
    mouse_over_scrollbar_ = false;
    if (!hide_scrollbar_timer_.IsRunning())
      hide_scrollbar_timer_.Reset();
  }
  void ScrollToPosition(int position) override {
    ShowScrollbar();
    views::ScrollBar::ScrollToPosition(position);
  }
  void ObserveScrollEvent(const ui::ScrollEvent& event) override {
    // Scroll fling events are generated by moving a single finger over the
    // trackpad; do not show the scrollbar for these events.
    if (event.type() == ui::ET_SCROLL_FLING_CANCEL)
      return;
    ShowScrollbar();
  }

 private:
  void ShowScrollbar() {
    bool currently_hidden =
        base::IsApproximatelyEqual(GetThumb()->layer()->GetTargetOpacity(), 0.f,
                                   std::numeric_limits<float>::epsilon());

    if (!mouse_over_scrollbar_)
      hide_scrollbar_timer_.Reset();

    if (currently_hidden) {
      ui::ScopedLayerAnimationSettings animation(
          GetThumb()->layer()->GetAnimator());
      animation.SetTransitionDuration(kScrollThumbFadeDuration);
      GetThumb()->layer()->SetOpacity(1);
    }
  }

  void HideScrollBar() {
    // Never hide the scrollbar if the mouse is over it. The auto-hide timer
    // will be reset when the mouse leaves the scrollable area.
    if (mouse_over_scrollbar_)
      return;

    hide_scrollbar_timer_.Stop();
    ui::ScopedLayerAnimationSettings animation(
        GetThumb()->layer()->GetAnimator());
    animation.SetTransitionDuration(kScrollThumbFadeDuration);
    GetThumb()->layer()->SetOpacity(0);
  }

  // When the mouse is hovering over the scrollbar, the scrollbar should always
  // be displayed.
  bool mouse_over_scrollbar_ = false;
  // Timer that will start the scrollbar's hiding animation when it reaches 0.
  base::RetainingOneShotTimer hide_scrollbar_timer_;

  DISALLOW_COPY_AND_ASSIGN(UsersListScrollBar);
};

}  // namespace

// static
ScrollableUsersListView::GradientParams
ScrollableUsersListView::GradientParams::BuildForStyle(
    LoginDisplayStyle style) {
  switch (style) {
    case LoginDisplayStyle::kExtraSmall: {
      SkColor dark_muted_color =
          Shell::Get()->wallpaper_controller()->GetProminentColor(
              color_utils::ColorProfile(color_utils::LumaRange::DARK,
                                        color_utils::SaturationRange::MUTED));
      SkColor tint_color = color_utils::GetResultingPaintColor(
          SkColorSetA(login_constants::kDefaultBaseColor,
                      login_constants::kTranslucentColorDarkenAlpha),
          SkColorSetA(dark_muted_color, SK_AlphaOPAQUE));
      tint_color =
          SkColorSetA(tint_color, login_constants::kScrollTranslucentAlpha);

      GradientParams params;
      params.color_from = dark_muted_color;
      params.color_to = tint_color;
      params.height = kExtraSmallGradientHeightDp;
      return params;
    }
    case LoginDisplayStyle::kSmall: {
      GradientParams params;
      params.height = 0.f;
      return params;
    }
    default: {
      NOTREACHED();
      return GradientParams();
    }
  }
}

ScrollableUsersListView::TestApi::TestApi(ScrollableUsersListView* view)
    : view_(view) {}

ScrollableUsersListView::TestApi::~TestApi() = default;

const std::vector<LoginUserView*>&
ScrollableUsersListView::TestApi::user_views() const {
  return view_->user_views_;
}

ScrollableUsersListView::ScrollableUsersListView(
    const std::vector<LoginUserInfo>& users,
    const ActionWithUser& on_tap_user,
    LoginDisplayStyle display_style)
    : display_style_(display_style) {
  auto layout_params = BuildLayoutForStyle(display_style);
  gradient_params_ = GradientParams::BuildForStyle(display_style);

  user_view_host_ = new NonAccessibleView();
  user_view_host_layout_ =
      user_view_host_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          layout_params.between_child_spacing));
  user_view_host_layout_->set_minimum_cross_axis_size(
      LoginUserView::WidthForLayoutStyle(display_style));
  user_view_host_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  user_view_host_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  for (std::size_t i = 1u; i < users.size(); ++i) {
    auto* view = new LoginUserView(
        display_style, false /*show_dropdown*/, false /*show_domain*/,
        base::BindRepeating(on_tap_user, i - 1), base::RepeatingClosure(),
        base::RepeatingClosure());
    user_views_.push_back(view);
    view->UpdateForUser(users[i], false /*animate*/);
    user_view_host_->AddChildView(view);
  }

  // |user_view_host_| is the same size as the user views, which may be shorter
  // than or taller than the display height. We need the exact height of all
  // user views to render a background if the wallpaper is not blurred.
  //
  // |user_view_host_| is a child of |ensure_min_height|, which has a layout
  // manager which will ensure |user_view_host_| is vertically centered if
  // |user_view_host_| is shorter than the display height.
  //
  // |user_view_host_| cannot be set as |contents()| directly because it needs
  // to be vertically centered when non-scrollable.
  auto ensure_min_height = std::make_unique<EnsureMinHeightView>();
  ensure_min_height
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  ensure_min_height->AddChildView(user_view_host_);
  SetContents(std::move(ensure_min_height));
  SetBackgroundColor(SK_ColorTRANSPARENT);
  SetDrawOverflowIndicator(false);

  SetVerticalScrollBar(std::make_unique<UsersListScrollBar>(false));
  SetHorizontalScrollBar(std::make_unique<UsersListScrollBar>(true));

  observer_.Add(Shell::Get()->wallpaper_controller());
}

ScrollableUsersListView::~ScrollableUsersListView() = default;

LoginUserView* ScrollableUsersListView::GetUserView(
    const AccountId& account_id) {
  for (auto* view : user_views_) {
    if (view->current_user().basic_user_info.account_id == account_id)
      return view;
  }
  return nullptr;
}

void ScrollableUsersListView::Layout() {
  DCHECK(user_view_host_layout_);

  // Update clipping height.
  if (parent()) {
    int parent_height = parent()->size().height();
    ClipHeightTo(parent_height, parent_height);
    if (height() != parent_height)
      PreferredSizeChanged();
  }

  // Update the user view layout.
  bool should_show_landscape =
      login_views_utils::ShouldShowLandscape(GetWidget());
  LayoutParams layout_params = BuildLayoutForStyle(display_style_);
  user_view_host_layout_->set_inside_border_insets(
      should_show_landscape ? layout_params.insets_landscape
                            : layout_params.insets_portrait);

  // Layout everything.
  ScrollView::Layout();
}

void ScrollableUsersListView::OnPaintBackground(gfx::Canvas* canvas) {
  // Find the bounds of the actual contents.
  gfx::RectF render_bounds(user_view_host_->GetLocalBounds());
  views::View::ConvertRectToTarget(user_view_host_, this, &render_bounds);

  // In extra-small, the render bounds height always match the display height.
  if (display_style_ == LoginDisplayStyle::kExtraSmall) {
    render_bounds.set_y(0);
    render_bounds.set_height(height());
  }

  // Only draw a gradient if the wallpaper is blurred. Otherwise, draw a rounded
  // rectangle.
  if (Shell::Get()->wallpaper_controller()->IsWallpaperBlurred()) {
    cc::PaintFlags flags;

    // Only draw a gradient if the content can be scrolled.
    if (vertical_scroll_bar()->GetVisible()) {
      // Draws symmetrical linear gradient at the top and bottom of the view.
      SkScalar view_height = render_bounds.height();
      SkScalar gradient_height = gradient_params_.height;
      if (gradient_height == 0)
        gradient_height = view_height;

      // Start and end point of the drawing in view space.
      SkPoint in_view_coordinates[2] = {SkPoint(),
                                        SkPoint::Make(0.f, view_height)};
      // Positions of colors to create gradient define in 0 to 1 range.
      SkScalar top_gradient_end = gradient_height / view_height;
      SkScalar bottom_gradient_start = 1.f - top_gradient_end;
      SkScalar color_positions[4] = {0.f, top_gradient_end,
                                     bottom_gradient_start, 1.f};
      SkColor colors[4] = {gradient_params_.color_from,
                           gradient_params_.color_to, gradient_params_.color_to,
                           gradient_params_.color_from};

      flags.setShader(cc::PaintShader::MakeLinearGradient(
          in_view_coordinates, colors, color_positions, 4, SkTileMode::kClamp));
    } else {
      flags.setColor(gradient_params_.color_to);
    }

    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRect(render_bounds, flags);
  } else {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        SkColorSetA(login_constants::kDefaultBaseColor,
                    login_constants::kNonBlurredWallpaperBackgroundAlpha));
    canvas->DrawRoundRect(
        render_bounds, login_constants::kNonBlurredWallpaperBackgroundRadiusDp,
        flags);
  }
}

// When the active user is updated, the wallpaper changes. The gradient color
// should be updated in response to the new primary wallpaper color.
void ScrollableUsersListView::OnWallpaperColorsChanged() {
  gradient_params_ = GradientParams::BuildForStyle(display_style_);
  SchedulePaint();
}

void ScrollableUsersListView::OnWallpaperBlurChanged() {
  gradient_params_ = GradientParams::BuildForStyle(display_style_);
  SchedulePaint();
}

}  // namespace ash
