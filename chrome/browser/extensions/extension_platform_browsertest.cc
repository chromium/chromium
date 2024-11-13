// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_platform_browsertest.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/platform_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_paths.h"

namespace extensions {

ExtensionPlatformBrowserTest::ExtensionPlatformBrowserTest(
    ContextType context_type)
    : context_type_(context_type) {}

ExtensionPlatformBrowserTest::~ExtensionPlatformBrowserTest() = default;

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

  PlatformTestExtensionLoader loader(profile());
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

  scoped_refptr<const Extension> extension = loader.LoadExtension(path);
  last_loaded_extension_id_ = extension->id();
  return extension.get();
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
