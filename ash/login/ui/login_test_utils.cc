// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_utils.h"
#include "ash/login/ui/login_big_user_view.h"
#include "base/strings/string_split.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {
constexpr char kPrimaryName[] = "primary";
constexpr char kSecondaryName[] = "secondary";
}  // namespace

const char* AuthTargetToString(AuthTarget target) {
  switch (target) {
    case AuthTarget::kPrimary:
      return kPrimaryName;
    case AuthTarget::kSecondary:
      return kSecondaryName;
  }
  NOTREACHED();
  return "";
}

LockContentsView::TestApi MakeLockContentsViewTestApi(LockContentsView* view) {
  return LockContentsView::TestApi(view);
}

LoginAuthUserView::TestApi MakeLoginAuthTestApi(LockContentsView* view,
                                                AuthTarget target) {
  switch (target) {
    case AuthTarget::kPrimary:
      return LoginAuthUserView::TestApi(
          MakeLockContentsViewTestApi(view).primary_big_view()->auth_user());
    case AuthTarget::kSecondary:
      return LoginAuthUserView::TestApi(MakeLockContentsViewTestApi(view)
                                            .opt_secondary_big_view()
                                            ->auth_user());
  }

  NOTREACHED();
}

LoginPasswordView::TestApi MakeLoginPasswordTestApi(LockContentsView* view,
                                                    AuthTarget target) {
  return LoginPasswordView::TestApi(
      MakeLoginAuthTestApi(view, target).password_view());
}

mojom::LoginUserInfoPtr CreateUser(const std::string& email) {
  auto user = mojom::LoginUserInfo::New();
  user->basic_user_info = mojom::UserInfo::New();
  user->basic_user_info->avatar = mojom::UserAvatar::New();
  user->basic_user_info->account_id = AccountId::FromUserEmail(email);
  user->basic_user_info->display_name = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)[0];
  user->basic_user_info->display_email = email;
  return user;
}

mojom::LoginUserInfoPtr CreatePublicAccountUser(const std::string& email) {
  auto user = mojom::LoginUserInfo::New();
  user->basic_user_info = mojom::UserInfo::New();
  std::vector<std::string> email_parts = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  user->basic_user_info->avatar = mojom::UserAvatar::New();
  user->basic_user_info->account_id = AccountId::FromUserEmail(email);
  user->basic_user_info->display_name = email_parts[0];
  user->basic_user_info->display_email = email;
  user->basic_user_info->type = user_manager::USER_TYPE_PUBLIC_ACCOUNT;
  user->public_account_info = ash::mojom::PublicAccountInfo::New();
  user->public_account_info->enterprise_domain = email_parts[1];
  user->public_account_info->show_expanded_view = true;
  return user;
}

bool HasFocusInAnyChildView(views::View* view) {
  if (view->HasFocus())
    return true;
  for (int i = 0; i < view->child_count(); ++i) {
    if (HasFocusInAnyChildView(view->child_at(i)))
      return true;
  }
  return false;
}

bool TabThroughView(ui::test::EventGenerator* event_generator,
                    views::View* view,
                    bool reverse) {
  if (!HasFocusInAnyChildView(view)) {
    ADD_FAILURE() << "View not focused initially.";
    return false;
  }

  for (int i = 0; i < 50; ++i) {
    event_generator->PressKey(ui::KeyboardCode::VKEY_TAB,
                              reverse ? ui::EF_SHIFT_DOWN : 0);
    if (!HasFocusInAnyChildView(view))
      return true;
  }

  return false;
}

}  // namespace ash
