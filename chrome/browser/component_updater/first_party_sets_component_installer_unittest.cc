// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/first_party_sets_component_installer.h"

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

class FirstPartySetsComponentInstallerTest : public ::testing::Test {
 public:
  FirstPartySetsComponentInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
  }

 protected:
  base::test::TaskEnvironment env_;

  base::ScopedTempDir component_install_dir_;
};

TEST_F(FirstPartySetsComponentInstallerTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(network::features::kFirstPartySets);
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  EXPECT_CALL(*service, RegisterComponent(_)).Times(0);
  RegisterFirstPartySetsComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerTest, LoadsSets) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(network::features::kFirstPartySets);

  SEQUENCE_CHECKER(sequence_checker);
  const std::string expectation = "some first party sets";
  base::RunLoop run_loop;
  auto policy = std::make_unique<FirstPartySetsComponentInstallerPolicy>(
      base::BindLambdaForTesting([&](const std::string& got) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_EQ(got, expectation);
        run_loop.Quit();
      }));

  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          component_install_dir_.GetPath()),
                      expectation));

  policy->ComponentReady(base::Version(), component_install_dir_.GetPath(),
                         std::make_unique<base::DictionaryValue>());

  run_loop.Run();
}

}  // namespace component_updater
