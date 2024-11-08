// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_

#include "base/files/file_path.h"
#include "chrome/test/base/platform_browser_test.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class Extension;

class ExtensionPlatformBrowserTest : public PlatformBrowserTest {
 public:
  // Different types of extension's lazy background contexts used in some tests.
  enum class ContextType {
    // TODO(crbug.com/40785880): Get rid of this value when we can use
    // std::optional in the LoadOptions struct.
    // No specific context type.
    kNone,
    // A non-persistent background page/JS based extension.
    kEventPage,
    // A Service Worker based extension.
    kServiceWorker,
    // A Service Worker based extension that uses MV2.
    kServiceWorkerMV2,
    // An extension with a persistent background page.
    kPersistentBackground,
    // Use the value from the manifest. This is used when the test
    // has been parameterized but the particular extension should
    // be loaded without using the parameterized type. Typically,
    // this is used when a test loads another extension that is
    // not parameterized.
    kFromManifest,
  };

  explicit ExtensionPlatformBrowserTest(
      ContextType context_type = ContextType::kNone);
  ExtensionPlatformBrowserTest(const ExtensionPlatformBrowserTest&) = delete;
  ExtensionPlatformBrowserTest& operator=(const ExtensionPlatformBrowserTest&) =
      delete;
  ~ExtensionPlatformBrowserTest() override;

 protected:
  struct LoadOptions {
    // Allows the extension to run in incognito mode.
    bool allow_in_incognito = false;

    // Allows file access for the extension.
    bool allow_file_access = false;

    // Doesn't fail when the loaded manifest has warnings (should only be used
    // when testing deprecated features).
    bool ignore_manifest_warnings = false;

    // Waits for extension renderers to fully load.
    bool wait_for_renderers = true;

    // An optional install param.
    const char* install_param = nullptr;

    // If this is a Service Worker-based extension, wait for the
    // Service Worker's registration to be stored before returning.
    bool wait_for_registration_stored = false;

    // Loads the extension with location COMPONENT.
    bool load_as_component = false;

    // Changes the "manifest_version" manifest key to 3. Note as of now, this
    // doesn't make any other changes to convert the extension to MV3 other than
    // changing the integer value in the manifest.
    bool load_as_manifest_version_3 = false;

    // Used to force loading the extension with a particular background type.
    // Currently this only support loading an extension as using a service
    // worker.
    ContextType context_type = ContextType::kNone;
  };

  // content::BrowserTestBase:
  void SetUpOnMainThread() override;

  const Extension* LoadExtension(const base::FilePath& path);
  const Extension* LoadExtension(const base::FilePath& path,
                                 const LoadOptions& options);

  // Lower case to match the style of InProcessBrowserTest.
  Profile* profile();

  const ExtensionId& last_loaded_extension_id() {
    return last_loaded_extension_id_;
  }

  // Set to "chrome/test/data/extensions". Derived classes may override.
  base::FilePath test_data_dir_;

  const ContextType context_type_;

 private:
  ExtensionId last_loaded_extension_id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PLATFORM_BROWSERTEST_H_
