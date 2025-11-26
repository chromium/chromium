// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/afp_blocked_domain_list_component_remover.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

using ::testing::_;

}  // namespace

namespace component_updater {

class AntiFingerprintingBlockedDomainListComponentRemoverTest
    : public PlatformTest {
 public:
  AntiFingerprintingBlockedDomainListComponentRemoverTest() = default;

  AntiFingerprintingBlockedDomainListComponentRemoverTest(
      const AntiFingerprintingBlockedDomainListComponentRemoverTest&) = delete;
  AntiFingerprintingBlockedDomainListComponentRemoverTest& operator=(
      const AntiFingerprintingBlockedDomainListComponentRemoverTest&) = delete;

 protected:
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
  content::BrowserTaskEnvironment task_env_;
};

TEST_F(AntiFingerprintingBlockedDomainListComponentRemoverTest,
       ComponentUnregistration_Success) {
  base::HistogramTester histogram_tester;
  // Create a test file in the component directory.
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath base_install_dir =
      user_data_dir.Append(kComponentBaseInstallDir);
  ASSERT_TRUE(base::CreateDirectory(base_install_dir));
  ASSERT_TRUE(base::WriteFile(
      base_install_dir.Append(FILE_PATH_LITERAL("test.txt")), "test"));
  ASSERT_TRUE(base::PathExists(base_install_dir));

  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();

  EXPECT_CALL(*service, UnregisterComponent(_));
  base::RunLoop run_loop;
  UnregisterAntiFingerprintingBlockedDomainListComponent(
      service.get(), user_data_dir, run_loop.QuitClosure());
  run_loop.Run();

  // The component directory should be deleted, but the user data directory
  // should still exist.
  EXPECT_TRUE(base::PathExists(user_data_dir));
  EXPECT_FALSE(base::PathExists(base_install_dir));
  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.BlockedDomainListComponent.InstallationResult",
      InstallationResult::kDeletionSuccess, 1);
}

TEST_F(AntiFingerprintingBlockedDomainListComponentRemoverTest,
       ComponentUnregistration_DirDoesNotExist) {
  base::HistogramTester histogram_tester;
  auto service =
      std::make_unique<component_updater::MockComponentUpdateService>();
  // Make sure the component directory doesn't exist to simulate a client that
  // doesn't have it installed.
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath base_install_dir =
      user_data_dir.Append(kComponentBaseInstallDir);
  ASSERT_FALSE(base::PathExists(base_install_dir));

  EXPECT_CALL(*service, UnregisterComponent(_));
  base::RunLoop run_loop;
  UnregisterAntiFingerprintingBlockedDomainListComponent(
      service.get(), user_data_dir, run_loop.QuitClosure());
  run_loop.Run();

  // The user data directory should not be deleted.
  EXPECT_TRUE(base::PathExists(user_data_dir));

  histogram_tester.ExpectUniqueSample(
      "FingerprintingProtection.BlockedDomainListComponent.InstallationResult",
      InstallationResult::kDeletionDirDoesNotExist, 1);
}

}  // namespace component_updater
