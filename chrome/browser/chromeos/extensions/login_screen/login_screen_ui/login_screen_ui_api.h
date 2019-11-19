// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_UI_LOGIN_SCREEN_UI_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_UI_LOGIN_SCREEN_UI_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class LoginScreenUiShowFunction : public ExtensionFunction {
 public:
  LoginScreenUiShowFunction();

  DECLARE_EXTENSION_FUNCTION("loginScreenUi.show", LOGINSCREENUI_SHOW)

 protected:
  ~LoginScreenUiShowFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenUiShowFunction);
};

class LoginScreenUiCloseFunction : public ExtensionFunction {
 public:
  LoginScreenUiCloseFunction();

  DECLARE_EXTENSION_FUNCTION("loginScreenUi.close", LOGINSCREENUI_CLOSE)

 protected:
  ~LoginScreenUiCloseFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginScreenUiCloseFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_UI_LOGIN_SCREEN_UI_API_H_
