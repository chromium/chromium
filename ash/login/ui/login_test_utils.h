// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_TEST_UTILS_H_
#define ASH_LOGIN_UI_LOGIN_TEST_UTILS_H_

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_password_view.h"

namespace ui {
namespace test {
class EventGenerator;
}
}  // namespace ui

namespace ash {

enum class AuthTarget { kPrimary, kSecondary };

// Converts |target| to a string for usage in logging.
const char* AuthTargetToString(AuthTarget target);

// Helpers for constructing TestApi instances.
LockContentsView::TestApi MakeLockContentsViewTestApi(LockContentsView* view);
LoginAuthUserView::TestApi MakeLoginAuthTestApi(LockContentsView* view,
                                                AuthTarget auth);
LoginPasswordView::TestApi MakeLoginPasswordTestApi(LockContentsView* view,
                                                    AuthTarget auth);

// Utility method to create a new |LoginUserInfo| instance
// for regular user.
LoginUserInfo CreateUser(const std::string& email);

// Utility method to create a new |LoginUserInfo| instance for child
// user.
LoginUserInfo CreateChildUser(const std::string& email);

// Utility method to create a new |LoginUserInfo| instance for
// public account user.
LoginUserInfo CreatePublicAccountUser(const std::string& email);

// Returns true if |view| or any child of it has focus.
bool HasFocusInAnyChildView(const views::View* view);

// Keeps tabbing through |view| until the view loses focus.
// The number of generated tab events will be limited - if the focus is still
// within the view by the time the limit is hit, this will return false.
bool TabThroughView(ui::test::EventGenerator* event_generator,
                    views::View* view,
                    bool reverse);

// Find the first button in the z layer stack of the given view
views::View* FindTopButton(views::View* current_view);

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_TEST_UTILS_H_
