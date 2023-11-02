// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_PRINT_JOBS_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_PRINT_JOBS_CLEANUP_HANDLER_H_

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"

namespace chromeos {

// A cleanup handler which clears the profile's print jobs.
// TODO(b:200678974) Add Tast test.
class PrintJobsCleanupHandler : public CleanupHandler {
 public:
  PrintJobsCleanupHandler();
  ~PrintJobsCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;

 private:
  void OnDeleteAllPrintJobsDone(bool success);

  CleanupHandlerCallback callback_;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_PRINT_JOBS_CLEANUP_HANDLER_H_
