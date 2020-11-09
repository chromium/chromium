// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/trust_token_key_commitments_component_installer.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
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
  TrustTokenKeyCommitmentsComponentInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

 protected:
  base::test::TaskEnvironment env_;

  base::ScopedTempDir component_install_dir_;
};

TEST_F(TrustTokenKeyCommitmentsComponentInstallerTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(network::features::kTrustTokens);
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(service.get());

  env_.RunUntilIdle();
}

TEST_F(TrustTokenKeyCommitmentsComponentInstallerTest, LoadsCommitments) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(network::features::kTrustTokens);

  base::SequenceCheckerImpl checker;

  std::string expectation = "some trust token keys";
  base::RunLoop run_loop;
  auto confirmation_callback = [&](const std::string& got) {
    EXPECT_TRUE(checker.CalledOnValidSequence());
    EXPECT_EQ(got, expectation);
    run_loop.Quit();
  };
  auto policy =
      std::make_unique<TrustTokenKeyCommitmentsComponentInstallerPolicy>(
          base::BindLambdaForTesting(confirmation_callback));

  ASSERT_TRUE(base::WriteFile(
      TrustTokenKeyCommitmentsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath()),
      expectation));

  policy->ComponentReady(base::Version(), component_install_dir_.GetPath(),
                         std::make_unique<base::DictionaryValue>());

  run_loop.Run();
}

}  // namespace component_updater
