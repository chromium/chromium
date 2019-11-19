// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_extension_loader.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"

namespace chromeos {

AccessibilityExtensionLoader::AccessibilityExtensionLoader(
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const base::Closure& unload_callback)
    : profile_(nullptr),
      extension_id_(extension_id),
      extension_path_(extension_path),
      loaded_(false),
      unload_callback_(unload_callback) {}

AccessibilityExtensionLoader::~AccessibilityExtensionLoader() {}

void AccessibilityExtensionLoader::SetProfile(
    Profile* profile,
    const base::Closure& done_callback) {
  Profile* prev_profile = profile_;
  profile_ = profile;

  if (!loaded_)
    return;

  // If the extension was loaded on the previous profile, unload it there.
  if (prev_profile)
    UnloadExtensionFromProfile(prev_profile);

  // If the extension was already enabled, but not for this profile, add it
  // to this profile.
  auto* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  auto* component_loader = extension_service->component_loader();
  if (!component_loader->Exists(extension_id_))
    LoadExtension(profile_, done_callback);
}

void AccessibilityExtensionLoader::Load(Profile* profile,
                                        const base::Closure& done_cb) {
  profile_ = profile;

  if (loaded_)
    return;

  loaded_ = true;
  LoadExtension(profile_, done_cb);
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
  extension_service->component_loader()->Remove(extension_path_);
}

void AccessibilityExtensionLoader::LoadExtension(Profile* profile,
                                                 base::Closure done_cb) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  extension_service->component_loader()->AddComponentFromDir(
      extension_path_, extension_id_.c_str(), done_cb);
}

}  // namespace chromeos
