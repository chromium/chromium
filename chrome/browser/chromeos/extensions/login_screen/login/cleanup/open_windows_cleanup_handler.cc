// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/open_windows_cleanup_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

OpenWindowsCleanupHandler::OpenWindowsCleanupHandler() = default;

OpenWindowsCleanupHandler::~OpenWindowsCleanupHandler() = default;

void OpenWindowsCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  DCHECK(callback_.is_null());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    std::move(callback).Run("There is no active user");
    return;
  }

  callback_ = std::move(callback);

  // `on_close_aborted` cannot be reached since `skip_beforeunload` is true.
  BrowserList::CloseAllBrowsersWithProfile(
      profile,
      /*on_close_success=*/
      base::BindRepeating(&OpenWindowsCleanupHandler::OnCloseDone,
                          base::Unretained(this)),
      /*on_close_aborted=*/BrowserList::CloseCallback(),
      /*skip_beforeunload=*/true);
}

void OpenWindowsCleanupHandler::OnCloseDone(const base::FilePath& file_path) {
  std::move(callback_).Run(absl::nullopt);
}

}  // namespace chromeos
