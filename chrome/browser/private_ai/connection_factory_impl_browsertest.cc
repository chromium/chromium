// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/connection_factory_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/features.h"
#include "components/private_ai/phosphor/token_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace private_ai {

namespace {

class ConnectionFactoryImplBrowserTest : public InProcessBrowserTest {
 public:
  ConnectionFactoryImplBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        kPrivateAi, {{kPrivateAiApiKey.name, "test-api-key"}});
  }
  ~ConnectionFactoryImplBrowserTest() override = default;

 protected:
  network::mojom::NetworkContext* GetNetworkContext() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

  phosphor::TokenManager* GetTokenManager() {
    auto* service =
        PrivateAiServiceFactory::GetForProfile(browser()->profile());
    CHECK(service);
    auto* token_manager = service->GetTokenManager();
    CHECK(token_manager);
    return token_manager;
  }

  PrivateAiLogger* GetLogger() { return &logger_; }

 private:
  PrivateAiLogger logger_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       CreateConnectionWithoutToken) {
  GURL url("wss://private-ai.googleapis.com?key=test_api_key");

  ConnectionFactoryImpl factory(url, GetNetworkContext(), GetLogger());

  auto connection = factory.Create(base::DoNothing());
  EXPECT_TRUE(connection);
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       FactoryCtorFailsWithoutApiKey) {
  GURL url("wss://private-ai.googleapis.com");
  EXPECT_CHECK_DEATH(
      ConnectionFactoryImpl(url, GetNetworkContext(), GetLogger()));
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       CreateConnectionWithToken) {
  GURL url("wss://private-ai.googleapis.com?key=test_api_key");

  ConnectionFactoryImpl factory(url, GetNetworkContext(), GetLogger());
  factory.EnableTokenAttestation(GetTokenManager());

  auto connection = factory.Create(base::DoNothing());
  EXPECT_TRUE(connection);
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       CreateConnectionWithProxyAndToken) {
  GURL url("wss://private-ai.googleapis.com?key=test_api_key");

  ConnectionFactoryImpl factory(url, GetNetworkContext(), GetLogger());
  factory.EnableTokenAttestation(GetTokenManager());
  factory.EnableProxy(GURL("https://proxy.com"), content::GetNetworkService());

  auto connection = factory.Create(base::DoNothing());
  EXPECT_TRUE(connection);
}

}  // namespace

}  // namespace private_ai
