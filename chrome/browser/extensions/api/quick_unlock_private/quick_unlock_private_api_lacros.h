// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_LACROS_H_

#include "base/functional/callback.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "chromeos/crosapi/mojom/authentication.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class QuickUnlockPrivateGetAuthTokenFunction : public ExtensionFunction {
 public:
  QuickUnlockPrivateGetAuthTokenFunction();
  QuickUnlockPrivateGetAuthTokenFunction(
      const QuickUnlockPrivateGetAuthTokenFunction&) = delete;
  const QuickUnlockPrivateGetAuthTokenFunction& operator=(
      const QuickUnlockPrivateGetAuthTokenFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getAuthToken",
                             QUICKUNLOCKPRIVATE_GETAUTHTOKEN)

 protected:
  ~QuickUnlockPrivateGetAuthTokenFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // Handler for crosapi CreateQuickUnlockPrivateTokenInfo() call.
  void OnCrosapiResult(
      crosapi::mojom::CreateQuickUnlockPrivateTokenInfoResultPtr result);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_LACROS_H_
