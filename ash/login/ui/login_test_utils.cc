// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_test_utils.h"

#include "ash/login/ui/login_big_user_view.h"
#include "base/containers/adapters.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace {
constexpr char kPrimaryName[] = "primary";
constexpr char kSecondaryName[] = "secondary";

LoginUserInfo CreateUserWithType(const std::string& email,
                                 user_manager::UserType user_type) {
  LoginUserInfo user;
  user.basic_user_info.type = user_type;
  user.basic_user_info.account_id = AccountId::FromUserEmail(email);
  user.basic_user_info.display_name = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)[0];
  user.basic_user_info.display_email = email;
  return user;
}

}  // namespace

const char* AuthTargetToString(AuthTarget target) {
  switch (target) {
    case AuthTarget::kPrimary:
      return kPrimaryName;
    case AuthTarget::kSecondary:
      return kSecondaryName;
  }
  NOTREACHED();
}

LockContentsViewTestApi MakeLockContentsViewTestApi(LockContentsView* view) {
  return LockContentsViewTestApi(view);
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

LoginUserInfo CreateUser(const std::string& email) {
  return CreateUserWithType(email, user_manager::UserType::kRegular);
}

LoginUserInfo CreateChildUser(const std::string& email) {
  return CreateUserWithType(email, user_manager::UserType::kChild);
}

LoginUserInfo CreatePublicAccountUser(const std::string& email) {
  LoginUserInfo user;
  std::vector<std::string> email_parts = base::SplitString(
      email, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  user.basic_user_info.account_id = AccountId::FromUserEmail(email);
  user.basic_user_info.display_name = email_parts[0];
  user.basic_user_info.display_email = email;
  user.basic_user_info.type = user_manager::UserType::kPublicAccount;
  user.public_account_info.emplace();
  user.public_account_info->device_enterprise_manager = email_parts[1];
  user.public_account_info->show_expanded_view = true;
  return user;
}

bool HasFocusInAnyChildView(const views::View* view) {
  return view->HasFocus() ||
         base::ranges::any_of(view->children(), [](const views::View* v) {
           return HasFocusInAnyChildView(v);
         });
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
    if (!HasFocusInAnyChildView(view)) {
      return true;
    }
  }

  return false;
}

// Performs a DFS for the first button in the views hierarchy
// The last child is on the top of the z layer stack
views::View* FindTopButton(views::View* current_view) {
  for (views::View* child : base::Reversed(current_view->children())) {
    if (views::Button::AsButton(child)) {
      return child;
    }
    if (!child->children().empty()) {
      views::View* child_button = FindTopButton(child);
      if (child_button) {
        return child_button;
      }
    }
  }
  return nullptr;
}

}  // namespace ash
