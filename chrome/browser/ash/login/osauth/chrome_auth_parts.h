// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_CHROME_AUTH_PARTS_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_CHROME_AUTH_PARTS_H_

#include <memory>

#include "base/callback_list.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

namespace ash {

// Creates and owns `ash::AuthParts` instance and provides it with
// browser-specific implementations.
class ChromeAuthParts {
 public:
  ChromeAuthParts();
  ~ChromeAuthParts();

 private:
  void OnAppTerminating();

  base::CallbackListSubscription app_termination_subscription_;
  std::unique_ptr<AuthParts> auth_parts_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_CHROME_AUTH_PARTS_H_
