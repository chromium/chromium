// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_PINNED_APPS_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_PINNED_APPS_CLEANUP_HANDLER_H_

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"

namespace chromeos {

// A cleanup handler which clears the pinned apps added by the user to the
// shelf.
// TODO(b:309099718) Add pinned apps to shared MGS session Tast test.
class PinnedAppsCleanupHandler : public CleanupHandler {
 public:
  PinnedAppsCleanupHandler();
  ~PinnedAppsCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_PINNED_APPS_CLEANUP_HANDLER_H_
