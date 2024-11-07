// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_verifier.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::mojom::ManifestLocation;

namespace extensions {

class InstallVerifierTest : public ExtensionServiceTestBase {
 public:
  InstallVerifierTest() = default;

  InstallVerifierTest(const InstallVerifierTest&) = delete;
  InstallVerifierTest& operator=(const InstallVerifierTest&) = delete;

  ~InstallVerifierTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeExtensionService(ExtensionServiceInitParams());
  }

  // Adds an extension as being allowed by policy.
  void AddExtensionAsPolicyInstalled(const ExtensionId& id) {
    base::Value::Dict extension_entry =
        base::Value::Dict().Set("installation_mode", "allowed");
    testing_pref_service()->SetManagedPref(
        pref_names::kExtensionManagement,
        base::Value::Dict().Set(id, std::move(extension_entry)));
    EXPECT_TRUE(ExtensionManagementFactory::GetForBrowserContext(profile())
                    ->IsInstallationExplicitlyAllowed(id));
  }

 private:
  ScopedInstallVerifierBypassForTest force_install_verification{
      ScopedInstallVerifierBypassForTest::kForceOn};
  std::unique_ptr<ExtensionManagement> extension_management_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

// Test the behavior of the InstallVerifier for various extensions.
TEST_F(InstallVerifierTest, TestIsFromStoreAndMustRemainDisabled) {
  enum FromStoreStatus {
    FROM_STORE,
    NOT_FROM_STORE,
  };

  enum MustRemainDisabledStatus {
    MUST_REMAIN_DISABLED,
    CAN_BE_ENABLED,
  };

  GURL store_update_url = extension_urls::GetWebstoreUpdateUrl();
  GURL non_store_update_url("https://example.com");
  struct {
    const char* test_name;
    ManifestLocation location;
    std::optional<GURL> update_url;
    FromStoreStatus expected_from_store_status;
    MustRemainDisabledStatus expected_must_remain_disabled_status;
  } test_cases[] = {
      {"internal from store", ManifestLocation::kInternal, store_update_url,
       FROM_STORE, CAN_BE_ENABLED},
      {"internal non-store update url", ManifestLocation::kInternal,
       non_store_update_url, NOT_FROM_STORE, MUST_REMAIN_DISABLED},
      {"internal no update url", ManifestLocation::kInternal, std::nullopt,
       NOT_FROM_STORE, MUST_REMAIN_DISABLED},
      {"unpacked from store", ManifestLocation::kUnpacked, store_update_url,
       FROM_STORE, CAN_BE_ENABLED},
      {"unpacked non-store update url", ManifestLocation::kUnpacked,
       non_store_update_url, NOT_FROM_STORE, CAN_BE_ENABLED},
      {"unpacked no update url", ManifestLocation::kUnpacked, std::nullopt,
       NOT_FROM_STORE, CAN_BE_ENABLED},
      {"external from store", ManifestLocation::kExternalPolicyDownload,
       store_update_url, FROM_STORE, CAN_BE_ENABLED},
      {"external non-store update url",
       ManifestLocation::kExternalPolicyDownload, non_store_update_url,
       NOT_FROM_STORE, CAN_BE_ENABLED},
  };

  InstallVerifier* install_verifier = InstallVerifier::Get(profile());
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.test_name);
    ExtensionBuilder extension_builder(test_case.test_name);
    extension_builder.SetLocation(test_case.location);
    if (test_case.update_url) {
      extension_builder.SetManifestKey("update_url",
                                       test_case.update_url->spec());
    }
    scoped_refptr<const Extension> extension = extension_builder.Build();

    if (Manifest::IsPolicyLocation(test_case.location))
      AddExtensionAsPolicyInstalled(extension->id());

    EXPECT_EQ(test_case.expected_from_store_status == FROM_STORE,
              InstallVerifier::IsFromStore(*extension, profile()));
    disable_reason::DisableReason disable_reason;
    EXPECT_EQ(
        test_case.expected_must_remain_disabled_status == MUST_REMAIN_DISABLED,
        install_verifier->MustRemainDisabled(extension.get(), &disable_reason));
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
// Test the behavior of the InstallVerifier when an extension is
// force-installed in different trust environments.
TEST_F(InstallVerifierTest, ForceInstalledExtensionBehaviorWithTrustLevels) {
  InstallVerifier* install_verifier = InstallVerifier::Get(profile());
  scoped_refptr<const Extension> forced_extension =
      ExtensionBuilder("Force Installed Extension")
          .SetLocation(ManifestLocation::kExternalPolicyDownload)
          .Build();
  base::Value::Dict forced_list_pref;
  ExternalPolicyLoader::AddExtension(forced_list_pref, forced_extension->id(),
                                     "http://example.com/update_url");
  testing_pref_service()->SetManagedPref(pref_names::kInstallForceList,
                                         forced_list_pref.Clone());

  {
    // Set up a low-trust environment.
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);

    EXPECT_TRUE(ExtensionManagementFactory::GetForBrowserContext(profile())
                    ->IsForceInstalledInLowTrustEnvironment(*forced_extension));

    // In a low-trust environment, the extension should remain disabled.
    disable_reason::DisableReason disable_reason = disable_reason::DISABLE_NONE;
    EXPECT_TRUE(install_verifier->MustRemainDisabled(forced_extension.get(),
                                                     &disable_reason));
    EXPECT_EQ(disable_reason::DISABLE_NOT_VERIFIED, disable_reason);
  }

  {
    // Set up a high-trust environment.
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

    EXPECT_FALSE(
        ExtensionManagementFactory::GetForBrowserContext(profile())
            ->IsForceInstalledInLowTrustEnvironment(*forced_extension));

    // In a high-trust environment, the extension should not remain disabled.
    disable_reason::DisableReason disable_reason = disable_reason::DISABLE_NONE;
    EXPECT_FALSE(install_verifier->MustRemainDisabled(forced_extension.get(),
                                                      &disable_reason));
    // Verify that disable_reason is still DISABLE_NONE.
    EXPECT_EQ(disable_reason::DISABLE_NONE, disable_reason);
  }
}
#endif

}  // namespace extensions
