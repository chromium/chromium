// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_platform_browsertest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_browser_test_util.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_paths.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#else
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"
#include "chrome/browser/extensions/platform_test_extension_loader.h"
#endif

namespace extensions {
namespace {

using ContextType = extensions::browser_test_util::ContextType;

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  NotificationDisplayServiceTester::EnsureFactoryBuilt();
}

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
bool IsMV3AllowedContextType(ContextType context_type) {
  return context_type == ContextType::kServiceWorker ||
         context_type == ContextType::kFromManifest ||
         context_type == ContextType::kNone;
}
#endif

}  // namespace

ExtensionPlatformBrowserTest::ExtensionPlatformBrowserTest(
    ContextType context_type)
    : context_type_(context_type) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // Android only allows certain context types.
  EXPECT_TRUE(IsMV3AllowedContextType(context_type));
#endif
}

ExtensionPlatformBrowserTest::~ExtensionPlatformBrowserTest() = default;

void ExtensionPlatformBrowserTest::SetUp() {
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
  PlatformBrowserTest::SetUp();
}

void ExtensionPlatformBrowserTest::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();

  RegisterPathProvider();
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

  web_contents_ = GetActiveWebContents()->GetWeakPtr();
}

void ExtensionPlatformBrowserTest::TearDown() {
  web_contents_.reset();
  PlatformBrowserTest::TearDown();
}

const Extension* ExtensionPlatformBrowserTest::LoadExtension(
    const base::FilePath& path) {
  return LoadExtension(path, {});
}

const Extension* ExtensionPlatformBrowserTest::LoadExtension(
    const base::FilePath& path,
    const LoadOptions& options) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  base::FilePath extension_path;
  if (!extensions::browser_test_util::ModifyExtensionIfNeeded(
          options, context_type_, GetTestPreCount(), temp_dir_.GetPath(), path,
          &extension_path)) {
    return nullptr;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ChromeTestExtensionLoader loader(profile());
#else
  PlatformTestExtensionLoader loader(profile());
#endif
  loader.set_allow_incognito_access(options.allow_in_incognito);
  loader.set_allow_file_access(options.allow_file_access);
  loader.set_ignore_manifest_warnings(options.ignore_manifest_warnings);
  loader.set_wait_for_renderers(options.wait_for_renderers);

  if (options.install_param != nullptr) {
    loader.set_install_param(options.install_param);
  }

  // TODO(crbug.com/373434594): Support TestServiceWorkerContextObserver for the
  // wait_for_registration_stored option.
  CHECK(!options.wait_for_registration_stored);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(extension_path);
  last_loaded_extension_id_ = extension->id();
  return extension.get();
}

void ExtensionPlatformBrowserTest::DisableExtension(
    const std::string& extension_id,
    int disable_reasons) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionSystem::Get(profile())->extension_service()->DisableExtension(
      extension_id, disable_reasons);
#else
  DesktopAndroidExtensionSystem* extension_system =
      static_cast<DesktopAndroidExtensionSystem*>(
          ExtensionSystem::Get(profile()));
  ASSERT_TRUE(extension_system);
  extension_system->DisableExtension(extension_id, disable_reasons);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

content::WebContents* ExtensionPlatformBrowserTest::GetActiveWebContents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

Profile* ExtensionPlatformBrowserTest::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* ExtensionPlatformBrowserTest::web_contents() {
  return web_contents_.get();
}

}  // namespace extensions
