// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_big_user_view.h"

#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/login_constants.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/logging.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

bool IsPublicAccountUser(const LoginUserInfo& user) {
  return user.basic_user_info.type == user_manager::UserType::kPublicAccount;
}

// Returns true if either a or b have a value, but not both.
bool OnlyOneSet(views::View* a, views::View* b) {
  return !!a ^ !!b;
}

}  // namespace

LoginBigUserView::TestApi::TestApi(LoginBigUserView* view) : view_(view) {}

LoginBigUserView::TestApi::~TestApi() = default;

void LoginBigUserView::TestApi::Remove() {
  view_->auth_user_callbacks_.on_remove.Run();
}

LoginBigUserView::LoginBigUserView(
    const LoginUserInfo& user,
    const LoginAuthUserView::Callbacks& auth_user_callbacks,
    const LoginPublicAccountUserView::Callbacks& public_account_callbacks)
    : auth_user_callbacks_(auth_user_callbacks),
      public_account_callbacks_(public_account_callbacks) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Creates either |auth_user_| or |public_account_|.
  CreateChildView(user);

  observation_.Observe(Shell::Get()->wallpaper_controller());
}

LoginBigUserView::~LoginBigUserView() = default;

void LoginBigUserView::CreateChildView(const LoginUserInfo& user) {
  if (IsPublicAccountUser(user)) {
    CreatePublicAccount(user);
  } else {
    CreateAuthUser(user);
  }
}

void LoginBigUserView::UpdateForUser(const LoginUserInfo& user) {
  // Rebuild child view for the following swap case:
  // 1. Public Account -> Auth User
  // 2. Auth User      -> Public Account
  if (IsPublicAccountUser(user) != IsPublicAccountUser(GetCurrentUser())) {
    if (Shell::Get()->login_screen_controller()->IsAuthenticating()) {
      // TODO(b/276246832): We should avoid re-layouting during Authentication.
      LOG(WARNING)
          << "LoginBigUserView::UpdateForUser called during Authentication.";
    }
    CreateChildView(user);
  }

  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    public_account_->UpdateForUser(user);
  }
  if (auth_user_) {
    auth_user_->UpdateForUser(user);
  }
}

const LoginUserInfo& LoginBigUserView::GetCurrentUser() const {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    return public_account_->current_user();
  }
  return auth_user_->current_user();
}

LoginUserView* LoginBigUserView::GetUserView() {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    return public_account_->user_view();
  }
  return auth_user_->user_view();
}

bool LoginBigUserView::IsAuthEnabled() const {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    return public_account_->auth_enabled();
  }
  return auth_user_->auth_methods() != LoginAuthUserView::AUTH_NONE;
}

void LoginBigUserView::RequestFocus() {
  DCHECK(OnlyOneSet(public_account_, auth_user_));
  if (public_account_) {
    return public_account_->RequestFocus();
  }
  return auth_user_->RequestFocus();
}

void LoginBigUserView::OnWallpaperBlurChanged() {
  if (Shell::Get()->wallpaper_controller()->IsWallpaperBlurredForLockState() ||
      Shell::Get()->session_controller()->GetSessionState() !=
          session_manager::SessionState::LOCKED) {
    SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);
    SetBackground(nullptr);
  } else {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    const ui::ColorId background_color_id = cros_tokens::kCrosSysScrim2;
    SetBackground(views::CreateThemedRoundedRectBackground(
        background_color_id, login::kNonBlurredWallpaperBackgroundRadiusDp, 0));
  }
}

void LoginBigUserView::CreateAuthUser(const LoginUserInfo& user) {
  DCHECK(!IsPublicAccountUser(user));
  DCHECK(!auth_user_);

  auth_user_ = new LoginAuthUserView(user, auth_user_callbacks_);
  delete public_account_;
  public_account_ = nullptr;
  AddChildView(auth_user_.get());
}

void LoginBigUserView::CreatePublicAccount(const LoginUserInfo& user) {
  DCHECK(IsPublicAccountUser(user));
  DCHECK(!public_account_);

  public_account_ =
      new LoginPublicAccountUserView(user, public_account_callbacks_);
  delete auth_user_;
  auth_user_ = nullptr;
  AddChildView(public_account_.get());
}

BEGIN_METADATA(LoginBigUserView)
END_METADATA

}  // namespace ash
