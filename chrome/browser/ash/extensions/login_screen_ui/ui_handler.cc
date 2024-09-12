// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/login_screen_ui/ui_handler.h"

#include <algorithm>
#include <iterator>

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/create_options.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/window.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos::login_screen_extension_ui {

namespace {

using ::ash::login_screen_extension_ui::CreateOptions;
using ::ash::login_screen_extension_ui::Window;
using ::ash::login_screen_extension_ui::WindowFactory;

const char kErrorWindowAlreadyExists[] =
    "Login screen extension UI already in use.";
const char kErrorNoExistingWindow[] = "No open window to close.";
const char kErrorNotOnLoginOrLockScreen[] =
    "Windows can only be created on the login and lock screen.";

const char kExtensionNameImprivata[] = "Imprivata OneSign";
const char kExtensionNameImprivataTest[] = "LoginScreenUi test extension";
const char kExtensionNameUnknown[] = "UNKNOWN EXTENSION";

std::string GetHardcodedExtensionName(const extensions::Extension* extension) {
  const extensions::Feature* imprivata_login_screen_extension =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kImprivataLoginScreenExtension);

  if (imprivata_login_screen_extension->IsAvailableToExtension(extension)
          .is_available()) {
    return kExtensionNameImprivata;
  }
  if (extension->id() == "oclffehlkdgibkainkilopaalpdobkan") {
    return kExtensionNameImprivataTest;
  }
  NOTREACHED_IN_MIGRATION();
  return kExtensionNameUnknown;
}

bool CanUseLoginScreenUiApi(const extensions::Extension* extension) {
  return extensions::ExtensionRegistry::Get(
             ash::ProfileHelper::GetSigninProfile())
             ->enabled_extensions()
             .Contains(extension->id()) &&
         extension->permissions_data()->HasAPIPermission(
             extensions::mojom::APIPermissionID::kLoginScreenUi) &&
         ash::InstallAttributes::Get()->IsEnterpriseManaged();
}

login_screen_extension_ui::UiHandler* g_instance = nullptr;

}  // namespace

ExtensionIdToWindowMapping::ExtensionIdToWindowMapping(
    const std::string& extension_id,
    std::unique_ptr<Window> window)
    : extension_id(extension_id), window(std::move(window)) {}

ExtensionIdToWindowMapping::~ExtensionIdToWindowMapping() = default;

// static
UiHandler* UiHandler::Get(bool can_create) {
  if (!g_instance && can_create) {
    std::unique_ptr<WindowFactory> window_factory =
        std::make_unique<WindowFactory>();
    g_instance = new UiHandler(std::move(window_factory));
  }
  return g_instance;
}

// static
void UiHandler::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

UiHandler::UiHandler(std::unique_ptr<WindowFactory> window_factory)
    : window_factory_(std::move(window_factory)) {
  UpdateSessionState();
  session_manager_observation_.Observe(session_manager::SessionManager::Get());
  extension_registry_observation_.Observe(extensions::ExtensionRegistry::Get(
      ash::ProfileHelper::GetSigninProfile()));
}

UiHandler::~UiHandler() = default;

bool UiHandler::Show(const extensions::Extension* extension,
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

  CreateOptions create_options(
      GetHardcodedExtensionName(extension),
      extension->GetResourceURL(resource_path), can_be_closed_by_user,
      base::BindOnce(base::IgnoreResult(&UiHandler::OnWindowClosed),
                     weak_ptr_factory_.GetWeakPtr(), extension->id()));

  current_window_ = std::make_unique<ExtensionIdToWindowMapping>(
      extension->id(), window_factory_->Create(&create_options));

  return true;
}

void UiHandler::Close(const extensions::Extension* extension,
                      WindowClosedCallback close_callback) {
  CHECK(CanUseLoginScreenUiApi(extension));
  close_callback_ = std::move(close_callback);
  RemoveWindowForExtension(extension->id());
}

void UiHandler::RemoveWindowForExtension(const std::string& extension_id) {
  if (!HasOpenWindow(extension_id)) {
    if (!close_callback_.is_null()) {
      std::move(close_callback_).Run(/*success=*/false, kErrorNoExistingWindow);
      close_callback_.Reset();
    }
    return;
  }

  ResetWindowAndHide();
}

void UiHandler::OnWindowClosed(const std::string& extension_id) {
  if (!close_callback_.is_null()) {
    std::move(close_callback_).Run(/*success=*/true, std::nullopt);
    close_callback_.Reset();
  }

  if (!HasOpenWindow(extension_id))
    return;

  ResetWindowAndHide();
}

void UiHandler::ResetWindowAndHide() {
  ash::LoginScreen::Get()->GetModel()->NotifyOobeDialogState(
      ash::OobeDialogState::EXTENSION_LOGIN_CLOSED);

  current_window_.reset(nullptr);
}

bool UiHandler::HasOpenWindow(const std::string& extension_id) const {
  return current_window_ && current_window_->extension_id == extension_id;
}

void UiHandler::UpdateSessionState() {
  TRACE_EVENT0("ui", "UiHandler::UpdateSessionState");
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

void UiHandler::OnSessionStateChanged() {
  TRACE_EVENT0("login", "LoginStateAsh::OnSessionStateChanged");
  UpdateSessionState();
}

void UiHandler::OnExtensionUninstalled(content::BrowserContext* browser_context,
                                       const extensions::Extension* extension,
                                       extensions::UninstallReason reason) {
  HandleExtensionUnloadOrUinstall(extension);
}

void UiHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  HandleExtensionUnloadOrUinstall(extension);
}

void UiHandler::HandleExtensionUnloadOrUinstall(
    const extensions::Extension* extension) {
  RemoveWindowForExtension(extension->id());
}

Window* UiHandler::GetWindowForTesting(const std::string& extension_id) {
  if (!HasOpenWindow(extension_id))
    return nullptr;

  return current_window_->window.get();
}

}  // namespace chromeos::login_screen_extension_ui
