// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_
#define CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_

#include <atomic>
#include <map>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "extensions/common/extension_id.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class GURL;
class Profile;

namespace base {
class Version;
}  // namespace base

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {
class MockConfigurationPolicyProvider;
}  // namespace policy

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace ash {
class DeviceStateMixin;
class EmbeddedPolicyTestServerMixin;
}  // namespace ash

namespace policy {
class DevicePolicyCrosTestHelper;
}  // namespace policy

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A mixin that allows to force-install an extension/app via user or device
// policy.
//
// Encapsulates the following operations:
// * generating a CRX file,
// * generating an update manifest,
// * hosting the update manifest and the CRX via an embedded test server,
// * configuring the force installation in the user/device policy.
//
// Example usage (for force-installing using the user-level policy):
//
//   class MyTestFixture : ... {
//    protected:
//     void SetUpOnMainThread() override {
//       ...
//       force_install_mixin_.InitWithMockPolicyProvider(...);
//     }
//     ExtensionForceInstallMixin force_install_mixin_{&mixin_host_};
//   };
//   IN_PROC_BROWSER_TEST_F(...) {
//     EXPECT_TRUE(force_install_mixin_.ForceInstallFromCrx(...));
//   }
//
// Internally, the mixin owns an embedded test server that hosts files needed
// for the forced installation:
// * "/<extension_id>.xml" - update manifests referred to by policies,
// * "/<extension_id>-<version>.crx" - CRX packages referred to by the update
//   manifests.
class ExtensionForceInstallMixin final : public InProcessBrowserTestMixin {
 public:
  // The type of the waiting mode for the force installation operation.
  enum class WaitMode {
    // Don't wait, and return immediately.
    kNone,
    // Wait until the force-installation pref is updated.
    kPrefSet,
    // Wait until the extension is loaded.
    kLoad,
    // Wait until the extension's background page is loaded for the first time.
    kBackgroundPageFirstLoad,
    // Wait until the extension is loaded and its (presumably javascript
    // typescript) code sends the hard-coded message 'ready'. The extension
    // needs to send a message via `chrome.test.sendMessage('ready')`.
    kReadyMessageReceived,
  };

  // The type of the waiting mode for the force-installed extension update.
  enum class UpdateWaitMode {
    // Don't wait, and return immediately.
    kNone,
    // TODO(crbug.com/40697472): Add other wait modes as necessary.
  };

  // The type of the server error that should be simulated.
  enum class ServerErrorMode {
    // No error - network requests will succeed.
    kNone,
    // Don't respond to any network request at all (a rough equivalent of an
    // absent network).
    kHung,
    // Respond with the HTTP 500 Internal Server Error.
    kInternalError,
  };

  explicit ExtensionForceInstallMixin(InProcessBrowserTestMixinHost* host);
  ExtensionForceInstallMixin(const ExtensionForceInstallMixin&) = delete;
  ExtensionForceInstallMixin& operator=(const ExtensionForceInstallMixin&) =
      delete;
  ~ExtensionForceInstallMixin() override;

  // Use one of the Init*() methods below to initialize the object before
  // calling any other method.
  // Note: The |profile| argument is optional; if it's null, only the `kNone`
  // waiting mode is allowed, and Get...Extension() methods are fobidden.

  void InitWithMockPolicyProvider(
      Profile* profile,
      policy::MockConfigurationPolicyProvider* mock_policy_provider);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InitWithDeviceStateMixin(Profile* profile,
                                ash::DeviceStateMixin* device_state_mixin);
  void InitWithDevicePolicyCrosTestHelper(
      Profile* profile,
      policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper);
  void InitWithEmbeddedPolicyMixin(
      Profile* profile,
      ash::EmbeddedPolicyTestServerMixin* policy_test_server_mixin,
      policy::UserPolicyBuilder* user_policy_builder,
      const std::string& account_id,
      const std::string& policy_type);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Force-installs the CRX file |crx_path|; under the hood, generates an update
  // manifest and serves it and the CRX file by the embedded test server.
  // |extension_id| - if non-null, will be set to the installed extension ID.
  // |extension_version| - if non-null, will be set to the installed extension
  // version.
  [[nodiscard]] bool ForceInstallFromCrx(
      const base::FilePath& crx_path,
      WaitMode wait_mode,
      extensions::ExtensionId* extension_id = nullptr,
      base::Version* extension_version = nullptr);
  // Force-installs the extension from the given source directory (which should
  // contain the manifest.json file and all other files of the extension).
  // Under the hood, packs the directory into a CRX file and serves it like
  // ForceInstallFromCrx().
  // |pem_path| - if non-empty, will be used to load the private key for packing
  // the extension; when empty, a random key will be generated.
  // |extension_id| - if non-null, will be set to the installed extension ID.
  // |extension_version| - if non-null, will be set to the installed extension
  // version.
  [[nodiscard]] bool ForceInstallFromSourceDir(
      const base::FilePath& extension_dir_path,
      const std::optional<base::FilePath>& pem_path,
      WaitMode wait_mode,
      extensions::ExtensionId* extension_id = nullptr,
      base::Version* extension_version = nullptr);

  // Updates the served extension to the new version from |crx_path|. It's
  // expected that a ForceInstallFromCrx() call was done previously for this
  // extension.
  // |extension_version| - if non-null, will be set to the CRX'es version.
  [[nodiscard]] bool UpdateFromCrx(const base::FilePath& crx_path,
                                   UpdateWaitMode wait_mode,
                                   base::Version* extension_version = nullptr);
  // Updates the served |extension_id| extension to the new version from
  // |extension_dir_path|. It's expected that a ForceInstallFromSourceDir() call
  // was done previously for this extension.
  // |extension_version| - if non-null, will be set to the extension's version.
  [[nodiscard]] bool UpdateFromSourceDir(
      const base::FilePath& extension_dir_path,
      const extensions::ExtensionId& extension_id,
      UpdateWaitMode wait_mode,
      base::Version* extension_version = nullptr);

  // Returns the extension, or null if it's not installed yet.
  const extensions::Extension* GetInstalledExtension(
      const extensions::ExtensionId& extension_id) const;
  // Returns the extension, or null if it's not installed or not enabled yet.
  const extensions::Extension* GetEnabledExtension(
      const extensions::ExtensionId& extension_id) const;

  // Changes the embedded test server's error mode, allowing to simulate
  // network unavailability and errors.
  void SetServerErrorMode(ServerErrorMode server_error_mode);

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

  bool initialized() const { return initialized_; }

 private:
  // Returns the path to the file that is served by the embedded test server
  // under the given name.
  base::FilePath GetPathInServedDir(const std::string& file_name) const;
  // Returns the URL of the update manifest pointing to the embedded test
  // server.
  GURL GetServedUpdateManifestUrl(
      const extensions::ExtensionId& extension_id) const;
  // Returns the URL of the CRX file pointing to the embedded test server.
  GURL GetServedCrxUrl(const extensions::ExtensionId& extension_id,
                       const base::Version& extension_version) const;
  // Makes the given |source_crx_path| file served by the embedded test server.
  bool ServeExistingCrx(const base::FilePath& source_crx_path,
                        const extensions::ExtensionId& extension_id,
                        const base::Version& extension_version);
  // Packs the given |extension_dir_path| (using the |pem_path| if provided or a
  // random key otherwise) and makes the produced CRX file served by the
  // embedded test server.
  bool CreateAndServeCrx(const base::FilePath& extension_dir_path,
                         const std::optional<base::FilePath>& pem_path,
                         const base::Version& extension_version,
                         extensions::ExtensionId* extension_id);
  // Force-installs the CRX file served by the embedded test server.
  bool ForceInstallFromServedCrx(const extensions::ExtensionId& extension_id,
                                 const base::Version& extension_version,
                                 WaitMode wait_mode);
  // Creates an update manifest with the CRX URL pointing to the embedded test
  // server.
  bool CreateAndServeUpdateManifestFile(
      const extensions::ExtensionId& extension_id,
      const base::Version& extension_version);
  // Sets the policy to force-install the given extension from the given update
  // manifest URL.
  bool UpdatePolicy(const extensions::ExtensionId& extension_id,
                    const GURL& update_manifest_url);
  // Waits until the extension's given version is installed and gets into the
  // requested mode. Does nothing if |wait_mode| is |kNone|.
  bool WaitForExtensionUpdate(const extensions::ExtensionId& extension_id,
                              const base::Version& extension_version,
                              UpdateWaitMode wait_mode);

  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer embedded_test_server_;
  bool initialized_ = false;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  raw_ptr<policy::MockConfigurationPolicyProvider> mock_policy_provider_ =
      nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  raw_ptr<ash::DeviceStateMixin> device_state_mixin_ = nullptr;
  raw_ptr<policy::DevicePolicyCrosTestHelper> device_policy_cros_test_helper_ =
      nullptr;
  raw_ptr<ash::EmbeddedPolicyTestServerMixin> policy_test_server_mixin_ =
      nullptr;
  raw_ptr<policy::UserPolicyBuilder> user_policy_builder_ = nullptr;
  // |account_id_| and |policy_type_| are only used with
  // |policy_test_server_mixin_|.
  std::string account_id_;
  std::string policy_type_;
#endif
  // Mapping from the extension ID to the PEM file (the supplied or a randomly
  // generated one). It's not populated for extensions installed from CRX files,
  // since there's no PEM file available in that case.
  std::map<extensions::ExtensionId, base::FilePath> extension_id_to_pem_path_;
  // The current error mode. Stored in an atomic variable, as the server's
  // request handlers are reading from it on IO thread.
  std::atomic<ServerErrorMode> server_error_mode_{ServerErrorMode::kNone};
};

#endif  // CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_
