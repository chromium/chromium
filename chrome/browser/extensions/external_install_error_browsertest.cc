// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/ui/global_error/global_error_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/mock_external_provider.h"
#include "extensions/browser/test_extension_registry_observer.h"

namespace extensions {

using ExternalInstallErrorTest = ExtensionBrowserTest;

// Test that global errors don't crash on shutdown. See crbug.com/720081.
IN_PROC_BROWSER_TEST_F(ExternalInstallErrorTest, TestShutdown) {
  // This relies on prompting for external extensions.
  FeatureSwitch::ScopedOverride feature_override(
      FeatureSwitch::prompt_for_external_extensions(), true);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  const char kId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
  {
    // Wait for an external extension to be installed and a global error about
    // it added.
    test::GlobalErrorWaiter waiter(profile());
    TestExtensionRegistryObserver observer(registry);

    auto provider = std::make_unique<MockExternalProvider>(
        extension_service(), mojom::ManifestLocation::kExternalPref);
    provider->UpdateOrAddExtension(kId, "1.0.0.0",
                                   test_data_dir_.AppendASCII("good.crx"));
    extension_service()->AddProviderForTesting(std::move(provider));
    extension_service()->CheckForExternalUpdates();

    auto extension = observer.WaitForExtensionInstalled();
    EXPECT_EQ(extension->id(), kId);

    waiter.Wait();
  }

  // Verify the extension is in the expected state (disabled for being
  // unacknowledged).
  EXPECT_FALSE(registry->enabled_extensions().Contains(kId));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_FALSE(prefs->IsExternalExtensionAcknowledged(kId));
  EXPECT_EQ(disable_reason::DISABLE_EXTERNAL_EXTENSION,
            prefs->GetDisableReasons(kId));

  // Verify the external error.
  ExternalInstallManager* manager =
      extension_service()->external_install_manager();
  std::vector<ExternalInstallError*> errors = manager->GetErrorsForTesting();
  ASSERT_EQ(1u, errors.size());
  EXPECT_EQ(kId, errors[0]->extension_id());

  // End the test and shutdown without removing the global error. This should
  // not crash.
}

}  // namespace extensions
