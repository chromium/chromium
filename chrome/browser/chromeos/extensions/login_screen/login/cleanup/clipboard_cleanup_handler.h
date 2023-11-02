// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLIPBOARD_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLIPBOARD_CLEANUP_HANDLER_H_

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"

namespace chromeos {

// A cleanup handler which clears the profile's clipboard.
class ClipboardCleanupHandler : public CleanupHandler {
 public:
  ClipboardCleanupHandler();
  ~ClipboardCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLIPBOARD_CLEANUP_HANDLER_H_
