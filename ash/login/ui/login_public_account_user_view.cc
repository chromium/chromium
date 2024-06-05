// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_public_account_user_view.h"

#include <memory>

#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/views_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

constexpr char kLoginPublicAccountUserViewClassName[] =
    "LoginPublicAccountUserView";

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 24;

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromUserViewToArrowButton = 44;

// Distance from the end of arrow button to the bottom of the big user view.
constexpr int kDistanceFromArrowButtonToBigUserViewBottom = 20;

constexpr int kArrowButtonSizeDp = 48;

// Non-empty width, useful for debugging/visualization.
constexpr int kNonEmptyWidth = 1;

constexpr int kArrowButtonFadeAnimationDurationMs = 180;

}  // namespace

LoginPublicAccountUserView::TestApi::TestApi(LoginPublicAccountUserView* view)
    : view_(view) {}

LoginPublicAccountUserView::TestApi::~TestApi() = default;

views::View* LoginPublicAccountUserView::TestApi::arrow_button() const {
  return view_->arrow_button_;
}

LoginPublicAccountUserView::Callbacks::Callbacks() = default;

LoginPublicAccountUserView::Callbacks::Callbacks(const Callbacks& other) =
    default;

LoginPublicAccountUserView::Callbacks::~Callbacks() = default;

LoginPublicAccountUserView::LoginPublicAccountUserView(
    const LoginUserInfo& user,
    const Callbacks& callbacks)
    : NonAccessibleView(kLoginPublicAccountUserViewClassName),
      on_tap_(callbacks.on_tap),
      on_public_account_tap_(callbacks.on_public_account_tapped) {
  DCHECK_EQ(user.basic_user_info.type, user_manager::UserType::kPublicAccount);
  DCHECK(callbacks.on_tap);
  DCHECK(callbacks.on_public_account_tapped);

  auto user_view = std::make_unique<LoginUserView>(
      LoginDisplayStyle::kLarge, false /*show_dropdown*/,
      base::BindRepeating(&LoginPublicAccountUserView::OnUserViewTap,
                          base::Unretained(this)),
      base::RepeatingClosure());
  auto arrow_button = std::make_unique<ArrowButtonView>(
      base::BindRepeating(&LoginPublicAccountUserView::ArrowButtonPressed,
                          base::Unretained(this)),
      kArrowButtonSizeDp);
  std::string display_name = user.basic_user_info.display_name;
  // display_name can be empty in debug builds with stub users.
  if (display_name.empty()) {
    display_name = user.basic_user_info.display_email;
  }
  arrow_button->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_PUBLIC_ACCOUNT_DIALOG_BUTTON_ACCESSIBLE_NAME,
      base::UTF8ToUTF16(display_name)));
  arrow_button->SetFocusPainter(nullptr);

  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  SetLayoutManagerUseConstrainedSpace(false);
  // build layout for public account.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  user_view_ = user_view.get();
  auto wrapped_user_view =
      login_views_utils::WrapViewForPreferredSize(std::move(user_view));

  auto add_padding = [&](int amount) {
    auto* padding = new NonAccessibleView();
    padding->SetPreferredSize(gfx::Size(kNonEmptyWidth, amount));
    AddChildView(padding);
  };

  add_padding(kDistanceFromTopOfBigUserViewToUserIconDp);
  AddChildView(std::move(wrapped_user_view));
  add_padding(kDistanceFromUserViewToArrowButton);

  // Arrow button size should be its preferred size so we wrap it.
  auto* arrow_button_container =
      AddChildView(std::make_unique<NonAccessibleView>());
  auto container_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  arrow_button_container->SetLayoutManager(std::move(container_layout));
  arrow_button_ = arrow_button_container->AddChildView(std::move(arrow_button));

  add_padding(kDistanceFromArrowButtonToBigUserViewBottom);

  // Update authentication UI.
  SetAuthEnabled(auth_enabled_, false /*animate*/);
  user_view_->UpdateForUser(user, false /*animate*/);

  hover_notifier_ = std::make_unique<HoverNotifier>(
      this, base::BindRepeating(&LoginPublicAccountUserView::OnHover,
                                base::Unretained(this)));
}

LoginPublicAccountUserView::~LoginPublicAccountUserView() = default;

void LoginPublicAccountUserView::SetAuthEnabled(bool enabled, bool animate) {
  auth_enabled_ = enabled;
  ignore_hover_ = enabled;
  UpdateArrowButtonOpacity(enabled ? 1 : 0, animate);
  arrow_button_->SetFocusBehavior(enabled ? FocusBehavior::ALWAYS
                                          : FocusBehavior::NEVER);

  // Only the active auth user view has auth enabled. If that is the
  // case, then render the user view as if it was always focused, since clicking
  // on it will not do anything (such as swapping users).
  user_view_->SetForceOpaque(enabled);
  user_view_->SetTapEnabled(!enabled);
  if (enabled) {
    arrow_button_->RequestFocus();
  }

  PreferredSizeChanged();
}

void LoginPublicAccountUserView::UpdateForUser(const LoginUserInfo& user) {
  user_view_->UpdateForUser(user, true /*animate*/);
}

const LoginUserInfo& LoginPublicAccountUserView::current_user() const {
  return user_view_->current_user();
}

gfx::Size LoginPublicAccountUserView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::View::CalculatePreferredSize(available_size);
  // Make sure we are at least as big as the user view. If we do not do this the
  // view will be below minimum size when no auth methods are displayed.
  size.SetToMax(user_view_->GetPreferredSize());
  return size;
}

void LoginPublicAccountUserView::ArrowButtonPressed() {
  DCHECK(arrow_button_);
  // If the pod isn't active, activate it first.
  if (!auth_enabled_) {
    OnUserViewTap();
  }

  DCHECK(auth_enabled_);
  on_public_account_tap_.Run();
}

void LoginPublicAccountUserView::OnUserViewTap() {
  on_tap_.Run();
}

void LoginPublicAccountUserView::OnHover(bool has_hover) {
  if (!ignore_hover_) {
    UpdateArrowButtonOpacity(has_hover ? 1 : 0, true /*animate*/);
  }
}

void LoginPublicAccountUserView::UpdateArrowButtonOpacity(float target_opacity,
                                                          bool animate) {
  if (!animate) {
    arrow_button_->layer()->SetOpacity(target_opacity);
    return;
  }

  // Set up animation.
  {
    ui::ScopedLayerAnimationSettings settings(
        arrow_button_->layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::Milliseconds(kArrowButtonFadeAnimationDurationMs));
    settings.SetTweenType(gfx::Tween::EASE_IN_OUT);

    arrow_button_->layer()->SetOpacity(target_opacity);
  }
}

BEGIN_METADATA(LoginPublicAccountUserView)
END_METADATA

}  // namespace ash
