// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_LOGIN_STATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_LOGIN_STATE_API_H_

#include "chrome/common/extensions/api/login_state.h"
#include "components/session_manager/session_manager_types.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

api::login_state::SessionState ToApiEnum(session_manager::SessionState state);

class LoginStateGetProfileTypeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("loginState.getProfileType",
                             LOGINSTATE_GETPROFILETYPE)

 protected:
  ~LoginStateGetProfileTypeFunction() override = default;
  ResponseAction Run() override;
};

class LoginStateGetSessionStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("loginState.getSessionState",
                             LOGINSTATE_GETSESSIONSTATE)

 protected:
  ~LoginStateGetSessionStateFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_STATE_LOGIN_STATE_API_H_
