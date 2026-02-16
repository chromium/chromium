// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/connection_factory_impl.h"

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/legion/private_ai_service.h"
#include "chrome/browser/legion/private_ai_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/legion/common/legion_logger.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/token_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace legion {

namespace {

class ConnectionFactoryImplBrowserTest : public InProcessBrowserTest {
 public:
  ConnectionFactoryImplBrowserTest() {
    feature_list_.InitAndEnableFeature(kLegion);
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

  LegionLogger* GetLogger() { return &logger_; }

 private:
  LegionLogger logger_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       ApiKeyConnectionFactoryCreate) {
  GURL url("wss://legion.googleapis.com?key=test_api_key");

  ApiKeyConnectionFactoryImpl factory(url, GetNetworkContext(), GetLogger());

  auto connection = factory.Create(base::DoNothing());
  EXPECT_TRUE(connection);
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       ApiKeyConnectionFactoryCtorFailsWithoutApiKey) {
  GURL url("wss://legion.googleapis.com");
  EXPECT_CHECK_DEATH(
      ApiKeyConnectionFactoryImpl(url, GetNetworkContext(), GetLogger()));
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       TokenConnectionFactoryCreate) {
  GURL url("wss://legion.googleapis.com");

  TokenConnectionFactoryImpl factory(url, GetNetworkContext(),
                                     GetTokenManager(), GetLogger());

  auto connection = factory.Create(base::DoNothing());
  EXPECT_TRUE(connection);
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       TokenConnectionFactoryCtorFailsWithApiKey) {
  GURL url("wss://legion.googleapis.com?key=test_api_key");
  EXPECT_CHECK_DEATH(TokenConnectionFactoryImpl(
      url, GetNetworkContext(), GetTokenManager(), GetLogger()));
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       ProxyWithTokenConnectionFactoryCreate) {
  GURL url("wss://legion.googleapis.com");

  ProxyWithTokenConnectionFactoryImpl factory(url, GURL("https://proxy.com"),
                                              content::GetNetworkService(),
                                              GetTokenManager(), GetLogger());

  auto connection = factory.Create(base::DoNothing());
  EXPECT_TRUE(connection);
}

IN_PROC_BROWSER_TEST_F(ConnectionFactoryImplBrowserTest,
                       ProxyWithTokenConnectionFactoryCtorFailsWithApiKey) {
  GURL url("wss://legion.googleapis.com?key=test_api_key");
  EXPECT_CHECK_DEATH(ProxyWithTokenConnectionFactoryImpl(
      url, GURL("https://proxy.com"), content::GetNetworkService(),
      GetTokenManager(), GetLogger()));
}

}  // namespace

}  // namespace legion
