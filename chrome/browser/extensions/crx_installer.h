// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/preload_check.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"

class ExtensionServiceTest;
class ScopedProfileKeepAlive;
class SkBitmap;

namespace base {
class SequencedTaskRunner;
}

namespace extensions {
class CrxInstallError;
class ExtensionService;
class ExtensionUpdaterTest;
enum class InstallationStage;
class MockCrxInstaller;
class PreloadCheckGroup;

// This class installs a crx file into a profile.
//
// Installing a CRX is a multi-step process, including unpacking the crx,
// validating it, prompting the user, and installing. Since many of these
// steps must occur on the file thread, this class contains a copy of all data
// necessary to do its job. (This also minimizes external dependencies for
// easier testing).
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessHost.
//
// Additionally, we hold a reference to our own client so that it lives at least
// long enough to receive the result of unpacking.
//
// IMPORTANT: Callers should keep a reference to a CrxInstaller while they are
// working with it, eg:
//
// scoped_refptr<CrxInstaller> installer(new CrxInstaller(...));
// installer->set_foo();
// installer->set_bar();
// installer->InstallCrx(...);
//
// Installation is aborted if the extension service learns that Chrome is
// terminating during the install. We can't listen for the app termination
// notification here in this class because it can be destroyed on any thread
// and won't safely be able to clean up UI thread notification listeners.
class CrxInstaller : public SandboxedUnpackerClient, public ProfileObserver {
 public:
  // A callback to be executed when the install finishes.
  using InstallerResultCallback = ExtensionSystem::InstallUpdateCallback;

  using ExpectationsVerifiedCallback = base::OnceClosure;

  // Used in histograms; do not change order.
  enum OffStoreInstallAllowReason {
    OffStoreInstallDisallowed,
    OffStoreInstallAllowedFromSettingsPage,
    OffStoreInstallAllowedBecausePref,
    OffStoreInstallAllowedInTest,
    NumOffStoreInstallAllowReasons
  };

  // Used to indicate if host permissions should be withheld during
  // installation.
  enum WithholdingBehavior { kWithholdPermissions, kDontWithholdPermissions };

  CrxInstaller(const CrxInstaller&) = delete;
  CrxInstaller& operator=(const CrxInstaller&) = delete;

  // Extensions will be installed into service->install_directory(), then
  // registered with |service|. This does a silent install - see below for
  // other options.
  static scoped_refptr<CrxInstaller> CreateSilent(ExtensionService* service);

  // Same as above, but use |client| to generate a confirmation prompt.
  static scoped_refptr<CrxInstaller> Create(
      ExtensionService* service,
      std::unique_ptr<ExtensionInstallPrompt> client);

  // Same as the previous method, except use the |approval| to bypass the
  // prompt. Note that the caller retains ownership of |approval|.
  static scoped_refptr<CrxInstaller> Create(
      ExtensionService* service,
      std::unique_ptr<ExtensionInstallPrompt> client,
      const WebstoreInstaller::Approval* approval);

  // Install the crx in |source_file|. The file must be a CRX3. A publisher
  // proof in the file is required unless off-webstore installation is allowed.
  void InstallCrx(const base::FilePath& source_file);

  // Install the crx in |source_file|.
  virtual void InstallCrxFile(const CRXFileInfo& source_file);

  // Install the unpacked crx in |unpacked_dir|.
  // If |delete_source_| is true, |unpacked_dir| will be removed at the end of
  // the installation.
  void InstallUnpackedCrx(const ExtensionId& extension_id,
                          const std::string& public_key,
                          const base::FilePath& unpacked_dir);

  // Convert the specified user script into an extension and install it.
  void InstallUserScript(const base::FilePath& source_file,
                         const GURL& download_url);

  // Update the extension |extension_id| with the unpacked crx in
  // |unpacked_dir|.
  // If |delete_source_| is true, |unpacked_dir| will be removed at the end of
  // the update.
  void UpdateExtensionFromUnpackedCrx(const ExtensionId& extension_id,
                                      const std::string& public_key,
                                      const base::FilePath& unpacked_dir);

  void OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload payload);

  void InitializeCreationFlagsForUpdate(const Extension* extension,
                                        const int initial_flags);

  // Adds a callback that will be run once the installation finishes
  // (successfully or not).
  // The added callbacks will be run in the order in which they were added
  // (FIFO).
  // Virtual for testing.
  virtual void AddInstallerCallback(InstallerResultCallback callback);

  int creation_flags() const { return creation_flags_; }
  void set_creation_flags(int val) { creation_flags_ = val; }

  const base::FilePath& source_file() const { return source_file_; }

  mojom::ManifestLocation install_source() const { return install_source_; }
  void set_install_source(mojom::ManifestLocation source) {
    install_source_ = source;
  }

  const ExtensionId& expected_id() const { return expected_id_; }
  void set_expected_id(const ExtensionId& val) { expected_id_ = val; }

  // Expected SHA256 hash sum for the package.
  const std::string& expected_hash() const { return expected_hash_; }
  void set_expected_hash(const std::string& val) { expected_hash_ = val; }

  // Set the exact version the installed extension should have. If
  // |fail_install_if_unexpected| is true, installation will fail if the actual
  // version doesn't match. If it is false, the installation will still
  // be performed, but the extension will not be granted any permissions.
  void set_expected_version(const base::Version& val,
                            bool fail_install_if_unexpected) {
    expected_version_ = val;
    fail_install_if_unexpected_version_ = fail_install_if_unexpected;
  }

  bool delete_source() const { return delete_source_; }
  void set_delete_source(bool val) { delete_source_ = val; }

  bool allow_silent_install() const { return allow_silent_install_; }
  void set_allow_silent_install(bool val) { allow_silent_install_ = val; }

  bool grant_permissions() const { return grant_permissions_; }
  void set_grant_permissions(bool val) { grant_permissions_ = val; }

  bool is_gallery_install() const {
    return (creation_flags_ & Extension::FROM_WEBSTORE) > 0;
  }
  void set_is_gallery_install(bool val) {
    if (val)
      creation_flags_ |= Extension::FROM_WEBSTORE;
    else
      creation_flags_ &= ~Extension::FROM_WEBSTORE;
  }
  void set_withhold_permissions();

  // If |apps_require_extension_mime_type_| is set to true, be sure to set
  // |original_mime_type_| as well.
  void set_apps_require_extension_mime_type(
      bool apps_require_extension_mime_type) {
    apps_require_extension_mime_type_ = apps_require_extension_mime_type;
  }

  void set_original_mime_type(const std::string& original_mime_type) {
    original_mime_type_ = original_mime_type;
  }

  extension_misc::CrxInstallCause install_cause() const {
    return install_cause_;
  }
  void set_install_cause(extension_misc::CrxInstallCause install_cause) {
    install_cause_ = install_cause;
  }

  OffStoreInstallAllowReason off_store_install_allow_reason() const {
    return off_store_install_allow_reason_;
  }
  void set_off_store_install_allow_reason(OffStoreInstallAllowReason reason) {
    off_store_install_allow_reason_ = reason;
  }

  void set_page_ordinal(const syncer::StringOrdinal& page_ordinal) {
    page_ordinal_ = page_ordinal;
  }

  void set_error_on_unsupported_requirements(bool val) {
    error_on_unsupported_requirements_ = val;
  }

  void set_install_immediately(bool val) {
    set_install_flag(kInstallFlagInstallImmediately, val);
  }
  void set_do_not_sync(bool val) {
    set_install_flag(kInstallFlagDoNotSync, val);
  }
  void set_bypassed_safebrowsing_friction_for_testing(bool val) {
    set_install_flag(kInstallFlagBypassedSafeBrowsingFriction, val);
  }

  // Callback to be invoked when the crx file has passed the expectations check
  // after unpack success and the ownership of the crx file lies with the
  // installer. The callback is passed the ownership of the crx file.
  void set_expectations_verified_callback(
      ExpectationsVerifiedCallback callback);

  bool did_handle_successfully() const { return did_handle_successfully_; }

  Profile* profile() { return profile_; }

  const Extension* extension() { return extension_.get(); }

  // The currently installed version of the extension, for updates. Will be
  // invalid if this isn't an update.
  const base::Version& current_version() const { return current_version_; }

 protected:
  // Run all callbacks received in AddInstallerCallback with the given error.
  // Protected so that FakeCrxInstaller can expose it.
  void RunInstallerCallbacks(const std::optional<CrxInstallError>& error);

 private:
  friend class ::ExtensionServiceTest;
  friend class BookmarkAppInstallFinalizerTest;
  friend class ExtensionUpdaterTest;
  friend class FakeCrxInstaller;
  friend class MockCrxInstaller;

  CrxInstaller(base::WeakPtr<ExtensionService> service_weak,
               std::unique_ptr<ExtensionInstallPrompt> client,
               const WebstoreInstaller::Approval* approval);
  ~CrxInstaller() override;

  // Converts the source user script to an extension.
  void ConvertUserScriptOnSharedFileThread();

  // Called after OnUnpackSuccess check to see whether the install expectations
  // are met and the install process should continue.
  std::optional<CrxInstallError> CheckExpectations(const Extension* extension);

  // Called after OnUnpackSuccess as a last check to see whether the install
  // should complete.
  std::optional<CrxInstallError> AllowInstall(const Extension* extension);

  // To check whether we need to compute hashes or not, we have to make a query
  // to ContentVerifier, and that should be done on the UI thread.
  void ShouldComputeHashesOnUI(scoped_refptr<const Extension> extension,
                               base::OnceCallback<void(bool)> callback);

  // To provide content verifier key to the unpacker.
  void GetContentVerifierKeyOnUI(
      base::OnceCallback<void(ContentVerifierKey)> callback);

  // SandboxedUnpackerClient
  void GetContentVerifierKey(
      base::OnceCallback<void(ContentVerifierKey)> callback) override;
  void ShouldComputeHashesForOffWebstoreExtension(
      scoped_refptr<const Extension> extension,
      base::OnceCallback<void(bool)> callback) override;
  void OnUnpackFailure(const CrxInstallError& error) override;
  void OnUnpackSuccess(const base::FilePath& temp_dir,
                       const base::FilePath& extension_dir,
                       std::unique_ptr<base::Value::Dict> original_manifest,
                       const Extension* extension,
                       const SkBitmap& install_icon,
                       base::Value::Dict ruleset_install_prefs) override;
  void OnStageChanged(InstallationStage stage) override;

  // ProfileObserver
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Called on the UI thread to start the requirements, policy and blocklist
  // checks on the extension.
  void CheckInstall();

  // Runs on the UI thread. Callback from PreloadCheckGroup.
  void OnInstallChecksComplete(const PreloadCheck::Errors& errors);

  // Runs on the UI thread. Confirms the installation to the ExtensionService.
  void ConfirmInstall();

  // Runs on the UI thread. Updates the creation flags for the extension and
  // calls CompleteInstall().
  void UpdateCreationFlagsAndCompleteInstall(
      WithholdingBehavior withholding_behavior);

  // Runs on File thread. Install the unpacked extension into the profile and
  // notify the frontend.
  void CompleteInstall(bool updates_from_webstore);

  // Reloads extension on File thread and reports installation result back
  // to UI thread.
  void ReloadExtensionAfterInstall(const base::FilePath& version_dir);

  // Result reporting.
  void ReportFailureFromSharedFileThread(const CrxInstallError& error);
  void ReportFailureFromUIThread(const CrxInstallError& error);
  void ReportSuccessFromSharedFileThread();
  void ReportSuccessFromUIThread();
  // Always report from the UI thread.
  void ReportInstallationStage(InstallationStage stage);
  void NotifyCrxInstallBegin();
  void NotifyCrxInstallComplete(const std::optional<CrxInstallError>& error);

  // Deletes temporary directory and crx file if needed.
  void CleanupTempFiles();

  // Checks whether the current installation is initiated by the user from
  // the extension settings page to update an existing extension or app.
  void CheckUpdateFromSettingsPage();

  // Show re-enable prompt if the update is initiated from the settings page
  // and needs additional permissions.
  void ConfirmReEnable();

  // OnUnpackSuccess() gets called on the unpacker sequence. It calls this
  // method on the shared file sequence, to avoid race conditions.
  virtual void OnUnpackSuccessOnSharedFileThread(
      base::FilePath temp_dir,
      base::FilePath extension_dir,
      std::unique_ptr<base::Value::Dict> original_manifest,
      scoped_refptr<const Extension> extension,
      SkBitmap install_icon,
      base::Value::Dict ruleset_install_prefs);

  void set_install_flag(int flag, bool val) {
    if (val)
      install_flags_ |= flag;
    else
      install_flags_ &= ~flag;
  }

  // Returns |unpacker_task_runner_|. Initializes it if it's still nullptr.
  base::SequencedTaskRunner* GetUnpackerTaskRunner();

  // The Profile the extension is being installed in.
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Prevent Profile destruction until the CrxInstaller is done.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  // ... but |profile_| could still get destroyed early, if Chrome shuts down
  // completely. We need to perform some cleanup if that happens.
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  // The extension being installed.
  scoped_refptr<const Extension> extension_;

  // The file we're installing.
  base::FilePath source_file_;

  // The URL the file was downloaded from.
  GURL download_url_;

  // The directory extensions are installed to.
  const base::FilePath install_directory_;

  // The location the installation came from (bundled with Chromium, registry,
  // manual install, etc). This metadata is saved with the installation if
  // successful. Defaults to INTERNAL.
  mojom::ManifestLocation install_source_;

  // Indicates whether the user has already approved the extension to be
  // installed. If true, |expected_manifest_| and |expected_id_| must match
  // those of the CRX.
  bool approved_;

  // For updates, external and webstore installs we have an ID we're expecting
  // the extension to contain.
  ExtensionId expected_id_;

  // An expected hash sum for the .crx file.
  std::string expected_hash_;

  // A copy of the expected manifest, before any transformations like
  // localization have taken place. If |approved_| is true, then the extension's
  // manifest must match this for the install to proceed.
  std::unique_ptr<base::Value::Dict> expected_manifest_;

  // The level of checking when comparing the actual manifest against
  // the |expected_manifest_|.
  WebstoreInstaller::ManifestCheckLevel expected_manifest_check_level_;

  // If valid, specifies the minimum version we'll install. Installation will
  // fail if the actual version is smaller.
  base::Version minimum_version_;

  // If valid, contains the expected version of the extension we're installing.
  // Important for external sources, where claiming the wrong version could
  // cause unnecessary unpacking of an extension at every restart.
  // See also |fail_install_if_unexpected_version_|!
  base::Version expected_version_;

  // If true, installation will fail if the actual version doesn't match
  // |expected_version_|. If false, the extension will still be installed, but
  // not granted any permissions.
  bool fail_install_if_unexpected_version_;

  // Whether manual extension installation is enabled. We can't just check this
  // before trying to install because themes and bookmark apps are special-cased
  // to always be allowed.
  bool extensions_enabled_;

  // Whether we're supposed to delete the source file on destruction. Defaults
  // to false.
  bool delete_source_;

  // The ordinal of the NTP apps page |extension_| will be shown on.
  syncer::StringOrdinal page_ordinal_;

  // A copy of the unmodified original manifest, before any transformations like
  // localization have taken place.
  std::unique_ptr<base::Value::Dict> original_manifest_;

  // If valid, contains the current version of the extension we're
  // installing (for upgrades).
  base::Version current_version_;

  // The icon we will display in the installation UI, if any.
  std::unique_ptr<SkBitmap> install_icon_;

  // The temp directory extension resources were unpacked to. We own this and
  // must delete it when we are done with it.
  base::FilePath temp_dir_;

  // The frontend we will report results back to.
  base::WeakPtr<ExtensionService> service_weak_;

  // The client we will work with to do the installation. This can be NULL, in
  // which case the install is silent.
  std::unique_ptr<ExtensionInstallPrompt> client_;

  // The root of the unpacked extension directory. This is a subdirectory of
  // temp_dir_, so we don't have to delete it explicitly.
  base::FilePath unpacked_extension_root_;

  // True when the CRX being installed was just downloaded.
  // Used to trigger extra checks before installing.
  bool apps_require_extension_mime_type_;

  // Allows for the possibility of a normal install (one in which a |client|
  // is provided in the ctor) to proceed without showing the permissions prompt
  // dialog.
  bool allow_silent_install_;

  // Allows for the possibility of an installation without granting any
  // permissions to the extension.
  bool grant_permissions_;

  // The value of the content type header sent with the CRX.
  // Ignorred unless |require_extension_mime_type_| is true.
  std::string original_mime_type_;

  // What caused this install?  Used only for histograms that report
  // on failure rates, broken down by the cause of the install.
  extension_misc::CrxInstallCause install_cause_;

  // Creation flags to use for the extension.  These flags will be used
  // when calling Extension::Create() by the crx installer.
  int creation_flags_;

  // Whether to allow off store installation.
  OffStoreInstallAllowReason off_store_install_allow_reason_;

  // Whether the installation was handled successfully. This is used to
  // indicate to the client whether the file should be removed and any UI
  // initiating the installation can be removed. This is different than whether
  // there was an error; if there was an error that rejects installation we
  // still consider the installation 'handled'.
  bool did_handle_successfully_;

  // Whether we should produce an error if the manifest declares requirements
  // that are not met. If false and there is an unmet requirement, the install
  // will continue but the extension will be distabled.
  bool error_on_unsupported_requirements_;

  // Sequenced task runner where most file I/O operations will be performed.
  scoped_refptr<base::SequencedTaskRunner> shared_file_task_runner_;

  // Sequenced task runner where the SandboxedUnpacker will run. Because the
  // unpacker uses its own temp dir, it won't hit race conditions, and can use a
  // separate task runner per instance (for better performance).
  //
  // Lazily initialized by GetUnpackerTaskRunner().
  scoped_refptr<base::SequencedTaskRunner> unpacker_task_runner_;

  // Used to show the install dialog.
  ExtensionInstallPrompt::ShowDialogCallback show_dialog_callback_;

  // Whether the update is initiated by the user from the extension settings
  // page.
  bool update_from_settings_page_;

  // The flags for ExtensionService::OnExtensionInstalled.
  int install_flags_;

  // Install prefs needed for the Declarative Net Request API.
  base::Value::Dict ruleset_install_prefs_;

  // Checks that may run before installing the extension.
  std::unique_ptr<PreloadCheck> policy_check_;
  std::unique_ptr<PreloadCheck> requirements_check_;
  std::unique_ptr<PreloadCheck> blocklist_check_;

  // Runs the above checks.
  std::unique_ptr<PreloadCheckGroup> check_group_;

  // Invoked when the install is completed.
  std::vector<InstallerResultCallback> installer_callbacks_;

  // Invoked when the expectations from CRXFileInfo match with the crx file
  // after unpack success.
  ExpectationsVerifiedCallback expectations_verified_callback_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
