// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_UI_LOGIN_SCREEN_EXTENSION_UI_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_UI_LOGIN_SCREEN_EXTENSION_UI_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

class LoginScreenExtensionUiWindow;
class LoginScreenExtensionUiWindowFactory;

struct ExtensionIdToWindowMapping {
  ExtensionIdToWindowMapping(
      const std::string& extension_id,
      std::unique_ptr<LoginScreenExtensionUiWindow> window);
  ~ExtensionIdToWindowMapping();

  const std::string extension_id;
  std::unique_ptr<LoginScreenExtensionUiWindow> window;
};

// This class receives calls from the chrome.loginScreenUi API and manages the
// associated window (only one window can be active at a time). Windows can only
// be created on the login and lock screen and are automatically closed when
// logging in or unlocking a user session.
// TODO(hendrich): handle interference with other ChromeOS UI that pops up
// during login (e.g. arc ToS).
class LoginScreenExtensionUiHandler
    : public session_manager::SessionManagerObserver,
      public extensions::ExtensionRegistryObserver {
 public:
  static LoginScreenExtensionUiHandler* Get(bool can_create);
  static void Shutdown();

  explicit LoginScreenExtensionUiHandler(
      std::unique_ptr<LoginScreenExtensionUiWindowFactory> window_factory);
  ~LoginScreenExtensionUiHandler() override;

  // Endpoint for calls to the chrome.loginScreenUi.show() API. If an error
  // occurs, |error| will contain the error description.
  bool Show(const extensions::Extension* extension,
            const std::string& url,
            bool can_be_closed_by_user,
            std::string* error);

  // Endpoint for calls to the chrome.loginScreenUi.close() API. If an error
  // occurs, |error| will contain the error description.
  bool Close(const extensions::Extension* extension, std::string* error);

  // Removes and closes the window for the given |extension_id|. This is also
  // passed as close callback to the windows during creation to detect when a
  // window was manually closed by the user. Returns true, if a window was
  // closed.
  bool RemoveWindowForExtension(const std::string& extension_id);

  bool HasOpenWindow(const std::string& extension_id) const;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  LoginScreenExtensionUiWindow* GetWindowForTesting(
      const std::string& extension_id);

 private:
  void UpdateSessionState();

  // extensions::ExtensionRegistryObserver
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  std::unique_ptr<LoginScreenExtensionUiWindowFactory> window_factory_;

  bool login_or_lock_screen_active_ = false;

  std::unique_ptr<ExtensionIdToWindowMapping> current_window_;

  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_manager_observer_{this};
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::WeakPtrFactory<LoginScreenExtensionUiHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoginScreenExtensionUiHandler);
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_UI_LOGIN_SCREEN_EXTENSION_UI_HANDLER_H_
