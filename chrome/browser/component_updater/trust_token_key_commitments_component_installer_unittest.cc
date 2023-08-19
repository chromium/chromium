// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/trust_token_key_commitments_component_installer.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {
using ::testing::_;
}  // namespace

class TrustTokenKeyCommitmentsComponentInstallerTest : public ::testing::Test {
 public:
  TrustTokenKeyCommitmentsComponentInstallerTest() = default;

 protected:
  base::test::TaskEnvironment env_;
};

TEST_F(TrustTokenKeyCommitmentsComponentInstallerTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitWithFeatures({}, {network::features::kPrivateStateTokens,
                                    network::features::kFledgePst});
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(service.get());

  env_.RunUntilIdle();
}

}  // namespace component_updater
