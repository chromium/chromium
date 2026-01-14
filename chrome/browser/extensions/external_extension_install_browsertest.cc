// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/one_shot_event.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/unpacked_installer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class ExternalExtensionInstallBrowserTest : public ExtensionBrowserTest {
 public:
  ExternalExtensionInstallBrowserTest() = default;
  ~ExternalExtensionInstallBrowserTest() override = default;

  // Reads private key from `private_key_path` and generates extension id using
  // it.
  std::string GetExtensionIdFromPrivateKeyFile(
      const base::FilePath& private_key_path) {
    base::ScopedAllowBlockingForTesting allow_file_io_in_scope;
    std::string private_key_contents;
    EXPECT_TRUE(
        base::ReadFileToString(private_key_path, &private_key_contents));
    std::string private_key_bytes;
    EXPECT_TRUE(
        Extension::ParsePEMKeyBytes(private_key_contents, &private_key_bytes));
    auto signing_key = crypto::keypair::PrivateKey::FromPrivateKeyInfo(
        base::as_byte_span(private_key_bytes));
    std::vector<uint8_t> public_key = signing_key->ToSubjectPublicKeyInfo();
    return crx_file::id_util::GenerateId(public_key);
  }
};

// Verify that an externally installed extension is disabled on update if the
// permissions have increased.
IN_PROC_BROWSER_TEST_F(ExternalExtensionInstallBrowserTest,
                       AddPermissionOnUpdate) {
  // Setup.
  TestExtensionDir test_dir;
  std::string manifest;

  // Write manifest.
  manifest = R"({
      "name": "Test",
      "version": "1.0",
      "manifest_version": 3
    })";
  test_dir.WriteManifest(manifest);

  // Pack the v1 extension. This generates the CRX and a PEM (private key) file.
  base::FilePath v1_crx_path = PackExtension(test_dir.UnpackedPath());
  ASSERT_FALSE(v1_crx_path.empty());

  // Locate the generated pem file (temp.pem) in the same directory.
  base::FilePath pem_path = v1_crx_path.DirName().AppendASCII("temp.pem");
  {
    base::ScopedAllowBlockingForTesting allow_file_io_in_scope;
    ASSERT_TRUE(base::PathExists(pem_path));
  }

  // Read the actual RSA key content, ensuring a perfect match.
  const std::string extension_id = GetExtensionIdFromPrivateKeyFile(pem_path);

  // Instantiate the necessary providers for an external extension installation.
  ExternalProviderManager* external_provider_manager =
      ExternalProviderManager::Get(profile());
  TestExtensionRegistryObserver observer(extension_registry());
  auto provider = std::make_unique<MockExternalProvider>(
      external_provider_manager, mojom::ManifestLocation::kExternalPref);

  // Store the raw pointer before moving the unique_ptr, for later reuse.
  MockExternalProvider* provider_ptr = provider.get();

  // Install the external extension.
  provider->UpdateOrAddExtension(extension_id, "1.0", v1_crx_path);
  external_provider_manager->AddProviderForTesting(std::move(provider));
  external_provider_manager->CheckForExternalUpdates();

  // Verify that the extension is installed.
  auto extension = observer.WaitForExtensionInstalled();
  EXPECT_EQ(extension->id(), extension_id);

  // Verify that the extension is enabled.
  ASSERT_TRUE(extension_registrar()->IsExtensionEnabled(extension_id));

  // Update the extension by increasing the permission and bumping the version.
  manifest = R"({
      "name": "Test",
      "version": "2.0",
      "manifest_version": 3,
      "permissions": ["tabs"]
    })";
  test_dir.WriteManifest(manifest);

  // Define a new path for the V2 CRX to avoid overwriting V1 while it might be
  // in use.
  base::FilePath v2_crx_path = v1_crx_path.DirName().AppendASCII("v2.crx");

  // Keep the same extension id by sharing the pem between versions.
  PackExtensionWithOptions(test_dir.UnpackedPath(), v2_crx_path, pem_path,
                           /*pem_out_path=*/base::FilePath(),
                           extensions::ExtensionCreator::kOverwriteCRX);

  // Install the updated extension.
  provider_ptr->UpdateOrAddExtension(extension_id, "2.0", v2_crx_path);
  ExternalProviderManager::Get(profile())->CheckForExternalUpdates();

  // Verify the extension version bump.
  extension = observer.WaitForExtensionInstalled();
  EXPECT_EQ(extension->version().GetString(), "2.0");

  // Verify that the permission increase does not enable the updated extension.
  ASSERT_FALSE(extension_registrar()->IsExtensionEnabled(extension_id));
  EXPECT_THAT(
      ExtensionPrefs::Get(profile())->GetDisableReasons(extension_id),
      testing::ElementsAre(disable_reason::DISABLE_PERMISSIONS_INCREASE));
}

}  // namespace extensions
