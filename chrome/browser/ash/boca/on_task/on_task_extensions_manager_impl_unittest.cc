// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_extensions_manager_impl.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::ExtensionId;
using extensions::ExtensionRegistrar;
using extensions::ExtensionRegistry;
using extensions::ExtensionService;
using extensions::ManagementPolicy;
using extensions::TestExtensionEnvironment;
using extensions::TestManagementPolicyProvider;
using extensions::UnloadedExtensionReason;
using extensions::mojom::ManifestLocation;
using ::testing::IsNull;

namespace ash::boca {
namespace {

class OnTaskExtensionsManagerImplTest : public ::testing::Test {
 public:
  OnTaskExtensionsManagerImplTest() {
    // Allow unpacked extensions without developer mode for testing.
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionDisableUnsupportedDeveloper);
  }

 protected:
  const Extension* AddExtension(
      ManifestLocation location = ManifestLocation::kUnpacked) {
    scoped_refptr<const Extension> extension =
        extensions::ExtensionBuilder("Extension").SetLocation(location).Build();
    extension_environment_.GetExtensionRegistrar()->AddExtension(
        extension.get());
    return extension.get();
  }

  void UninstallExtension(const std::string& extension_id) {
    extension_environment_.GetExtensionRegistrar()->RemoveExtension(
        extension_id, UnloadedExtensionReason::UNINSTALL);
  }

  TestingProfile* profile() { return extension_environment_.profile(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestExtensionEnvironment extension_environment_;
};

TEST_F(OnTaskExtensionsManagerImplTest, ShouldNotDisableComponentExtension) {
  const Extension* const extension = AddExtension(ManifestLocation::kComponent);
  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  on_task_extensions_manager.DisableExtensions();
  EXPECT_TRUE(
      ExtensionRegistrar::Get(profile())->IsExtensionEnabled(extension->id()));
}

TEST_F(OnTaskExtensionsManagerImplTest,
       ShouldDisableExtensionIfAllowedByPolicy) {
  const Extension* const extension = AddExtension();

  // Allow all extension modifications by policy.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::ALLOW_ALL);
  ManagementPolicy* const policy =
      extension_environment_.GetExtensionSystem()->management_policy();
  policy->RegisterProvider(&provider);

  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  on_task_extensions_manager.DisableExtensions();
  EXPECT_FALSE(
      ExtensionRegistrar::Get(profile())->IsExtensionEnabled(extension->id()));
}

TEST_F(OnTaskExtensionsManagerImplTest,
       ShouldNotDisableExtensionIfForcedEnabledByPolicy) {
  const Extension* const extension = AddExtension();

  // Force enable extension by policy.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_ENABLED);
  ManagementPolicy* const policy =
      extension_environment_.GetExtensionSystem()->management_policy();
  policy->RegisterProvider(&provider);

  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  on_task_extensions_manager.DisableExtensions();
  EXPECT_TRUE(
      ExtensionRegistrar::Get(profile())->IsExtensionEnabled(extension->id()));
}

TEST_F(OnTaskExtensionsManagerImplTest,
       ShouldReEnableExtensionIfAllowedByPolicy) {
  const Extension* const extension = AddExtension();
  const ExtensionRegistrar* const extension_registrar =
      ExtensionRegistrar::Get(profile());

  // Allow all extension modifications by policy.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::ALLOW_ALL);
  ManagementPolicy* const policy =
      extension_environment_.GetExtensionSystem()->management_policy();
  policy->RegisterProvider(&provider);

  // Disable extensions.
  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  on_task_extensions_manager.DisableExtensions();
  ASSERT_FALSE(extension_registrar->IsExtensionEnabled(extension->id()));

  // Re-enable extensions and verify the extension is enabled.
  on_task_extensions_manager.ReEnableExtensions();
  EXPECT_TRUE(extension_registrar->IsExtensionEnabled(extension->id()));
}

TEST_F(OnTaskExtensionsManagerImplTest,
       ShouldNotReEnableExtensionIfForcedDisabledByPolicy) {
  const Extension* const extension = AddExtension();
  const ExtensionRegistrar* const extension_registrar =
      ExtensionRegistrar::Get(profile());

  // Disable extensions.
  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  on_task_extensions_manager.DisableExtensions();
  ASSERT_FALSE(extension_registrar->IsExtensionEnabled(extension->id()));

  // Force disable extensions by policy.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_DISABLED);
  ManagementPolicy* const policy =
      extension_environment_.GetExtensionSystem()->management_policy();
  policy->RegisterProvider(&provider);

  // Re-enable extensions and verify the extension remains disabled.
  on_task_extensions_manager.ReEnableExtensions();
  EXPECT_FALSE(extension_registrar->IsExtensionEnabled(extension->id()));
}

TEST_F(OnTaskExtensionsManagerImplTest, ShouldReEnableExtensionsOnInit) {
  const Extension* const extension = AddExtension();
  const ExtensionRegistrar* const extension_registrar =
      ExtensionRegistrar::Get(profile());

  // Allow all extension modifications by policy.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::ALLOW_ALL);
  ManagementPolicy* const policy =
      extension_environment_.GetExtensionSystem()->management_policy();
  policy->RegisterProvider(&provider);

  // Disable extensions.
  {
    OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
    on_task_extensions_manager.DisableExtensions();
    ASSERT_FALSE(extension_registrar->IsExtensionEnabled(extension->id()));
  }

  // Verify the extension is enabled after re-initialization.
  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return extension_registrar->IsExtensionEnabled(extension->id());
  }));
}

TEST_F(OnTaskExtensionsManagerImplTest,
       ShouldReEnableExtensionIfDisableMultipleTimes) {
  const Extension* const extension = AddExtension();
  const ExtensionRegistrar* const extension_registrar =
      ExtensionRegistrar::Get(profile());

  // Allow all extension modifications by policy.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::ALLOW_ALL);
  ManagementPolicy* const policy =
      extension_environment_.GetExtensionSystem()->management_policy();
  policy->RegisterProvider(&provider);

  // Disable extensions multiple times.
  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  on_task_extensions_manager.DisableExtensions();
  ASSERT_FALSE(extension_registrar->IsExtensionEnabled(extension->id()));
  on_task_extensions_manager.DisableExtensions();
  ASSERT_FALSE(extension_registrar->IsExtensionEnabled(extension->id()));

  // Re-enable extensions and verify the extension is enabled.
  on_task_extensions_manager.ReEnableExtensions();
  EXPECT_TRUE(extension_registrar->IsExtensionEnabled(extension->id()));
}

TEST_F(OnTaskExtensionsManagerImplTest,
       ShouldNotReEnableExtensionIfExtensionIsUninstalled) {
  const Extension* const extension = AddExtension();
  const ExtensionRegistrar* const extension_registrar =
      ExtensionRegistrar::Get(profile());
  const ExtensionRegistry* const extension_registry =
      ExtensionRegistry::Get(profile());

  // Allow all extension modifications by policy.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::ALLOW_ALL);
  ManagementPolicy* const policy =
      extension_environment_.GetExtensionSystem()->management_policy();
  policy->RegisterProvider(&provider);

  // Disable extensions.
  OnTaskExtensionsManagerImpl on_task_extensions_manager(profile());
  on_task_extensions_manager.DisableExtensions();
  ASSERT_FALSE(extension_registrar->IsExtensionEnabled(extension->id()));

  // Uninstall the extension.
  const ExtensionId extension_id = extension->id();
  UninstallExtension(extension_id);
  ASSERT_THAT(extension_registry->disabled_extensions().GetByID(extension_id),
              IsNull());

  // Re-enable extensions and verify the extension is not enabled.
  on_task_extensions_manager.ReEnableExtensions();
  EXPECT_FALSE(extension_registrar->IsExtensionEnabled(extension_id));
}

}  // namespace
}  // namespace ash::boca
