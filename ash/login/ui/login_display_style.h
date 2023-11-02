// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_DISPLAY_STYLE_H_
#define ASH_LOGIN_UI_LOGIN_DISPLAY_STYLE_H_

namespace ash {

// LoginDisplayStyle enables child views to tweak layout (ie, dp between
// elements, element ordering, etc) while still retaining the same high-level
// view structure.
//
// For example, LoginUserView always has a profile icon and a username label. In
// a large style, the icon will be above the label, but in a small style the
// icon will be to the left of the label.
enum class LoginDisplayStyle {
  kLarge,      // Large pod (always present, sometimes 2 instances)
  kSmall,      // Used in non-scrollabe user list for a moderate user count.
  kExtraSmall  // Extra small user list for a very large number of users.
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_DISPLAY_STYLE_H_