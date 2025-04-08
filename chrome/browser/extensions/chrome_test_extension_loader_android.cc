// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_test_extension_loader.h"

#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/common/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

scoped_refptr<const Extension> ChromeTestExtensionLoader::LoadUnpacked(
    const base::FilePath& file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Clean up the kMetadataFolder if necessary.
  file_util::MaybeCleanupMetadataFolder(file_path);

  std::string load_error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      file_path, mojom::ManifestLocation::kUnpacked, 0, &load_error);
  if (!extension) {
    ADD_FAILURE() << "Failed to parse extension: " << load_error;
    return nullptr;
  }

  // Force file access and/or incognito state and set install param if
  // requested. This must occur before the call to AddExtension() below,
  // because ExtensionRegistry observers do things like load content scripts,
  // and the incognito status must be set.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  if (allow_file_access_.has_value()) {
    prefs->SetAllowFileAccess(extension->id(), *allow_file_access_);
  }
  if (allow_incognito_access_.has_value()) {
    prefs->SetIsIncognitoEnabled(extension->id(), *allow_incognito_access_);
  }
  if (install_param_.has_value()) {
    SetInstallParam(prefs, extension->id(), *install_param_);
  }

  extension_registry_->TriggerOnWillBeInstalled(extension.get(),
                                                /*is_update=*/false,
                                                /*old_name=*/std::string());
  auto* android_system = static_cast<DesktopAndroidExtensionSystem*>(
      ExtensionSystem::Get(browser_context_));
  std::string error;
  if (!android_system->AddExtension(extension, error)) {
    ADD_FAILURE() << "Failed to add extension: " << error;
  }
  extension_registry_->TriggerOnInstalled(extension.get(), /*is_update=*/false);

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  if (!registry->enabled_extensions().Contains(extension->id())) {
    ADD_FAILURE() << "Extension is not properly enabled.";
    return nullptr;
  }

  return extension;
}

}  // namespace extensions
