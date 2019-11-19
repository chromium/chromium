// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/login_screen_extension_ui_handler.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_create_options.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/login_screen_extension_ui_window.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

namespace {

const char kErrorWindowAlreadyExists[] =
    "Login screen extension UI already in use.";
const char kErrorNoExistingWindow[] = "No open window to close.";
const char kErrorNotOnLoginOrLockScreen[] =
    "Windows can only be created on the login and lock screen.";

LoginScreenExtensionUiHandler* g_instance = nullptr;

struct HardcodedExtensionNameMapping {
  const char* extension_id;
  const char* extension_name;
};

// Hardcoded extension names to be used in the window's dialog title.
// Intentionally not using |extension->name()| here to prevent a compromised
// extension from being able to control the dialog title's content.
const HardcodedExtensionNameMapping kHardcodedExtensionNameMappings[] = {
    {"cdgickkdpbekbnalbmpgochbninibkko", "Imprivata"},
    {"lpimkpkllnkdlcigdbgmabfplniahkgm", "Imprivata"},
    {"oclffehlkdgibkainkilopaalpdobkan", "LoginScreenUi test extension"},
};

std::string GetHardcodedExtensionName(const extensions::Extension* extension) {
  for (const HardcodedExtensionNameMapping& mapping :
       kHardcodedExtensionNameMappings) {
    if (mapping.extension_id == extension->id())
      return mapping.extension_name;
  }
  NOTREACHED();
  return "UNKNOWN EXTENSION";
}

bool CanUseLoginScreenUiApi(const extensions::Extension* extension) {
  return extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile())
             ->enabled_extensions()
             .Contains(extension->id()) &&
         extension->permissions_data()->HasAPIPermission(
             extensions::APIPermission::kLoginScreenUi) &&
         InstallAttributes::Get()->IsEnterpriseManaged();
}

}  // namespace

ExtensionIdToWindowMapping::ExtensionIdToWindowMapping(
    const std::string& extension_id,
    std::unique_ptr<LoginScreenExtensionUiWindow> window)
    : extension_id(extension_id), window(std::move(window)) {}

ExtensionIdToWindowMapping::~ExtensionIdToWindowMapping() = default;

// static
LoginScreenExtensionUiHandler* LoginScreenExtensionUiHandler::Get(
    bool can_create) {
  if (!g_instance && can_create) {
    std::unique_ptr<LoginScreenExtensionUiWindowFactory> window_factory =
        std::make_unique<LoginScreenExtensionUiWindowFactory>();
    g_instance = new LoginScreenExtensionUiHandler(std::move(window_factory));
  }
  return g_instance;
}

// static
void LoginScreenExtensionUiHandler::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

LoginScreenExtensionUiHandler::LoginScreenExtensionUiHandler(
    std::unique_ptr<LoginScreenExtensionUiWindowFactory> window_factory)
    : window_factory_(std::move(window_factory)) {
  UpdateSessionState();
  session_manager_observer_.Add(session_manager::SessionManager::Get());
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(ProfileHelper::GetSigninProfile()));
}

LoginScreenExtensionUiHandler::~LoginScreenExtensionUiHandler() = default;

bool LoginScreenExtensionUiHandler::Show(const extensions::Extension* extension,
                                         const std::string& resource_path,
                                         bool can_be_closed_by_user,
                                         std::string* error) {
  CHECK(CanUseLoginScreenUiApi(extension));
  if (!login_or_lock_screen_active_) {
    *error = kErrorNotOnLoginOrLockScreen;
    return false;
  }
  if (current_window_) {
    *error = kErrorWindowAlreadyExists;
    return false;
  }

  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      ash::OobeDialogState::EXTENSION_LOGIN);

  LoginScreenExtensionUiCreateOptions create_options(
      GetHardcodedExtensionName(extension),
      extension->GetResourceURL(resource_path), can_be_closed_by_user,
      base::BindOnce(
          base::IgnoreResult(
              &LoginScreenExtensionUiHandler::RemoveWindowForExtension),
          weak_ptr_factory_.GetWeakPtr(), extension->id()));

  current_window_ = std::make_unique<ExtensionIdToWindowMapping>(
      extension->id(), window_factory_->Create(&create_options));

  return true;
}

bool LoginScreenExtensionUiHandler::Close(
    const extensions::Extension* extension,
    std::string* error) {
  CHECK(CanUseLoginScreenUiApi(extension));
  if (!RemoveWindowForExtension(extension->id())) {
    *error = kErrorNoExistingWindow;
    return false;
  }
  return true;
}

bool LoginScreenExtensionUiHandler::RemoveWindowForExtension(
    const std::string& extension_id) {
  if (!HasOpenWindow(extension_id))
    return false;

  current_window_.reset(nullptr);

  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      ash::OobeDialogState::HIDDEN);

  return true;
}

bool LoginScreenExtensionUiHandler::HasOpenWindow(
    const std::string& extension_id) const {
  return current_window_ && current_window_->extension_id == extension_id;
}

void LoginScreenExtensionUiHandler::UpdateSessionState() {
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  bool new_login_or_lock_screen_active =
      state == session_manager::SessionState::LOGIN_PRIMARY ||
      state == session_manager::SessionState::LOGGED_IN_NOT_ACTIVE ||
      state == session_manager::SessionState::LOCKED;
  if (login_or_lock_screen_active_ == new_login_or_lock_screen_active)
    return;

  login_or_lock_screen_active_ = new_login_or_lock_screen_active;

  if (!login_or_lock_screen_active_)
    current_window_.reset(nullptr);
}

void LoginScreenExtensionUiHandler::OnSessionStateChanged() {
  UpdateSessionState();
}

void LoginScreenExtensionUiHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  RemoveWindowForExtension(extension->id());
}

LoginScreenExtensionUiWindow*
LoginScreenExtensionUiHandler::GetWindowForTesting(
    const std::string& extension_id) {
  if (!HasOpenWindow(extension_id))
    return nullptr;

  return current_window_->window.get();
}

}  // namespace chromeos
