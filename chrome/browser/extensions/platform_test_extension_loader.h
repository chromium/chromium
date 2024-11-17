// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLATFORM_TEST_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_PLATFORM_TEST_EXTENSION_LOADER_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionRegistry;
class ExtensionService;
class ExtensionSystem;

// A test class to help with loading packed or unpacked extensions. Designed to
// be used by both browser tests and unit tests. Note that this should be used
// for a single extension, and is designed to be used on the stack (rather than
// as a test suite member).
//
// NOTE: This is temporary code that exists until ExtensionService has been
// ported to desktop Android. ChromeTestExtensionLoader has too many
// dependencies on ExtensionService and UI parts of //chrome/browser.
class PlatformTestExtensionLoader {
 public:
  explicit PlatformTestExtensionLoader(
      content::BrowserContext* browser_context);

  PlatformTestExtensionLoader(const PlatformTestExtensionLoader&) = delete;
  PlatformTestExtensionLoader& operator=(const PlatformTestExtensionLoader&) =
      delete;

  ~PlatformTestExtensionLoader();

  // Loads the extension specified by |file_path|. Works for both packed and
  // unpacked extensions.
  scoped_refptr<const Extension> LoadExtension(const base::FilePath& file_path);

  void set_allow_file_access(bool allow_file_access) {
    allow_file_access_ = allow_file_access;
  }
  void set_allow_incognito_access(bool allow_incognito_access) {
    allow_incognito_access_ = allow_incognito_access;
  }
  void set_require_modern_manifest_version(bool require_modern_version) {
    require_modern_manifest_version_ = require_modern_version;
  }
  void set_ignore_manifest_warnings(bool ignore_manifest_warnings) {
    ignore_manifest_warnings_ = ignore_manifest_warnings;
  }
  void set_install_param(const std::string& install_param) {
    install_param_ = install_param;
  }
  void set_wait_for_renderers(bool wait_for_renderers) {
    wait_for_renderers_ = wait_for_renderers;
  }

 private:
  // Attempts to parse and load an extension from the given `file_path` and add
  // it to the extensions system (which will also activate the extension).
  // Returns the extension on success; on failure, returns null and adds a test
  // failure.
  scoped_refptr<const Extension> LoadExtensionFromDirectory(
      const base::FilePath& file_path);

  // Verifies that the permissions of the loaded extension are correct.
  // Returns false if they are not.
  bool VerifyPermissions(const Extension* extension);

  // Checks for any install warnings associated with the extension.
  bool CheckInstallWarnings(const Extension& extension);

  // Waits for the extension to finish setting up.
  bool WaitForExtensionReady(const Extension& extension);

  // The associated context and services.
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;
  raw_ptr<ExtensionSystem> extension_system_ = nullptr;
  raw_ptr<ExtensionService> extension_service_ = nullptr;
  raw_ptr<ExtensionRegistry> extension_registry_ = nullptr;

  // The extension id of the loaded extension.
  ExtensionId extension_id_;

  // An install param to use with the loaded extension.
  std::optional<std::string> install_param_;

  // Whether or not to allow file access by default to the extension.
  std::optional<bool> allow_file_access_;

  // Whether or not to allow incognito access by default to the extension.
  std::optional<bool> allow_incognito_access_;

  // Whether or not to ignore manifest warnings during installation.
  bool ignore_manifest_warnings_ = false;

  // Whether or not to enforce a minimum manifest version requirement.
  bool require_modern_manifest_version_ = true;

  // Whether to wait for extension renderers to be ready before continuing.
  // If unspecified, this will default to true if there is at least one existent
  // renderer and false otherwise (this roughly maps to "true in browser tests,
  // false in unit tests").
  std::optional<bool> wait_for_renderers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLATFORM_TEST_EXTENSION_LOADER_H_
