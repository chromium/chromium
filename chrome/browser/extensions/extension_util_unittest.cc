// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"
#include "url/gurl.h"

namespace extensions {

using ExtensionUtilUnittest = ExtensionServiceTestBase;

TEST_F(ExtensionUtilUnittest, SetAllowFileAccess) {
  InitializeEmptyExtensionService();
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2,
           "permissions": ["<all_urls>"]
         })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // An unpacked extension would get file access by default, so disabled it on
  // the loader.
  loader.set_allow_file_access(false);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(dir.UnpackedPath());
  const std::string extension_id = extension->id();

  GURL file_url("file://etc");
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents.get()).id();

  // Initially the file access pref will be false and the extension will not be
  // able to capture a file URL page.
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Calling SetAllowFileAccess should reload the extension with file access.
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), true);
    extension = observer.WaitForExtensionInstalled();
  }

  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Removing the file access should reload the extension again back to not
  // having file access.
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), false);
    extension = observer.WaitForExtensionInstalled();
  }

  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));
}

TEST_F(ExtensionUtilUnittest, SetAllowFileAccessWhileDisabled) {
  InitializeEmptyExtensionService();
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2,
           "permissions": ["<all_urls>"]
         })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // An unpacked extension would get file access by default, so disabled it on
  // the loader.
  loader.set_allow_file_access(false);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(dir.UnpackedPath());
  const std::string extension_id = extension->id();

  GURL file_url("file://etc");
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents.get()).id();

  // Initially the file access pref will be false and the extension will not be
  // able to capture a file URL page.
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Disabling the extension then calling SetAllowFileAccess should reload the
  // extension with file access.
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), true);
    extension = observer.WaitForExtensionInstalled();
  }
  // The extension should still be disabled.
  EXPECT_FALSE(service()->IsExtensionEnabled(extension_id));

  service()->EnableExtension(extension_id);
  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Disabling the extension and then removing the file access should reload it
  // again back to not having file access. Regression test for
  // crbug.com/1385343.
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), false);
    extension = observer.WaitForExtensionInstalled();
  }
  // The extension should still be disabled.
  EXPECT_FALSE(service()->IsExtensionEnabled(extension_id));

  service()->EnableExtension(extension_id);
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));
}

}  // namespace extensions
