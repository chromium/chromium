// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/scrollable_users_list_view.h"

#include <memory>
#include <optional>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/box_layout.h"

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

// Inset the scroll bar from the edges of the screen.
constexpr auto kVerticalScrollInsets = gfx::Insets::TLBR(2, 0, 2, 8);

constexpr char kScrollableUsersListContentViewName[] =
    "ScrollableUsersListContent";

// A view that is at least as tall as its parent.
class EnsureMinHeightView : public NonAccessibleView {
 public:
  EnsureMinHeightView()
      : NonAccessibleView(kScrollableUsersListContentViewName) {}

  EnsureMinHeightView(const EnsureMinHeightView&) = delete;
  EnsureMinHeightView& operator=(const EnsureMinHeightView&) = delete;

  ~EnsureMinHeightView() override = default;

  // NonAccessibleView:
  void Layout(PassKey) override {
    // Make sure our height is at least as tall as the parent, so the layout
    // manager will center us properly.
    int min_height = parent()->height();
    if (size().height() < min_height) {
      gfx::Size new_size = size();
      new_size.set_height(min_height);
      SetSize(new_size);
    }
    LayoutSuperclass<NonAccessibleView>(this);
  }
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
          gfx::Insets::TLBR(kExtraSmallPaddingTopBottomOfUserListPortraitDp,
                            kExtraSmallPaddingLeftOfUserListPortraitDp,
                            kExtraSmallPaddingTopBottomOfUserListPortraitDp,
                            kExtraSmallPaddingRightOfUserListPortraitDp);
      return params;
    }
    case LoginDisplayStyle::kSmall: {
      LayoutParams params;
      params.insets_landscape =
          gfx::Insets::VH(kSmallPaddingTopBottomOfUserListDp,
                          kSmallPaddingLeftRightOfUserListDp);
      params.insets_portrait =
          gfx::Insets::VH(kSmallPaddingTopBottomOfUserListDp,
                          kSmallPaddingLeftRightOfUserListDp);
      params.between_child_spacing = kSmallVerticalDistanceBetweenUsersDp;
      return params;
    }
    default: {
      NOTREACHED();
    }
  }
}

}  // namespace

// static
ScrollableUsersListView::GradientParams
ScrollableUsersListView::GradientParams::BuildForStyle(LoginDisplayStyle style,
                                                       views::View* view) {
  switch (style) {
    case LoginDisplayStyle::kExtraSmall: {
      SkColor dark_muted_color = view->GetColorProvider()->GetColor(
          kColorAshLoginScrollableUserListBackground);

      ui::ColorId tint_color_id = cros_tokens::kCrosSysScrim2;

      SkColor tint_color = color_utils::GetResultingPaintColor(
          view->GetColorProvider()->GetColor(tint_color_id),
          SkColorSetA(dark_muted_color, SK_AlphaOPAQUE));

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
    }
  }
}

ScrollableUsersListView::TestApi::TestApi(ScrollableUsersListView* view)
    : view_(view) {}

ScrollableUsersListView::TestApi::~TestApi() = default;

const std::vector<raw_ptr<LoginUserView, VectorExperimental>>&
ScrollableUsersListView::TestApi::user_views() const {
  return view_->user_views_;
}

ScrollableUsersListView::ScrollableUsersListView(
    const std::vector<LoginUserInfo>& users,
    const ActionWithUser& on_tap_user,
    LoginDisplayStyle display_style)
    : display_style_(display_style) {
  auto layout_params = BuildLayoutForStyle(display_style);

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
    auto* view = new LoginUserView(display_style, false /*show_dropdown*/,
                                   base::BindRepeating(on_tap_user, i - 1),
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
  auto* ensure_min_height_layout =
      ensure_min_height->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  ensure_min_height_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  ensure_min_height_layout->set_cross_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  ensure_min_height->AddChildView(user_view_host_.get());
  SetContents(std::move(ensure_min_height));
  SetBackgroundColor(std::nullopt);
  SetDrawOverflowIndicator(false);

  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  SetVerticalScrollBar(std::move(vertical_scroll));
  SetHorizontalScrollBar(std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kHorizontal));

  observation_.Observe(Shell::Get()->wallpaper_controller());
}

ScrollableUsersListView::~ScrollableUsersListView() = default;

LoginUserView* ScrollableUsersListView::GetUserView(
    const AccountId& account_id) {
  for (ash::LoginUserView* view : user_views_) {
    if (view->current_user().basic_user_info.account_id == account_id) {
      return view;
    }
  }
  return nullptr;
}

void ScrollableUsersListView::UpdateUserViewHostLayoutInsets() {
  DCHECK(GetWidget());
  bool should_show_landscape =
      login_views_utils::ShouldShowLandscape(GetWidget());
  LayoutParams layout_params = BuildLayoutForStyle(display_style_);
  user_view_host_layout_->set_inside_border_insets(
      should_show_landscape ? layout_params.insets_landscape
                            : layout_params.insets_portrait);
}

void ScrollableUsersListView::Layout(PassKey) {
  DCHECK(user_view_host_layout_);

  // Update clipping height.
  if (parent()) {
    int parent_height = parent()->size().height();
    ClipHeightTo(parent_height, parent_height);
    if (height() != parent_height) {
      PreferredSizeChanged();
    }
  }

  UpdateUserViewHostLayoutInsets();

  // Layout everything.
  LayoutSuperclass<ScrollView>(this);
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
  if (Shell::Get()->wallpaper_controller()->IsWallpaperBlurredForLockState()) {
    cc::PaintFlags flags;

    // Only draw a gradient if the content can be scrolled.
    if (vertical_scroll_bar()->GetVisible()) {
      // Draws symmetrical linear gradient at the top and bottom of the view.
      SkScalar view_height = render_bounds.height();
      SkScalar gradient_height = gradient_params_.height;
      if (gradient_height == 0) {
        gradient_height = view_height;
      }

      // Start and end point of the drawing in view space.
      SkPoint in_view_coordinates[2] = {SkPoint(),
                                        SkPoint::Make(0.f, view_height)};
      // Positions of colors to create gradient define in 0 to 1 range.
      SkScalar top_gradient_end = gradient_height / view_height;
      SkScalar bottom_gradient_start = 1.f - top_gradient_end;
      SkScalar color_positions[4] = {0.f, top_gradient_end,
                                     bottom_gradient_start, 1.f};
      SkColor4f colors[4] = {SkColor4f::FromColor(gradient_params_.color_from),
                             SkColor4f::FromColor(gradient_params_.color_to),
                             SkColor4f::FromColor(gradient_params_.color_to),
                             SkColor4f::FromColor(gradient_params_.color_from)};

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

    ui::ColorId background_color_id = cros_tokens::kCrosSysScrim2;
    flags.setColor(GetColorProvider()->GetColor(background_color_id));
    canvas->DrawRoundRect(render_bounds,
                          login::kNonBlurredWallpaperBackgroundRadiusDp, flags);
  }
}

void ScrollableUsersListView::OnThemeChanged() {
  views::ScrollView::OnThemeChanged();
  gradient_params_ = GradientParams::BuildForStyle(display_style_, this);
}

// When the active user is updated, the wallpaper changes. The gradient color
// should be updated in response to the new primary wallpaper color.
void ScrollableUsersListView::OnWallpaperColorsChanged() {
  gradient_params_ = GradientParams::BuildForStyle(display_style_, this);
  SchedulePaint();
}

void ScrollableUsersListView::OnWallpaperBlurChanged() {
  gradient_params_ = GradientParams::BuildForStyle(display_style_, this);
  SchedulePaint();
}

BEGIN_METADATA(ScrollableUsersListView)
END_METADATA

}  // namespace ash
