// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_api_lacros.h"

namespace extensions {

// quickUnlockPrivate.getAuthToken

QuickUnlockPrivateGetAuthTokenFunction::
    QuickUnlockPrivateGetAuthTokenFunction() {}

QuickUnlockPrivateGetAuthTokenFunction::
    ~QuickUnlockPrivateGetAuthTokenFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetAuthTokenFunction::Run() {
  // TODO(crbug.com/1227546): Call Ash implementation via crosapi when
  // available.
  return RespondNow(Error("AuthenticateProfileHandler crosapi unavailable."));
}

}  // namespace extensions
