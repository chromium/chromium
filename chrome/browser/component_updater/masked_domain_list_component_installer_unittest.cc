// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/masked_domain_list_component_installer.h"

#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
using ::testing::_;

}  // namespace

class MaskedDomainListComponentInstallerPolicyTest : public ::testing::Test {
 public:
  MaskedDomainListComponentInstallerPolicyTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
    content::GetNetworkService();  // Initializes Network Service.
  }

 protected:
  content::BrowserTaskEnvironment env_;
  base::test::ScopedFeatureList scoped_feature_list_;

  base::ScopedTempDir component_install_dir_;
};

TEST_F(MaskedDomainListComponentInstallerPolicyTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      network::features::kMaskedDomainList);
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  RegisterMaskedDomainListComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(MaskedDomainListComponentInstallerPolicyTest,
       FeatureEnabled_NoFileExists) {
  scoped_feature_list_.InitWithFeatures(
      {network::features::kMaskedDomainList,
       net::features::kEnableIpProtectionProxy},
      {});
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterMaskedDomainListComponent(service.get());
  env_.RunUntilIdle();

  // If no file has been passed, the allow list is not populated.
  EXPECT_FALSE(network::NetworkService::GetNetworkServiceForTesting()
                   ->masked_domain_list_manager()
                   ->IsPopulated());
}

}  // namespace component_updater
