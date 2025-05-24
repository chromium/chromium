// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/probabilistic_reveal_token_component_installer.h"

#include <optional>

#include "base/test/run_until.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/ip_protection/common/probabilistic_reveal_token_registry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/network_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace component_updater {

class ProbabilisticRevealTokenComponentInstallerTest : public ::testing::Test {
 public:
  ProbabilisticRevealTokenComponentInstallerTest() {
    content::GetNetworkService();  // Initializes Network Service.
  }

  void RunUntilIdle() { env_.RunUntilIdle(); }

 protected:
  content::BrowserTaskEnvironment env_;
  MockComponentUpdateService cus_;
};

TEST_F(ProbabilisticRevealTokenComponentInstallerTest, ComponentRegistration) {
  EXPECT_CALL(cus_, RegisterComponent(testing::_)).Times(1);
  RegisterProbabilisticRevealTokenComponent(&cus_);
  RunUntilIdle();
}

TEST_F(ProbabilisticRevealTokenComponentInstallerTest,
       OnProbabilisticRevealTokenRegistryReadySuccess) {
  OnProbabilisticRevealTokenRegistryReady(R"json({
    "domains": [
      "example.com"
    ]
  })json");

  EXPECT_TRUE(base::test::RunUntil([&] {
    return network::NetworkService::GetNetworkServiceForTesting()
        ->probabilistic_reveal_token_registry()
        ->IsRegistered(GURL("https://example.com"));
  }));
}

TEST_F(ProbabilisticRevealTokenComponentInstallerTest,
       OnProbabilisticRevealTokenRegistryReadyFailure) {
  // No JSON content.
  OnProbabilisticRevealTokenRegistryReady(std::nullopt);
  RunUntilIdle();
  EXPECT_FALSE(network::NetworkService::GetNetworkServiceForTesting()
                   ->probabilistic_reveal_token_registry()
                   ->IsRegistered(GURL("https://example.com")));

  // Invalid JSON.
  OnProbabilisticRevealTokenRegistryReady("example.com");
  RunUntilIdle();
  EXPECT_FALSE(network::NetworkService::GetNetworkServiceForTesting()
                   ->probabilistic_reveal_token_registry()
                   ->IsRegistered(GURL("https://example.com")));

  // Not a top level dictionary.
  OnProbabilisticRevealTokenRegistryReady(R"json(["example.com"])json");
  RunUntilIdle();
  EXPECT_FALSE(network::NetworkService::GetNetworkServiceForTesting()
                   ->probabilistic_reveal_token_registry()
                   ->IsRegistered(GURL("https://example.com")));
}

}  // namespace component_updater
