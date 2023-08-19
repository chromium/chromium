// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_WEB_APP_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_WEB_APP_CLEANUP_HANDLER_H_

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "components/webapps/browser/uninstall_result_code.h"

namespace chromeos {

// A cleanup handler which uninstalls the user-installed web apps.
class WebAppCleanupHandler : public CleanupHandler {
 public:
  WebAppCleanupHandler();
  ~WebAppCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_WEB_APP_CLEANUP_HANDLER_H_
