// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/first_party_sets_component_installer.h"

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

using ::testing::_;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

std::string ReadToString(base::File file) {
  std::string contents;
  base::ScopedFILE scoped_file(base::FileToFILE(std::move(file), "r"));
  return base::ReadStreamToString(scoped_file.get(), &contents) ? contents : "";
}

}  // namespace

class FirstPartySetsComponentInstallerTest : public ::testing::Test {
 public:
  FirstPartySetsComponentInstallerTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeature(net::features::kFirstPartySets);
  }

  void TearDown() override {
    FirstPartySetsComponentInstallerPolicy::ResetForTesting();
  }

 protected:
  base::test::TaskEnvironment env_;

  base::ScopedTempDir component_install_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FirstPartySetsComponentInstallerTest, FeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(net::features::kFirstPartySets);
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  // We still install the component and subscribe to updates even when the
  // feature is disabled, so that if the feature eventually gets enabled, we
  // will already have the requisite data.
  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterFirstPartySetsComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerTest, NonexistentFile_OnComponentReady) {
  SEQUENCE_CHECKER(sequence_checker);

  ASSERT_TRUE(
      base::DeleteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath())));

  FirstPartySetsComponentInstallerPolicy(
      base::BindRepeating([](base::File) { CHECK(false); }))
      .ComponentReady(base::Version(), component_install_dir_.GetPath(),
                      base::Value(base::Value::Type::DICTIONARY));

  base::RunLoop().RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerTest,
       NonexistentFile_OnRegistrationComplete) {
  ASSERT_TRUE(
      base::DeleteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath())));

  base::RunLoop run_loop;
  FirstPartySetsComponentInstallerPolicy(
      base::BindLambdaForTesting([&](base::File file) {
        EXPECT_FALSE(file.IsValid());
        run_loop.Quit();
      }))
      .OnRegistrationComplete();

  run_loop.Run();
}

TEST_F(FirstPartySetsComponentInstallerTest, LoadsSets_OnComponentReady) {
  SEQUENCE_CHECKER(sequence_checker);
  const std::string expectation = "some first party sets";
  base::RunLoop run_loop;
  auto policy = std::make_unique<FirstPartySetsComponentInstallerPolicy>(
      base::BindLambdaForTesting([&](base::File file) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_TRUE(file.IsValid());
        EXPECT_EQ(ReadToString(std::move(file)), expectation);
        run_loop.Quit();
      }));

  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          component_install_dir_.GetPath()),
                      expectation));

  policy->ComponentReady(base::Version(), component_install_dir_.GetPath(),
                         base::Value(base::Value::Type::DICTIONARY));

  run_loop.Run();
}

// Test that when the first version of the component is installed,
// ComponentReady is a no-op, because OnRegistrationComplete already executed
// the OnceCallback.
TEST_F(FirstPartySetsComponentInstallerTest, IgnoreNewSets_NoInitialComponent) {
  SEQUENCE_CHECKER(sequence_checker);

  int callback_calls = 0;
  FirstPartySetsComponentInstallerPolicy policy(
      // This should run only once for the OnRegistrationComplete call.
      base::BindLambdaForTesting([&](base::File file) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_FALSE(file.IsValid());
        callback_calls++;
      }));

  policy.OnRegistrationComplete();
  env_.RunUntilIdle();
  EXPECT_EQ(callback_calls, 1);

  // Install the component, which should be ignored.
  base::ScopedTempDir install_dir;
  CHECK(install_dir.CreateUniqueTempDirUnderPath(
      component_install_dir_.GetPath()));
  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          install_dir.GetPath()),
                      "first party sets content"));
  policy.ComponentReady(base::Version(), install_dir.GetPath(),
                        base::Value(base::Value::Type::DICTIONARY));

  env_.RunUntilIdle();

  EXPECT_EQ(callback_calls, 1);
}

// Test if a component has been installed, ComponentReady will be no-op when
// newer versions are installed.
TEST_F(FirstPartySetsComponentInstallerTest, IgnoreNewSets_OnComponentReady) {
  SEQUENCE_CHECKER(sequence_checker);
  const std::string sets_v1 = "first party sets v1";
  const std::string sets_v2 = "first party sets v2";

  base::ScopedTempDir dir_v1;
  CHECK(dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  base::ScopedTempDir dir_v2;
  CHECK(dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  int callback_calls = 0;
  FirstPartySetsComponentInstallerPolicy policy(
      // It should run only once for the first ComponentReady call.
      base::BindLambdaForTesting([&](base::File file) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_TRUE(file.IsValid());
        EXPECT_EQ(ReadToString(std::move(file)), sets_v1);
        callback_calls++;
      }));

  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          dir_v1.GetPath()),
                      sets_v1));
  policy.ComponentReady(base::Version(), dir_v1.GetPath(),
                        base::Value(base::Value::Type::DICTIONARY));

  // Install newer version of the component, which should not be picked up when
  // calling ComponentReady again.
  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          dir_v2.GetPath()),
                      sets_v2));
  policy.ComponentReady(base::Version(), dir_v2.GetPath(),
                        base::Value(base::Value::Type::DICTIONARY));

  env_.RunUntilIdle();

  EXPECT_EQ(callback_calls, 1);
}

TEST_F(FirstPartySetsComponentInstallerTest, LoadsSets_OnNetworkRestart) {
  SEQUENCE_CHECKER(sequence_checker);
  const std::string expectation = "some first party sets";

  // We do this in order for the static to memoize the install path.
  {
    base::RunLoop run_loop;
    FirstPartySetsComponentInstallerPolicy policy(
        base::BindLambdaForTesting([&](base::File file) {
          DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
          EXPECT_TRUE(file.IsValid());
          EXPECT_EQ(ReadToString(std::move(file)), expectation);
          run_loop.Quit();
        }));

    ASSERT_TRUE(base::WriteFile(
        FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
            component_install_dir_.GetPath()),
        expectation));

    policy.ComponentReady(base::Version(), component_install_dir_.GetPath(),
                          base::Value(base::Value::Type::DICTIONARY));

    run_loop.Run();
  }

  {
    base::RunLoop run_loop;

    FirstPartySetsComponentInstallerPolicy::ReconfigureAfterNetworkRestart(
        base::BindLambdaForTesting([&](base::File file) {
          DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
          EXPECT_TRUE(file.IsValid());
          EXPECT_EQ(ReadToString(std::move(file)), expectation);
          run_loop.Quit();
        }));

    run_loop.Run();
  }
}

// Test ReconfigureAfterNetworkRestart calls the callback with the correct
// version, i.e. the first installed component, even if there are newer versions
// installed after browser startup.
TEST_F(FirstPartySetsComponentInstallerTest, IgnoreNewSets_OnNetworkRestart) {
  SEQUENCE_CHECKER(sequence_checker);
  const std::string sets_v1 = "first party sets v1";
  const std::string sets_v2 = "first party sets v2";

  base::ScopedTempDir dir_v1;
  CHECK(dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  base::ScopedTempDir dir_v2;
  CHECK(dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  FirstPartySetsComponentInstallerPolicy policy(
      base::BindLambdaForTesting([&](base::File file) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_TRUE(file.IsValid());
        EXPECT_EQ(ReadToString(std::move(file)), sets_v1);
      }));

  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          dir_v1.GetPath()),
                      sets_v1));
  policy.ComponentReady(base::Version(), dir_v1.GetPath(),
                        base::Value(base::Value::Type::DICTIONARY));

  // Install newer version of the component, which should not be picked up when
  // calling ComponentReady again.
  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          dir_v2.GetPath()),
                      sets_v2));
  policy.ComponentReady(base::Version(), dir_v2.GetPath(),
                        base::Value(base::Value::Type::DICTIONARY));

  env_.RunUntilIdle();

  // ReconfigureAfterNetworkRestart calls the callback with the correct version.
  int callback_calls = 0;
  FirstPartySetsComponentInstallerPolicy::ReconfigureAfterNetworkRestart(
      base::BindLambdaForTesting([&](base::File file) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker);
        EXPECT_TRUE(file.IsValid());
        EXPECT_EQ(ReadToString(std::move(file)), sets_v1);
        callback_calls++;
      }));

  env_.RunUntilIdle();
  EXPECT_EQ(callback_calls, 1);
}

TEST_F(FirstPartySetsComponentInstallerTest, GetInstallerAttributes_Disabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(net::features::kFirstPartySets);

  FirstPartySetsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_THAT(policy.GetInstallerAttributes(),
              UnorderedElementsAre(
                  Pair(FirstPartySetsComponentInstallerPolicy::
                           kDogfoodInstallerAttributeName,
                       "false"),
                  Pair(FirstPartySetsComponentInstallerPolicy::kV2FormatOptIn,
                       "true")));
}

TEST_F(FirstPartySetsComponentInstallerTest,
       GetInstallerAttributes_NonDogfooder) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      net::features::kFirstPartySets,
      {{net::features::kFirstPartySetsIsDogfooder.name, "false"}});

  FirstPartySetsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_THAT(policy.GetInstallerAttributes(),
              UnorderedElementsAre(
                  Pair(FirstPartySetsComponentInstallerPolicy::
                           kDogfoodInstallerAttributeName,
                       "false"),
                  Pair(FirstPartySetsComponentInstallerPolicy::kV2FormatOptIn,
                       "true")));
}

TEST_F(FirstPartySetsComponentInstallerTest, GetInstallerAttributes_Dogfooder) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      net::features::kFirstPartySets,
      {{net::features::kFirstPartySetsIsDogfooder.name, "true"}});

  FirstPartySetsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_THAT(policy.GetInstallerAttributes(),
              UnorderedElementsAre(
                  Pair(FirstPartySetsComponentInstallerPolicy::
                           kDogfoodInstallerAttributeName,
                       "true"),
                  Pair(FirstPartySetsComponentInstallerPolicy::kV2FormatOptIn,
                       "true")));
}

TEST_F(FirstPartySetsComponentInstallerTest, GetInstallerAttributes_V2OptOut) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {net::features::kFirstPartySets,
           net::features::kFirstPartySetsV2ComponentFormat});

  FirstPartySetsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_THAT(policy.GetInstallerAttributes(),
              UnorderedElementsAre(
                  Pair(FirstPartySetsComponentInstallerPolicy::
                           kDogfoodInstallerAttributeName,
                       "false"),
                  Pair(FirstPartySetsComponentInstallerPolicy::kV2FormatOptIn,
                       "false")));
}

}  // namespace component_updater
