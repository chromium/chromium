// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_api_lacros.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

namespace quick_unlock_private = api::quick_unlock_private;

// quickUnlockPrivate.getAuthToken

QuickUnlockPrivateGetAuthTokenFunction::
    QuickUnlockPrivateGetAuthTokenFunction() {}

QuickUnlockPrivateGetAuthTokenFunction::
    ~QuickUnlockPrivateGetAuthTokenFunction() {}

ExtensionFunction::ResponseAction
QuickUnlockPrivateGetAuthTokenFunction::Run() {
  NOTIMPLEMENTED();
  return RespondNow(Error("Deprecated"));
}

}  // namespace extensions
