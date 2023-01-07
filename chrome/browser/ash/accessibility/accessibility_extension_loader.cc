// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_extension_loader.h"

#include <utility>

#include "base/callback.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"

namespace ash {

AccessibilityExtensionLoader::AccessibilityExtensionLoader(
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const base::FilePath::CharType* manifest_filename,
    const base::FilePath::CharType* guest_manifest_filename,
    base::RepeatingClosure unload_callback)
    : profile_(nullptr),
      extension_id_(extension_id),
      extension_path_(extension_path),
      manifest_filename_(manifest_filename),
      guest_manifest_filename_(guest_manifest_filename),
      loaded_(false),
      unload_callback_(unload_callback) {}

AccessibilityExtensionLoader::~AccessibilityExtensionLoader() = default;

void AccessibilityExtensionLoader::SetProfile(Profile* profile,
                                              base::OnceClosure done_callback) {
  Profile* prev_profile = profile_;
  profile_ = profile;

  if (!loaded_)
    return;

  // If the extension was loaded on the previous profile (which isn't the
  // current profile), unload it there.
  if (prev_profile && prev_profile != profile)
    UnloadExtensionFromProfile(prev_profile);

  // If the extension was already enabled, but not for this profile, add it
  // to this profile.
  auto* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  auto* component_loader = extension_service->component_loader();
  if (!component_loader->Exists(extension_id_))
    LoadExtension(profile_, std::move(done_callback));
}

void AccessibilityExtensionLoader::Load(Profile* profile,
                                        base::OnceClosure done_cb) {
  profile_ = profile;

  if (loaded_)
    return;

  loaded_ = true;
  LoadExtension(profile_, std::move(done_cb));
}

void AccessibilityExtensionLoader::Unload() {
  if (loaded_) {
    UnloadExtensionFromProfile(profile_);
    UnloadExtensionFromProfile(ProfileHelper::GetSigninProfile());
    loaded_ = false;
  }

  profile_ = nullptr;

  if (unload_callback_)
    unload_callback_.Run();
}

//
// private
//

void AccessibilityExtensionLoader::UnloadExtensionFromProfile(
    Profile* profile) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->component_loader()->Remove(extension_id_);
}

void AccessibilityExtensionLoader::LoadExtension(Profile* profile,
                                                 base::OnceClosure done_cb) {
  // In Kiosk mode, we should reinstall the extension upon first load. This way,
  // no state from the previous session is preserved.
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);

  base::OnceClosure closure;
  if (user && user->IsKioskType() && !was_reset_for_kiosk_) {
    was_reset_for_kiosk_ = true;
    extensions::ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    extension_service->DisableExtension(
        extension_id_, extensions::disable_reason::DISABLE_REINSTALL);
    closure = base::BindOnce(
        &AccessibilityExtensionLoader::ReinstallExtensionForKiosk,
        weak_ptr_factory_.GetWeakPtr(), profile, std::move(done_cb));
  } else {
    closure = std::move(done_cb);
  }

  LoadExtensionImpl(profile, std::move(closure));
}

void AccessibilityExtensionLoader::LoadExtensionImpl(
    Profile* profile,
    base::OnceClosure done_cb) {
  DCHECK(manifest_filename_);
  DCHECK(guest_manifest_filename_);

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  extension_service->component_loader()
      ->AddComponentFromDirWithManifestFilename(
          extension_path_, extension_id_.c_str(), manifest_filename_,
          guest_manifest_filename_, std::move(done_cb));
}

void AccessibilityExtensionLoader::ReinstallExtensionForKiosk(
    Profile* profile,
    base::OnceClosure done_cb) {
  DCHECK(was_reset_for_kiosk_);

  auto* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  std::u16string error;
  extension_service->UninstallExtension(
      extension_id_, extensions::UninstallReason::UNINSTALL_REASON_REINSTALL,
      &error);
  extension_service->component_loader()->Reload(extension_id_);

  if (done_cb)
    std::move(done_cb).Run();
}

}  // namespace ash
