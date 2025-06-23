// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_LOGIN_ENTERPRISE_LOGIN_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_LOGIN_ENTERPRISE_LOGIN_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class EnterpriseLoginExitCurrentManagedGuestSessionFunction
    : public ExtensionFunction {
 public:
  EnterpriseLoginExitCurrentManagedGuestSessionFunction();

  EnterpriseLoginExitCurrentManagedGuestSessionFunction(
      const EnterpriseLoginExitCurrentManagedGuestSessionFunction&) = delete;

  EnterpriseLoginExitCurrentManagedGuestSessionFunction& operator=(
      const EnterpriseLoginExitCurrentManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("enterprise.login.exitCurrentManagedGuestSession",
                             ENTERPRISE_LOGIN_EXITCURRENTMANAGEDGUESTSESSION)

 protected:
  ~EnterpriseLoginExitCurrentManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  //  namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_LOGIN_ENTERPRISE_LOGIN_API_H_
