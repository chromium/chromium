// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UNPACKED_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_UNPACKED_INSTALLER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/browser/preload_check.h"
#include "extensions/common/manifest.h"

class Profile;

namespace extensions {

class Extension;
class ExtensionService;
class PreloadCheckGroup;

// Installs and loads an unpacked extension. Because internal state needs to be
// held about the instalation process, only one call to Load*() should be made
// per UnpackedInstaller.
// TODO(erikkay): It might be useful to be able to load a packed extension
// (presumably into memory) without installing it.
class UnpackedInstaller
    : public base::RefCountedThreadSafe<UnpackedInstaller> {
 public:
  using CompletionCallback = base::OnceCallback<void(const Extension* extension,
                                                     const base::FilePath&,
                                                     const std::string&)>;

  UnpackedInstaller(const UnpackedInstaller&) = delete;
  UnpackedInstaller& operator=(const UnpackedInstaller&) = delete;

  static scoped_refptr<UnpackedInstaller> Create(
      ExtensionService* extension_service);

  // Loads the extension from the directory |extension_path|, which is
  // the top directory of a specific extension where its manifest file lives.
  // Errors are reported through LoadErrorReporter. On success,
  // ExtensionService::AddExtension() is called.
  void Load(const base::FilePath& extension_path);

  // Loads the extension from the directory |extension_path|;
  // for use with command line switch --load-extension=path or
  // --load-and-launch-app=path.
  // This is equivalent to Load, except that it reads the extension from
  // |extension_path| synchronously.
  // The return value indicates whether the installation has begun successfully.
  // The id of the extension being loaded is returned in |extension_id|.
  // |only_allow_apps| is used to avoid side-loading of non-app extensions.
  bool LoadFromCommandLine(const base::FilePath& extension_path,
                           std::string* extension_id,
                           bool only_allow_apps);

  // Allows overriding of whether modern manifest versions are required;
  // intended for testing.
  bool require_modern_manifest_version() const {
    return require_modern_manifest_version_;
  }
  void set_require_modern_manifest_version(bool val) {
    require_modern_manifest_version_ = val;
  }

  void set_be_noisy_on_failure(bool be_noisy_on_failure) {
    be_noisy_on_failure_ = be_noisy_on_failure;
  }

  void set_completion_callback(CompletionCallback callback) {
    callback_ = std::move(callback);
  }

  void set_allow_file_access(bool allow) { allow_file_access_ = allow; }

  void set_allow_incognito_access(bool allow) {
    allow_incognito_access_ = allow;
  }

  void set_install_param(const std::string& param) { install_param_ = param; }

 private:
  friend class base::RefCountedThreadSafe<UnpackedInstaller>;

  explicit UnpackedInstaller(ExtensionService* extension_service);
  virtual ~UnpackedInstaller();

  // Must be called from the UI thread. Begin management policy and requirements
  // checks.
  void StartInstallChecks();

  // Callback from PreloadCheckGroup.
  void OnInstallChecksComplete(const PreloadCheck::Errors& errors);

  // Verifies if loading unpacked extensions is allowed.
  bool IsLoadingUnpackedAllowed() const;

  // We change the input extension path to an absolute path, on the file thread.
  // Then we need to check the file access preference, which needs
  // to happen back on the UI thread, so it posts CheckExtensionFileAccess on
  // the UI thread. In turn, once that gets the pref, it goes back to the
  // file thread with LoadWithFileAccess.
  // TODO(yoz): It would be nice to remove this ping-pong, but we need to know
  // what file access flags to pass to file_util::LoadExtension.
  void GetAbsolutePath();
  void CheckExtensionFileAccess();
  void LoadWithFileAccess(int flags);

  // Notify the frontend that an attempt to retry will not be necessary.
  void UnregisterLoadRetryListener();

  // Notify the frontend that there was an error loading an extension.
  void ReportExtensionLoadError(const std::string& error);

  // Passes the extension onto extension service.
  void InstallExtension();

  // Helper to get the Extension::CreateFlags for the installing extension.
  int GetFlags();

  // Helper to load an extension. Should be called on a sequence where file IO
  // is allowed. Loads the extension, validates extension locales and persists
  // the ruleset for the Declarative Net Request API, if needed. In case of an
  // error, returns false and populates |error|.
  bool LoadExtension(mojom::ManifestLocation location,
                     int flags,
                     std::string* error);

  // Reads the Declarative Net Request JSON rulesets for the extension, if it
  // provided any, and persists the indexed rulesets. Returns false and
  // populates |error| in case of an error. Should be called on a sequence where
  // file IO is allowed.
  bool IndexAndPersistRulesIfNeeded(std::string* error);

  // Records command-line extension metrics organized by developer mode, emitted
  // when a command line extension is loaded.
  void RecordCommandLineDeveloperModeMetrics();

  const Extension* extension() { return extension_.get(); }

  // The service we will report results back to.
  base::WeakPtr<ExtensionService> service_weak_;

  // The Profile the extension is being installed in.
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // The pathname of the directory to load from, which is an absolute path
  // after GetAbsolutePath has been called.
  base::FilePath extension_path_;

  // The extension being installed.
  scoped_refptr<Extension> extension_;

  // Whether to require the extension installed to have a modern manifest
  // version.
  bool require_modern_manifest_version_;

  // Whether or not to be noisy (show a dialog) on failure. Defaults to true.
  bool be_noisy_on_failure_;

  // Checks to run before the extension can be installed.
  std::unique_ptr<PreloadCheck> policy_check_;
  std::unique_ptr<PreloadCheck> requirements_check_;

  // Runs the above checks.
  std::unique_ptr<PreloadCheckGroup> check_group_;

  // Install prefs needed for the Declarative Net Request API.
  base::Value::Dict ruleset_install_prefs_;

  CompletionCallback callback_;

  // Override default file access.
  std::optional<bool> allow_file_access_;

  // Override default incognito access.
  std::optional<bool> allow_incognito_access_;

  // Specify an install param.
  std::optional<std::string> install_param_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UNPACKED_INSTALLER_H_
