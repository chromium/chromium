// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/platform_test_extension_loader.h"

#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_notification_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

PlatformTestExtensionLoader::PlatformTestExtensionLoader(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_service_(extension_system_->extension_service()),
      extension_registry_(ExtensionRegistry::Get(browser_context)) {}

PlatformTestExtensionLoader::~PlatformTestExtensionLoader() = default;

scoped_refptr<const Extension> PlatformTestExtensionLoader::LoadExtension(
    const base::FilePath& path) {
  scoped_refptr<const Extension> extension = LoadExtensionFromDirectory(path);
  if (!extension) {
    return nullptr;
  }

  extension_id_ = extension->id();

  extension = extension_registry_->enabled_extensions().GetByID(extension_id_);
  if (!extension) {
    return nullptr;
  }
  if (!VerifyPermissions(extension.get())) {
    ADD_FAILURE() << "The extension did not get the requested permissions.";
    return nullptr;
  }
  if (!CheckInstallWarnings(*extension)) {
    return nullptr;
  }

  if (!WaitForExtensionReady(*extension)) {
    ADD_FAILURE() << "Failed to wait for extension ready";
    return nullptr;
  }
  return extension;
}

scoped_refptr<const Extension>
PlatformTestExtensionLoader::LoadExtensionFromDirectory(
    const base::FilePath& file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string load_error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      file_path, mojom::ManifestLocation::kUnpacked, 0, &load_error);
  if (!extension) {
    ADD_FAILURE() << "Failed to parse extension: " << load_error;
    return nullptr;
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

bool PlatformTestExtensionLoader::VerifyPermissions(
    const Extension* extension) {
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  if (allow_file_access_.has_value() &&
      prefs->AllowFileAccess(extension->id()) != *allow_file_access_) {
    return false;
  }
  if (allow_incognito_access_.has_value() &&
      prefs->IsIncognitoEnabled(extension->id()) != *allow_incognito_access_) {
    return false;
  }
  return true;
}

bool PlatformTestExtensionLoader::CheckInstallWarnings(
    const Extension& extension) {
  if (ignore_manifest_warnings_) {
    return true;
  }

  const std::vector<InstallWarning>& install_warnings =
      extension.install_warnings();
  std::string install_warnings_string;
  for (const InstallWarning& warning : install_warnings) {
    // Don't fail on the manifest v2 deprecation warning in tests for now.
    // TODO(crbug.com/40804030): Stop skipping this warning when all
    // tests are updated to MV3.
    if (warning.message == manifest_errors::kManifestV2IsDeprecatedWarning) {
      continue;
    }
    install_warnings_string += "  " + warning.message + "\n";
  }

  if (install_warnings_string.empty()) {
    return true;
  }

  ADD_FAILURE() << "Unexpected warnings for extension:\n"
                << install_warnings_string;
  return false;
}

bool PlatformTestExtensionLoader::WaitForExtensionReady(
    const Extension& extension) {
  // TODO(crbug.com/373434594): Support content scripts.
  if (!ContentScriptsInfo::GetContentScripts(&extension).empty()) {
    NOTIMPLEMENTED() << "Content scripts not yet supported.";
  }

  const int num_processes =
      content::RenderProcessHost::GetCurrentRenderProcessCountForTesting();
  // Determine whether or not to wait for extension renderers. By default, we
  // base this on whether any renderer processes exist (which is also a proxy
  // for whether this is a browser test, since MockRenderProcessHosts and
  // similar don't count towards the render process host count), but we allow
  // tests to override this behavior.
  const bool should_wait_for_ready =
      wait_for_renderers_.value_or(num_processes > 0);

  if (!should_wait_for_ready) {
    return true;
  }

  content::BrowserContext* context_to_use =
      IncognitoInfo::IsSplitMode(&extension)
          ? browser_context_.get()
          : Profile::FromBrowserContext(browser_context_)->GetOriginalProfile();

  // If possible, wait for the extension's background context to be loaded.
  std::string reason_unused;
  if (ExtensionBackgroundPageWaiter::CanWaitFor(extension, reason_unused)) {
    ExtensionBackgroundPageWaiter(context_to_use, extension)
        .WaitForBackgroundInitialized();
  }

  // TODO(devlin): Should this use |context_to_use|? Or should
  // WaitForExtensionViewsToLoad check both contexts if one is OTR?
  if (!ExtensionTestNotificationObserver(browser_context_)
           .WaitForExtensionViewsToLoad()) {
    return false;
  }

  return true;
}

}  // namespace extensions
