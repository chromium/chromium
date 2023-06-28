// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_EXTENSION_CLEANUP_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_EXTENSION_CLEANUP_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_handler.h"
#include "chrome/browser/extensions/extension_service.h"

class Profile;

namespace chromeos {

// A cleanup handler which clears app/extension related data stored in memory
// while they are running or in persistent storage (e.g. chrome.storage API).
// By uninstalling the apps/extensions, we make sure all user data is removed
// and after that, we reinstall the force-installed, component and external
// component extensions.
class ExtensionCleanupHandler : public CleanupHandler {
 public:
  ExtensionCleanupHandler();
  ~ExtensionCleanupHandler() override;

  // CleanupHandler:
  void Cleanup(CleanupHandlerCallback callback) override;

 private:
  // Uninstalls all installed extensions except the ones added to
  // prefs::kRestrictedManagedGuestSessionExtensionCleanupExemptList.
  void UninstallExtensions();

  // Callback for each uninstalled extension.
  void OnUninstallDataDeleterFinished(const std::string& extension_id);

  // Reinstalls force-installed, component and external component extensions.
  void ReinstallExtensions();

  std::unordered_set<std::string> GetCleanupExemptExtensions();

  raw_ptr<Profile> profile_ = nullptr;
  std::vector<std::string> errors_;
  raw_ptr<extensions::ExtensionService> extension_service_ = nullptr;
  CleanupHandlerCallback callback_;
  std::unordered_set<std::string> extensions_to_be_uninstalled_;
};

}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_EXTENSION_CLEANUP_HANDLER_H_
