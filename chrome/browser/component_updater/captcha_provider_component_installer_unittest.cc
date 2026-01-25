// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/captcha_provider_component_installer.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace component_updater {

class CaptchaProviderComponentInstallerPolicyTest : public ::testing::Test {
 public:
  CaptchaProviderComponentInstallerPolicyTest() {
    CHECK(component_install_dir_.CreateUniqueTempDir());
    CHECK(component_install_dir_.IsValid());
  }

 protected:
  base::ScopedTempDir component_install_dir_;

  base::test::TaskEnvironment env_;
};

TEST_F(CaptchaProviderComponentInstallerPolicyTest,
       VerifyInstallation_ValidDir) {
  CaptchaProviderComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::DictValue(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(
      base::WriteFile(CaptchaProviderComponentInstallerPolicy::GetInstalledPath(
                          component_install_dir_.GetPath()),
                      ""));
  EXPECT_TRUE(policy.VerifyInstallation(base::DictValue(),
                                        component_install_dir_.GetPath()));
}

TEST_F(CaptchaProviderComponentInstallerPolicyTest,
       VerifyInstallation_InvalidDir) {
  CaptchaProviderComponentInstallerPolicy policy;

  EXPECT_FALSE(policy.VerifyInstallation(base::DictValue(),
                                         component_install_dir_.GetPath()));
  ASSERT_TRUE(base::WriteFile(component_install_dir_.GetPath().Append(
                                  base::FilePath(FILE_PATH_LITERAL("invalid"))),
                              ""));
  EXPECT_FALSE(policy.VerifyInstallation(base::DictValue(),
                                         component_install_dir_.GetPath()));
}

TEST_F(CaptchaProviderComponentInstallerPolicyTest,
       ComponentReady_NonexistentFile) {
  base::test::TestFuture<const std::optional<std::string>> future;
  CaptchaProviderComponentInstallerPolicy policy(future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  base::FilePath(FILE_PATH_LITERAL("invalid")),
                                  base::DictValue());

  EXPECT_EQ(future.Take(), std::nullopt);
}

TEST_F(CaptchaProviderComponentInstallerPolicyTest, ComponentReady_ValidFile) {
  const std::string expectation = "json";
  ASSERT_TRUE(
      base::WriteFile(CaptchaProviderComponentInstallerPolicy::GetInstalledPath(
                          component_install_dir_.GetPath()),
                      expectation));

  base::test::TestFuture<const std::optional<std::string>> future;
  CaptchaProviderComponentInstallerPolicy policy(future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"),
                                  component_install_dir_.GetPath(),
                                  base::DictValue());

  EXPECT_EQ(future.Take(), expectation);
}

TEST_F(CaptchaProviderComponentInstallerPolicyTest,
       ComponentReady_ComponentUpdate) {
  base::ScopedTempDir dir_v1;
  ASSERT_TRUE(
      dir_v1.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  const std::string expectation_v1 = "json";
  ASSERT_TRUE(
      base::WriteFile(CaptchaProviderComponentInstallerPolicy::GetInstalledPath(
                          dir_v1.GetPath()),
                      expectation_v1));

  base::test::TestFuture<const std::optional<std::string>> future;
  CaptchaProviderComponentInstallerPolicy policy(future.GetRepeatingCallback());

  policy.ComponentReadyForTesting(base::Version("0.0.1"), dir_v1.GetPath(),
                                  base::DictValue());

  EXPECT_EQ(future.Take(), expectation_v1);

  // Install newer component, which should be read by the policy.
  base::ScopedTempDir dir_v2;
  ASSERT_TRUE(
      dir_v2.CreateUniqueTempDirUnderPath(component_install_dir_.GetPath()));

  const std::string expectation_v2 = "new_json";
  ASSERT_TRUE(
      base::WriteFile(CaptchaProviderComponentInstallerPolicy::GetInstalledPath(
                          dir_v2.GetPath()),
                      expectation_v2));

  policy.ComponentReadyForTesting(base::Version("0.0.2"), dir_v2.GetPath(),
                                  base::DictValue());

  EXPECT_EQ(future.Take(), expectation_v2);
}

TEST_F(CaptchaProviderComponentInstallerPolicyTest,
       OnCaptchaProviderComponentReady) {
  const std::string captcha_providers_json = "[\"test.com\",\"other.com\"]";
  OnCaptchaProviderComponentReady(captcha_providers_json);

  // Verify that the captcha urls are correctly set in the manager.
  EXPECT_TRUE(
      page_load_metrics::CaptchaProviderManager::GetInstance()->loaded());
  EXPECT_FALSE(
      page_load_metrics::CaptchaProviderManager::GetInstance()->empty());
  EXPECT_TRUE(
      page_load_metrics::CaptchaProviderManager::GetInstance()->IsCaptchaUrl(
          GURL("https://test.com/")));
  EXPECT_TRUE(
      page_load_metrics::CaptchaProviderManager::GetInstance()->IsCaptchaUrl(
          GURL("https://other.com/")));
  EXPECT_FALSE(
      page_load_metrics::CaptchaProviderManager::GetInstance()->IsCaptchaUrl(
          GURL("https://not-in-list.com/")));
}

TEST_F(CaptchaProviderComponentInstallerPolicyTest, ComponentRegistered) {
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, RegisterComponent(testing::_));
  RegisterCaptchaProviderComponent(service.get());
  env_.RunUntilIdle();
}

}  // namespace component_updater
