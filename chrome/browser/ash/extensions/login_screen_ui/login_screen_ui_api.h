// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_LOGIN_SCREEN_UI_LOGIN_SCREEN_UI_API_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_LOGIN_SCREEN_UI_LOGIN_SCREEN_UI_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class LoginScreenUiShowFunction : public ExtensionFunction {
 public:
  LoginScreenUiShowFunction();

  LoginScreenUiShowFunction(const LoginScreenUiShowFunction&) = delete;
  LoginScreenUiShowFunction& operator=(const LoginScreenUiShowFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION("loginScreenUi.show", LOGINSCREENUI_SHOW)

 protected:
  ~LoginScreenUiShowFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginScreenUiCloseFunction : public ExtensionFunction {
 public:
  LoginScreenUiCloseFunction();

  LoginScreenUiCloseFunction(const LoginScreenUiCloseFunction&) = delete;
  LoginScreenUiCloseFunction& operator=(const LoginScreenUiCloseFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION("loginScreenUi.close", LOGINSCREENUI_CLOSE)

 protected:
  ~LoginScreenUiCloseFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Callback upon completion of window closing.
  void OnClosed(bool success, const std::optional<std::string>& error);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_LOGIN_SCREEN_UI_LOGIN_SCREEN_UI_API_H_
