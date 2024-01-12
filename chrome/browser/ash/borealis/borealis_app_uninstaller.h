// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_UNINSTALLER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_UNINSTALLER_H_

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"

class Profile;

namespace borealis {

// Helper class responsible for uninstalling borealis' apps.
class BorealisAppUninstaller {
 public:
  enum class UninstallResult {
    kSuccess,
    kError,
  };

  using OnUninstalledCallback = base::OnceCallback<void(UninstallResult)>;

  explicit BorealisAppUninstaller(Profile* profile);

  // Uninstall the given |app_id|'s associated application. Uninstalling the
  // parent borealis app itself will result in removing it and all of the child
  // apps, whereas uninstalling individual child apps will only remove that
  // specific app (using its own uninstallation flow).
  void Uninstall(std::string app_id, OnUninstalledCallback callback);

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_APP_UNINSTALLER_H_
