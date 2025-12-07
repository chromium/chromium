// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/protocol_handlers/protocol_handlers_manager.h"

#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"

namespace {

static constexpr const char kId[] = "oddldldiddnfpoecindoignlngbecglb";

}  // namespace

namespace extensions {

class ProtocolHandlersManagerServiceTest : public ExtensionServiceTestBase {
 public:
  ProtocolHandlersManagerServiceTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionProtocolHandlers);
  }

 protected:
  TestingProfile::TestingFactories GetTestingFactories() const {
    // Use SimpleProtocolHandlerRegistryFactory to prevent OS integration during
    // the protocol registration process.
    return TestingProfile::TestingFactories{TestingProfile::TestingFactory{
        ProtocolHandlerRegistryFactory::GetInstance(),
        custom_handlers::SimpleProtocolHandlerRegistryFactory::
            GetDefaultFactory()}};
  }

  // Initialize an ExtensionService with a few already-installed extensions
  // registering protocol handlers..
  void CreateExtensionService(bool extensions_enabled) {
    ExtensionServiceInitParams params;
    params.testing_factories = GetTestingFactories();
    params.extensions_enabled = extensions_enabled;
    ASSERT_TRUE(params.ConfigureByTestDataDirectory(
        data_dir().AppendASCII("protocol_handlers_api")));
    InitializeExtensionService(std::move(params));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// These tests are not useful for desktop-android builds unless the
// ProtocolHandlerSanityCheck method  of the ExtensionBrowserClient interface is
// properly implemented for Android.
TEST_F(ProtocolHandlersManagerServiceTest, PRE_ProtocolHandlerSanityCheck) {
  CreateExtensionService(false);
  service()->Init();
  EXPECT_FALSE(registry()->enabled_extensions().GetByID(kId));

  const auto* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile());
  DCHECK(registry);
  ASSERT_FALSE(registry->IsHandledProtocol("ipfs"));
}

TEST_F(ProtocolHandlersManagerServiceTest, ProtocolHandlerSanityCheck) {
  CreateExtensionService(true);
  service()->Init();
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(kId));

  const auto* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile());
  DCHECK(registry);
  ASSERT_TRUE(registry->IsHandledProtocol("ipfs"));
}

}  // namespace extensions
