// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_
#define CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
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

namespace chromeos {
class DeviceStateMixin;
}  // namespace chromeos

namespace policy {
class DevicePolicyCrosTestHelper;
}  // namespace policy

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// A mixin that allows to force-install an extension/app via the device policy.
//
// Encapsulates the following operations:
// * generating a CRX file,
// * generating an update manifest,
// * hosting the update manifest and the CRX via an embedded test server,
// * configuring the force installation in the device policy.
//
// Example usage (for force-installing into the sign-in profile using the device
// policy):
//
//   class MyTestFixture : ... {
//    protected:
//     void SetUpOnMainThread() override {
//       ...
//       force_install_mixin_.InitWithDevicePolicyCrosTestHelper(...);
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
//
// TODO(crbug.com/1090941): Add user policy, auto update.
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
    // Wait until the extension's background page is ready.
    kBackgroundPageReady,
    // Wait until the extension's background page is loaded for the first time.
    kBackgroundPageFirstLoad,
  };

  explicit ExtensionForceInstallMixin(InProcessBrowserTestMixinHost* host);
  ExtensionForceInstallMixin(const ExtensionForceInstallMixin&) = delete;
  ExtensionForceInstallMixin& operator=(const ExtensionForceInstallMixin&) =
      delete;
  ~ExtensionForceInstallMixin() override;

  // Use one of the Init*() methods to initialize the object before calling any
  // other method:

  void InitWithMockPolicyProvider(
      Profile* profile,
      policy::MockConfigurationPolicyProvider* mock_policy_provider);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void InitWithDeviceStateMixin(Profile* profile,
                                chromeos::DeviceStateMixin* device_state_mixin);
  void InitWithDevicePolicyCrosTestHelper(
      Profile* profile,
      policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Force-installs the CRX file |crx_path|; under the hood, generates an update
  // manifest and serves it and the CRX file by the embedded test server.
  // |extension_id| - if non-null, will be set to the installed extension ID.
  // |extension_version| - if non-null, will be set to the installed extension
  // version.
  bool ForceInstallFromCrx(const base::FilePath& crx_path,
                           WaitMode wait_mode,
                           extensions::ExtensionId* extension_id = nullptr,
                           base::Version* extension_version = nullptr)
      WARN_UNUSED_RESULT;
  // Force-installs the extension from the given source directory (which should
  // contain the manifest.json file and all other files of the extension).
  // Under the hood, packs the directory into a CRX file and serves it like
  // ForceInstallFromCrx().
  // |pem_path| - if non-empty, will be used to load the private key for packing
  // the extension; when empty, a random key will be generated.
  // |extension_id| - if non-null, will be set to the installed extension ID.
  // |extension_version| - if non-null, will be set to the installed extension
  // version.
  bool ForceInstallFromSourceDir(
      const base::FilePath& extension_dir_path,
      const base::Optional<base::FilePath>& pem_path,
      WaitMode wait_mode,
      extensions::ExtensionId* extension_id = nullptr,
      base::Version* extension_version = nullptr) WARN_UNUSED_RESULT;

  // Returns the extension, or null if it's not installed yet.
  const extensions::Extension* GetInstalledExtension(
      const extensions::ExtensionId& extension_id) const;
  // Returns the extension, or null if it's not installed or not enabled yet.
  const extensions::Extension* GetEnabledExtension(
      const extensions::ExtensionId& extension_id) const;
  // Returns whether the installed extension's background page is ready.
  bool IsExtensionBackgroundPageReady(
      const extensions::ExtensionId& extension_id) const;

  // InProcessBrowserTestMixin:
  void SetUpOnMainThread() override;

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
                         const base::Optional<base::FilePath>& pem_path,
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

  base::ScopedTempDir temp_dir_;
  net::EmbeddedTestServer embedded_test_server_;
  Profile* profile_ = nullptr;
  policy::MockConfigurationPolicyProvider* mock_policy_provider_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::DeviceStateMixin* device_state_mixin_ = nullptr;
  policy::DevicePolicyCrosTestHelper* device_policy_cros_test_helper_ = nullptr;
#endif
};

#endif  // CHROME_BROWSER_POLICY_EXTENSION_FORCE_INSTALL_MIXIN_H_
