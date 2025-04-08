// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_test_extension_loader.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/extension_test_notification_observer.h"
#include "extensions/test/test_content_script_load_waiter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

ChromeTestExtensionLoader::ChromeTestExtensionLoader(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      extension_system_(ExtensionSystem::Get(browser_context)),
#if !BUILDFLAG(IS_ANDROID)
      extension_service_(extension_system_->extension_service()),
#endif
      extension_registry_(ExtensionRegistry::Get(browser_context)) {
}

ChromeTestExtensionLoader::~ChromeTestExtensionLoader() {
  // If there was a temporary directory created for a CRX, we need to clean it
  // up before the member is destroyed so we can explicitly allow IO.
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (temp_dir_.IsValid())
    EXPECT_TRUE(temp_dir_.Delete());
}

bool ChromeTestExtensionLoader::WaitForExtensionReady(
    const Extension& extension) {
  UserScriptManager* user_script_manager =
      ExtensionSystem::Get(browser_context_)->user_script_manager();
  // Note: |user_script_manager| can be null in tests.
  if (user_script_manager &&
      !ContentScriptsInfo::GetContentScripts(&extension).empty()) {
    ExtensionUserScriptLoader* user_script_loader =
        user_script_manager->GetUserScriptLoaderForExtension(extension_id_);
    if (!user_script_loader->HasLoadedScripts()) {
      ContentScriptLoadWaiter waiter(user_script_loader);
      waiter.Wait();
    }
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

  if (!should_wait_for_ready)
    return true;

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

bool ChromeTestExtensionLoader::VerifyPermissions(const Extension* extension) {
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

bool ChromeTestExtensionLoader::CheckInstallWarnings(
    const Extension& extension) {
  if (ignore_manifest_warnings_)
    return true;

  const std::vector<InstallWarning>& install_warnings =
      extension.install_warnings();
  std::string install_warnings_string;
  for (const InstallWarning& warning : install_warnings) {
    // Don't fail on the manifest v2 deprecation warning in tests for now.
    // TODO(crbug.com/40804030): Stop skipping this warning when all
    // tests are updated to MV3.
    if (warning.message == manifest_errors::kManifestV2IsDeprecatedWarning)
      continue;
    install_warnings_string += "  " + warning.message + "\n";
  }

  if (install_warnings_string.empty())
    return true;

  ADD_FAILURE() << "Unexpected warnings for extension:\n"
                << install_warnings_string;
  return false;
}

}  // namespace extensions
