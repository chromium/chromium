// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_extension_loader.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "extensions/browser/extension_system.h"

namespace ash {

AccessibilityExtensionLoader::AccessibilityExtensionLoader(
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const base::FilePath::CharType* manifest_filename,
    const base::FilePath::CharType* guest_manifest_filename,
    base::RepeatingClosure unload_callback)
    : extension_id_(extension_id),
      extension_path_(extension_path),
      manifest_filename_(manifest_filename),
      guest_manifest_filename_(guest_manifest_filename),
      unload_callback_(unload_callback) {}

AccessibilityExtensionLoader::~AccessibilityExtensionLoader() = default;

void AccessibilityExtensionLoader::SetBrowserContext(
    content::BrowserContext* browser_context,
    base::OnceClosure done_callback) {
  content::BrowserContext* prev_browser_context = browser_context_;
  browser_context_ = browser_context;

  if (!loaded_)
    return;

  // If the extension was loaded on the previous browser context (which isn't
  // the current browser context), unload it there.
  if (prev_browser_context && prev_browser_context != browser_context) {
    UnloadExtension(prev_browser_context);
  }

  // If the extension was already enabled, but not for this profile, add it
  // to this profile.
  auto* extension_service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  auto* component_loader = extension_service->component_loader();
  if (!component_loader->Exists(extension_id_)) {
    LoadExtension(browser_context_, std::move(done_callback));
  }
}

void AccessibilityExtensionLoader::Load(
    content::BrowserContext* browser_context,
    base::OnceClosure done_cb) {
  browser_context_ = browser_context;

  if (loaded_)
    return;

  loaded_ = true;
  LoadExtension(browser_context_, std::move(done_cb));
}

void AccessibilityExtensionLoader::Unload() {
  if (loaded_) {
    UnloadExtension(browser_context_);
    UnloadExtension(BrowserContextHelper::Get()->GetSigninBrowserContext());
    loaded_ = false;
  }

  browser_context_ = nullptr;

  if (unload_callback_) {
    unload_callback_.Run();
  }
}

//
// private
//

void AccessibilityExtensionLoader::LoadExtension(
    content::BrowserContext* browser_context,
    base::OnceClosure done_cb) {
  DCHECK(manifest_filename_);
  DCHECK(guest_manifest_filename_);

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_context)->extension_service();

  // In Kiosk mode, we should reinstall the extension upon first load. This way,
  // no state from the previous session is preserved.
  const user_manager::User* user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(browser_context);
  if (user && user->IsKioskType() && !was_reset_for_kiosk_) {
    was_reset_for_kiosk_ = true;
    extension_service->DisableExtension(
        extension_id_, extensions::disable_reason::DISABLE_REINSTALL);
    done_cb = base::BindOnce(
        &AccessibilityExtensionLoader::ReinstallExtensionForKiosk,
        weak_ptr_factory_.GetWeakPtr(), browser_context, std::move(done_cb));
  }

  extension_service->component_loader()
      ->AddComponentFromDirWithManifestFilename(
          extension_path_, extension_id_.c_str(), manifest_filename_,
          guest_manifest_filename_, std::move(done_cb));
}

void AccessibilityExtensionLoader::ReinstallExtensionForKiosk(
    content::BrowserContext* browser_context,
    base::OnceClosure done_cb) {
  DCHECK(was_reset_for_kiosk_);

  auto* extension_service =
      extensions::ExtensionSystem::Get(browser_context)->extension_service();
  std::u16string error;
  extension_service->UninstallExtension(
      extension_id_, extensions::UninstallReason::UNINSTALL_REASON_REINSTALL,
      &error);
  extension_service->component_loader()->Reload(extension_id_);

  if (done_cb)
    std::move(done_cb).Run();
}

void AccessibilityExtensionLoader::UnloadExtension(
    content::BrowserContext* browser_context) {
  auto* extension_service =
      extensions::ExtensionSystem::Get(browser_context)->extension_service();
  extension_service->component_loader()->Remove(extension_id_);
}

}  // namespace ash
