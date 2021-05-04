// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_SAFE_MODE_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_SAFE_MODE_DELEGATE_H_

#include "chromeos/login/auth/safe_mode_delegate.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chromeos/login/auth/user_context.h"

namespace ash {

class ChromeSafeModeDelegate : public SafeModeDelegate {
 public:
  ChromeSafeModeDelegate() = default;
  ~ChromeSafeModeDelegate() override = default;

  // Not copyable or movable.
  ChromeSafeModeDelegate(const ChromeSafeModeDelegate&) = delete;
  ChromeSafeModeDelegate& operator=(const ChromeSafeModeDelegate&) = delete;

  bool IsSafeMode() override;
  void CheckSafeModeOwnership(const UserContext& context,
                              IsOwnerCallback callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_SAFE_MODE_DELEGATE_H_
