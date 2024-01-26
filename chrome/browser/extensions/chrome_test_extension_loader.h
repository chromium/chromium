// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_TEST_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_TEST_EXTENSION_LOADER_H_

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
class ChromeTestExtensionLoader {
 public:
  explicit ChromeTestExtensionLoader(content::BrowserContext* browser_context);

  ChromeTestExtensionLoader(const ChromeTestExtensionLoader&) = delete;
  ChromeTestExtensionLoader& operator=(const ChromeTestExtensionLoader&) =
      delete;

  ~ChromeTestExtensionLoader();

  // Loads the extension specified by |file_path|. Works for both packed and
  // unpacked extensions.
  scoped_refptr<const Extension> LoadExtension(const base::FilePath& file_path);

  // A limited asynchronous version of LoadExtension. It only supports unpacked
  // extensions and the callback is run as soon as the OnExtensionLoaded fires.
  // It also does not support any of the custom settings below.
  void LoadUnpackedExtensionAsync(
      const base::FilePath& file_path,
      base::OnceCallback<void(const Extension*)> callback);

  // Myriad different settings. See the member variable declarations for
  // explanations and defaults.
  // Prefer using these setters rather than adding n different
  // LoadExtensionWith* variants (that's not scalable).
  void set_expected_id(const std::string& expected_id) {
    expected_id_ = expected_id;
  }
  void add_creation_flag(Extension::InitFromValueFlags flag) {
    creation_flags_ |= flag;
  }
  void set_creation_flags(int flags) { creation_flags_ = flags; }
  void set_location(mojom::ManifestLocation location) { location_ = location; }
  void set_should_fail(bool should_fail) { should_fail_ = should_fail; }
  void set_pack_extension(bool pack_extension) {
    pack_extension_ = pack_extension;
  }
  void set_install_immediately(bool install_immediately) {
    install_immediately_ = install_immediately;
  }
  void set_grant_permissions(bool grant_permissions) {
    grant_permissions_ = grant_permissions;
  }
  void set_allow_file_access(bool allow_file_access) {
    allow_file_access_ = allow_file_access;
  }
  void set_allow_incognito_access(bool allow_incognito_access) {
    allow_incognito_access_ = allow_incognito_access;
  }
  void set_ignore_manifest_warnings(bool ignore_manifest_warnings) {
    ignore_manifest_warnings_ = ignore_manifest_warnings;
  }
  void set_require_modern_manifest_version(bool require_modern_version) {
    require_modern_manifest_version_ = require_modern_version;
  }
  void set_install_param(const std::string& install_param) {
    install_param_ = install_param;
  }
  void set_wait_for_renderers(bool wait_for_renderers) {
    wait_for_renderers_ = wait_for_renderers;
  }
  void set_pem_path(const base::FilePath& pem_path) { pem_path_ = pem_path; }

 private:
  // Packs the extension at |unpacked_path| and returns the path to the created
  // crx. Note that the created crx is tied to the lifetime of |this|.
  base::FilePath PackExtension(const base::FilePath& unpacked_path);

  // Loads the crx pointed to by |crx_path|.
  scoped_refptr<const Extension> LoadCrx(const base::FilePath& crx_path);

  // Loads the unpacked extension pointed to by |unpacked_path|.
  scoped_refptr<const Extension> LoadUnpacked(
      const base::FilePath& unpacked_path);

  // Checks that the permissions of the loaded extension are correct
  // and updates them if necessary.
  void CheckPermissions(const Extension* extension);

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

  // A temporary directory for packing extensions.
  base::ScopedTempDir temp_dir_;

  // The extension id of the loaded extension.
  ExtensionId extension_id_;

  // A provided PEM path to use. If not provided, a temporary one will be
  // created.
  base::FilePath pem_path_;

  // The expected extension id, if any.
  std::string expected_id_;

  // An install param to use with the loaded extension.
  std::optional<std::string> install_param_;

  // Any creation flags (see Extension::InitFromValueFlags) to use for the
  // extension. Only used for crx installs.
  int creation_flags_ = Extension::NO_FLAGS;

  // The install location of the added extension. Not valid for unpacked
  // extensions.
  mojom::ManifestLocation location_ = mojom::ManifestLocation::kInternal;

  // Whether or not the extension load should fail.
  bool should_fail_ = false;

  // Whether or not to always pack the extension before loading it. Otherwise,
  // the extension will be loaded as an unpacked extension.
  bool pack_extension_ = false;

  // Whether or not to install the extension immediately. Only used for crx
  // installs.
  bool install_immediately_ = true;

  // Whether or not to automatically grant permissions to the installed
  // extension. Only used for crx installs.
  bool grant_permissions_ = true;

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

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_TEST_EXTENSION_LOADER_H_
