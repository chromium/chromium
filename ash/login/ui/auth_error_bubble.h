// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_AUTH_ERROR_BUBBLE_H_
#define ASH_LOGIN_UI_AUTH_ERROR_BUBBLE_H_

#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/login_error_bubble.h"

namespace ash {

// Horizontal and vertical padding of auth error bubble.
constexpr int kHorizontalPaddingAuthErrorBubbleDp = 8;
constexpr int kVerticalPaddingAuthErrorBubbleDp = 8;

class AuthErrorBubble : public LoginErrorBubble {
 public:
  AuthErrorBubble() {
    set_positioning_strategy(PositioningStrategy::kTryAfterThenBefore);
    SetPadding(kHorizontalPaddingAuthErrorBubbleDp,
               kVerticalPaddingAuthErrorBubbleDp);
  }
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_ERROR_BUBBLE_H_
