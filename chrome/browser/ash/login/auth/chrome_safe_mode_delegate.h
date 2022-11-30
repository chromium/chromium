// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_SAFE_MODE_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_SAFE_MODE_DELEGATE_H_

#include "chromeos/ash/components/login/auth/safe_mode_delegate.h"

namespace ash {

class ChromeSafeModeDelegate : public SafeModeDelegate {
 public:
  ChromeSafeModeDelegate() = default;
  ~ChromeSafeModeDelegate() override = default;

  // Not copyable or movable.
  ChromeSafeModeDelegate(const ChromeSafeModeDelegate&) = delete;
  ChromeSafeModeDelegate& operator=(const ChromeSafeModeDelegate&) = delete;

  bool IsSafeMode() override;
  void CheckSafeModeOwnership(const std::string& user_id_hash,
                              IsOwnerCallback callback) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_CHROME_SAFE_MODE_DELEGATE_H_
