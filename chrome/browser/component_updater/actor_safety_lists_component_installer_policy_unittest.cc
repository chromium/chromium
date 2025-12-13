// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/component_updater/actor_safety_lists_component_installer.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class ActorSafetyListsComponentInstallerPolicyTest : public ::testing::Test {
 public:
  ActorSafetyListsComponentInstallerPolicyTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
    CHECK(component_install_dir_.IsValid());
  }

 protected:
  base::ScopedTempDir component_install_dir_;

  base::test::TaskEnvironment env_;
};

TEST_F(ActorSafetyListsComponentInstallerPolicyTest,
       VerifyInstallation_ValidDir) {
  ActorSafetyListsComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(
      ActorSafetyListsComponentInstallerPolicy::GetInstalledPathForTesting(
          component_install_dir_.GetPath()),
      ""));
  EXPECT_TRUE(policy.VerifyInstallation(base::Value::Dict(),
                                        component_install_dir_.GetPath()));
}

TEST_F(ActorSafetyListsComponentInstallerPolicyTest,
       VerifyInstallation_InvalidDir) {
  ActorSafetyListsComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(component_install_dir_.GetPath().Append(
                                  base::FilePath(FILE_PATH_LITERAL("invalid"))),
                              ""));
  EXPECT_FALSE(policy.VerifyInstallation(base::Value::Dict(),
                                         component_install_dir_.GetPath()));
}

TEST_F(ActorSafetyListsComponentInstallerPolicyTest,
       ComponentReady_NonexistentFile) {
  base::test::TestFuture<const std::optional<std::string>&> future;
  ActorSafetyListsComponentInstallerPolicy policy(
      future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  base::FilePath(FILE_PATH_LITERAL("invalid")),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(ActorSafetyListsComponentInstallerPolicyTest, ComponentReady_ValidFile) {
  const std::string expectation = "json";
  ASSERT_TRUE(base::WriteFile(
      ActorSafetyListsComponentInstallerPolicy::GetInstalledPathForTesting(
          component_install_dir_.GetPath()),
      expectation));

  base::test::TestFuture<const std::optional<std::string>&> future;
  ActorSafetyListsComponentInstallerPolicy policy(
      future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  component_install_dir_.GetPath(),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take(), expectation);
}

TEST_F(ActorSafetyListsComponentInstallerPolicyTest,
       ComponentReady_ComponentUpdate) {
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  const std::string expectation_v1 = "json";
  ASSERT_TRUE(base::WriteFile(
      ActorSafetyListsComponentInstallerPolicy::GetInstalledPathForTesting(
          dir_v1.GetPath()),
      expectation_v1));

  base::test::TestFuture<const std::optional<std::string>&> future;
  ActorSafetyListsComponentInstallerPolicy policy(
      future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"), dir_v1.GetPath(),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take(), expectation_v1);

  // Install newer component, which should be read by the policy.
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  const std::string expectation_v2 = "new json";
  ASSERT_TRUE(base::WriteFile(
      ActorSafetyListsComponentInstallerPolicy::GetInstalledPathForTesting(
          dir_v2.GetPath()),
      expectation_v2));

  policy.ComponentReadyForTesting(base::Version("0.0.2"), dir_v2.GetPath(),
                                  base::Value::Dict());

  EXPECT_EQ(future.Take(), expectation_v2);
}

TEST_F(ActorSafetyListsComponentInstallerPolicyTest, ComponentRegistered) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(testing::_));
  base::RunLoop run_loop;
  RegisterActorSafetyListsComponent(service.get(), run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace component_updater
