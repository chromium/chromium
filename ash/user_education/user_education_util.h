// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_UTIL_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_UTIL_H_

#include <string>

#include "ash/ash_export.h"

class AccountId;

namespace ui {
class ElementIdentifier;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

enum class TutorialId;
struct UserSession;

namespace user_education_util {

// Returns the `AccountId` for the specified `user_session`. If the specified
// `user_session` is `nullptr`, `EmptyAccountId()` is returned.
ASH_EXPORT const AccountId& GetAccountId(const UserSession* user_session);

// Returns a matching view for the specified `element_id` in the root window
// associated with the specified `display_id`, or `nullptr` if no match is
// found. Note that if multiple matches exist, this method does *not* guarantee
// which will be returned.
ASH_EXPORT views::View* GetMatchingViewInRootWindow(
    int64_t display_id,
    ui::ElementIdentifier element_id);

// Returns whether the primary user account is active.
ASH_EXPORT bool IsPrimaryAccountActive();

// Returns whether `account_id` is associated with the primary user account.
ASH_EXPORT bool IsPrimaryAccountId(const AccountId& account_id);

// Returns the unique string representation of the specified `tutorial_id`.
ASH_EXPORT std::string ToString(TutorialId tutorial_id);

}  // namespace user_education_util
}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_UTIL_H_
