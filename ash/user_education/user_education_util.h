// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_UTIL_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_UTIL_H_

#include <string>

#include "ash/ash_export.h"
#include "components/user_education/common/help_bubble_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class AccountId;

namespace ui {
class ElementIdentifier;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

enum class HelpBubbleId;
enum class HelpBubbleStyle;
enum class TutorialId;
struct UserSession;

namespace user_education_util {

// Returns extended properties for a help bubble having set `help_bubble_id`.
ASH_EXPORT user_education::HelpBubbleParams::ExtendedProperties
CreateExtendedProperties(HelpBubbleId help_bubble_id);

// Returns extended properties for a help bubble having set `help_bubble_style`.
ASH_EXPORT user_education::HelpBubbleParams::ExtendedProperties
CreateExtendedProperties(HelpBubbleStyle help_bubble_style);

// Returns the `AccountId` for the specified `user_session`. If the specified
// `user_session` is `nullptr`, `EmptyAccountId()` is returned.
ASH_EXPORT const AccountId& GetAccountId(const UserSession* user_session);

// Returns help bubble ID from the specified `extended_properties`.
ASH_EXPORT HelpBubbleId GetHelpBubbleId(
    const user_education::HelpBubbleParams::ExtendedProperties&
        extended_properties);

// Returns help bubble style from the specified `extended_properties`. If the
// specified `extended_properties` does not contain help bubble style, an
// absent value is returned.
ASH_EXPORT absl::optional<HelpBubbleStyle> GetHelpBubbleStyle(
    const user_education::HelpBubbleParams::ExtendedProperties&
        extended_properties);

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
