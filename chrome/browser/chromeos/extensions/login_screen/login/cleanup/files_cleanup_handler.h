// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_FILES_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_FILES_CLEANUP_HANDLER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "content/public/browser/browsing_data_remover.h"

class Profile;

namespace chromeos {

// A cleanup handler which clears the profile's MyFiles and Downloads
// directories.
class FilesCleanupHandler : public CleanupHandler {
 public:
  FilesCleanupHandler();
  ~FilesCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;

 private:
  bool CleanupTaskOnTaskRunner(Profile* profile);

  void CleanupTaskDone(CleanupHandlerCallback callback, bool success);

  scoped_refptr<base::TaskRunner> task_runner_;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_FILES_CLEANUP_HANDLER_H_
