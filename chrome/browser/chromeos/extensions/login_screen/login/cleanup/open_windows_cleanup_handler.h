// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_OPEN_WINDOWS_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_OPEN_WINDOWS_CLEANUP_HANDLER_H_

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"

namespace base {
class FilePath;
}

namespace chromeos {

// A cleanup handler which closes all open browser windows. Windows created by
// the chrome.app.window are not closed but they will be handled separately by
// the extension and app cleanup handler.
// TODO(jityao, b:200678974) Add browser tests.
class OpenWindowsCleanupHandler : public CleanupHandler {
 public:
  OpenWindowsCleanupHandler();
  ~OpenWindowsCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;

 private:
  void OnCloseDone(const base::FilePath& file_path);

  CleanupHandlerCallback callback_;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_OPEN_WINDOWS_CLEANUP_HANDLER_H_
