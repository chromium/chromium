// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/first_party_sets_component_installer.h"

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

using ::testing::_;
using ::testing::IsEmpty;

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
  }

  void SetUp() override {
    FirstPartySetsComponentInstallerPolicy::ResetForTesting();
  }

  // Subclasses are expected to call this in their constructors.
  virtual void InitializeFeatureList() = 0;

 protected:
  base::test::TaskEnvironment env_;

  base::ScopedTempDir component_install_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  first_party_sets::ScopedMockFirstPartySetsHandler
      mock_first_party_sets_handler_;
};

class FirstPartySetsComponentInstallerFeatureEnabledTest
    : public FirstPartySetsComponentInstallerTest {
 public:
  FirstPartySetsComponentInstallerFeatureEnabledTest() {
    InitializeFeatureList();
  }

  void InitializeFeatureList() override {
    scoped_feature_list_.InitAndEnableFeature(features::kFirstPartySets);
  }
};

class FirstPartySetsComponentInstallerFeatureDisabledTest
    : public FirstPartySetsComponentInstallerTest {
 public:
  FirstPartySetsComponentInstallerFeatureDisabledTest() {
    InitializeFeatureList();
  }

  void InitializeFeatureList() override {
    scoped_feature_list_.InitAndDisableFeature(features::kFirstPartySets);
  }
};

TEST_F(FirstPartySetsComponentInstallerFeatureDisabledTest, FeatureDisabled) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  // We still install the component and subscribe to updates even when the
  // feature is disabled, so that if the feature eventually gets enabled, we
  // will already have the requisite data.
  EXPECT_CALL(*service, RegisterComponent(_)).Times(1);
  RegisterFirstPartySetsComponent(service.get());

  env_.RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerFeatureEnabledTest,
       NonexistentFile_OnComponentReady) {
  ASSERT_TRUE(
      base::DeleteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath())));

  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy(future.GetCallback())
      .ComponentReady(base::Version(), component_install_dir_.GetPath(),
                      base::Value::Dict());

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_FALSE(std::get<0>(got).IsValid());
  EXPECT_FALSE(std::get<1>(got).IsValid());
}

TEST_F(FirstPartySetsComponentInstallerFeatureEnabledTest,
       NonexistentFile_OnRegistrationComplete) {
  ASSERT_TRUE(
      base::DeleteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
          component_install_dir_.GetPath())));

  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy policy(future.GetCallback());
  policy.OnRegistrationComplete();

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_FALSE(std::get<0>(got).IsValid());
  EXPECT_FALSE(std::get<1>(got).IsValid());

  // Only one call has any effect.
  policy.OnRegistrationComplete();
  env_.RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerFeatureEnabledTest,
       LoadsSets_OnComponentReady) {
  const base::Version version = base::Version("0.0.1");
  const std::string expectation = "some first party sets";
  base::test::TestFuture<base::Version, base::File> future;
  auto policy = std::make_unique<FirstPartySetsComponentInstallerPolicy>(
      future.GetCallback());

  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          component_install_dir_.GetPath()),
                      expectation));

  policy->ComponentReady(version, component_install_dir_.GetPath(),
                         base::Value::Dict());

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version);
  EXPECT_TRUE(std::get<1>(got).IsValid());
  EXPECT_EQ(ReadToString(std::move(std::get<1>(got))), expectation);
}

// Test that when the first version of the component is installed,
// ComponentReady is a no-op, because OnRegistrationComplete already executed
// the OnceCallback.
TEST_F(FirstPartySetsComponentInstallerFeatureEnabledTest,
       IgnoreNewSets_NoInitialComponent) {
  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy policy(future.GetCallback());

  policy.OnRegistrationComplete();
  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_FALSE(std::get<0>(got).IsValid());
  EXPECT_FALSE(std::get<1>(got).IsValid());

  // Install the component, which should be ignored.
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDirUnderPath(
      component_install_dir_.GetPath()));
  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          install_dir.GetPath()),
                      "first party sets content"));
  policy.ComponentReady(base::Version("0.0.1"), install_dir.GetPath(),
                        base::Value::Dict());

  env_.RunUntilIdle();
}

// Test if a component has been installed, ComponentReady will be no-op when
// newer versions are installed.
TEST_F(FirstPartySetsComponentInstallerFeatureEnabledTest,
       IgnoreNewSets_OnComponentReady) {
  base::test::TestFuture<base::Version, base::File> future;
  FirstPartySetsComponentInstallerPolicy policy(future.GetCallback());

  const base::Version version = base::Version("0.0.1");
  const std::string sets_v1 = "first party sets v1";
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          dir_v1.GetPath()),
                      sets_v1));
  policy.ComponentReady(version, dir_v1.GetPath(), base::Value::Dict());

  std::tuple<base::Version, base::File> got = future.Take();
  EXPECT_TRUE(std::get<0>(got).IsValid());
  EXPECT_EQ(std::get<0>(got), version);
  EXPECT_TRUE(std::get<1>(got).IsValid());
  EXPECT_EQ(ReadToString(std::move(std::get<1>(got))), sets_v1);

  // Install newer version of the component, which should not be picked up
  // when calling ComponentReady again.
  const std::string sets_v2 = "first party sets v2";
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));
  ASSERT_TRUE(
      base::WriteFile(FirstPartySetsComponentInstallerPolicy::GetInstalledPath(
                          dir_v2.GetPath()),
                      sets_v2));
  policy.ComponentReady(base::Version("0.0.1"), dir_v2.GetPath(),
                        base::Value::Dict());

  env_.RunUntilIdle();
}

TEST_F(FirstPartySetsComponentInstallerFeatureDisabledTest,
       GetInstallerAttributes) {
  FirstPartySetsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_THAT(policy.GetInstallerAttributes(), IsEmpty());
}

TEST_F(FirstPartySetsComponentInstallerFeatureEnabledTest,
       GetInstallerAttributes) {
  FirstPartySetsComponentInstallerPolicy policy(base::DoNothing());

  EXPECT_THAT(policy.GetInstallerAttributes(), IsEmpty());
}

}  // namespace component_updater
