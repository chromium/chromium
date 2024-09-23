// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/install_limiter.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/mock_crx_installer.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/crx_file_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/verifier_formats.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::CrxInstaller;
using extensions::CrxInstallError;
using extensions::InstallLimiter;
using testing::_;
using testing::Field;
using testing::Invoke;
using testing::Mock;

namespace {

constexpr char kRandomExtensionId[] = "abacabadabacabaeabacabadabacabaf";

constexpr int kLargeExtensionSize = 2000000;
constexpr int kSmallExtensionSize = 200000;

constexpr char kLargeExtensionCrx[] = "large.crx";
constexpr char kSmallExtensionCrx[] = "small.crx";

constexpr base::TimeDelta kLessThanExpectedWaitTime = base::Seconds(4);
constexpr base::TimeDelta kTimeDeltaUntilExpectedWaitTime = base::Seconds(1);

}  // namespace

class InstallLimiterShouldDeferInstallTest
    : public testing::TestWithParam<ash::DemoSession::DemoModeConfig> {
 public:
  InstallLimiterShouldDeferInstallTest()
      : scoped_user_manager_(std::make_unique<ash::FakeChromeUserManager>()) {}

  InstallLimiterShouldDeferInstallTest(
      const InstallLimiterShouldDeferInstallTest&) = delete;
  InstallLimiterShouldDeferInstallTest& operator=(
      const InstallLimiterShouldDeferInstallTest&) = delete;

  ~InstallLimiterShouldDeferInstallTest() override = default;

 protected:
  ash::ScopedStubInstallAttributes test_install_attributes_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_P(InstallLimiterShouldDeferInstallTest, ShouldDeferInstall) {
  const std::vector<std::string> screensaver_ids = {
      extension_misc::kScreensaverAppId, extension_misc::kNewAttractLoopAppId};

  ash::DemoModeTestHelper demo_mode_test_helper;
  if (GetParam() != ash::DemoSession::DemoModeConfig::kNone) {
    test_install_attributes_.Get()->SetDemoMode();
    demo_mode_test_helper.InitializeSession(GetParam());
  }

  // In demo mode, all apps larger than 1MB except for the screensaver
  // should be deferred.
  for (const std::string& id : screensaver_ids) {
    bool expected_defer_install =
        GetParam() == ash::DemoSession::DemoModeConfig::kNone ||
        id != ash::DemoSession::GetScreensaverAppId();
    EXPECT_EQ(expected_defer_install,
              InstallLimiter::ShouldDeferInstall(kLargeExtensionSize, id));
  }
  EXPECT_TRUE(InstallLimiter::ShouldDeferInstall(kLargeExtensionSize,
                                                 kRandomExtensionId));
  EXPECT_FALSE(InstallLimiter::ShouldDeferInstall(kSmallExtensionSize,
                                                  kRandomExtensionId));
}

INSTANTIATE_TEST_SUITE_P(
    DemoModeConfig,
    InstallLimiterShouldDeferInstallTest,
    ::testing::Values(ash::DemoSession::DemoModeConfig::kNone,
                      ash::DemoSession::DemoModeConfig::kOnline));

class InstallLimiterTest : public extensions::ExtensionServiceTestBase {
 public:
  InstallLimiterTest()
      : extensions::ExtensionServiceTestBase(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::MainThreadType::IO,
                content::BrowserTaskEnvironment::TimeSource::MOCK_TIME)) {}

  InstallLimiterTest(const InstallLimiterTest&) = delete;
  InstallLimiterTest& operator=(const InstallLimiterTest&) = delete;

  ~InstallLimiterTest() override = default;

 protected:
  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();

    ExtensionServiceInitParams params;
    params.enable_install_limiter = true;
    InitializeExtensionService(std::move(params));

    install_limiter_ = InstallLimiter::Get(profile());

    mock_installer_ =
        base::MakeRefCounted<extensions::MockCrxInstaller>(service());
  }

  extensions::CRXFileInfo CreateTestExtensionCrx(const base::FilePath& path,
                                                 int extension_size) {
    const std::string data(extension_size, 0);
    EXPECT_TRUE(base::WriteFile(path, data));
    extensions::CRXFileInfo crx_info(path, extensions::GetTestVerifierFormat());
    crx_info.extension_id = kRandomExtensionId;
    return crx_info;
  }

  raw_ptr<InstallLimiter> install_limiter_;
  scoped_refptr<extensions::MockCrxInstaller> mock_installer_;
};

// Test that small extensions are installed immediately.
TEST_F(InstallLimiterTest, DontDeferSmallExtensionInstallation) {
  const base::FilePath path =
      extensions_install_dir().AppendASCII(kSmallExtensionCrx);
  extensions::CRXFileInfo crx_info_small =
      CreateTestExtensionCrx(path, kSmallExtensionSize);

  EXPECT_CALL(*mock_installer_,
              InstallCrxFile(Field(&extensions::CRXFileInfo::path, path)));
  install_limiter_->Add(mock_installer_, crx_info_small);
  task_environment()->RunUntilIdle();
  Mock::VerifyAndClearExpectations(mock_installer_.get());
}

// Test that large extension installations are deferred.
TEST_F(InstallLimiterTest, DeferLargeExtensionInstallation) {
  const base::FilePath path =
      extensions_install_dir().AppendASCII(kLargeExtensionCrx);
  extensions::CRXFileInfo crx_info_large =
      CreateTestExtensionCrx(path, kLargeExtensionSize);

  // Check that the large extension will not be installed immediately.
  EXPECT_CALL(*mock_installer_,
              InstallCrxFile(Field(&extensions::CRXFileInfo::path, path)))
      .Times(0);
  install_limiter_->Add(mock_installer_, crx_info_large);
  task_environment()->FastForwardBy(kLessThanExpectedWaitTime);
  task_environment()->RunUntilIdle();
  Mock::VerifyAndClearExpectations(mock_installer_.get());

  // The installation starts only after the wait time is elapsed.
  EXPECT_CALL(*mock_installer_,
              InstallCrxFile(Field(&extensions::CRXFileInfo::path, path)));
  task_environment()->FastForwardBy(kTimeDeltaUntilExpectedWaitTime);
  task_environment()->RunUntilIdle();
  Mock::VerifyAndClearExpectations(mock_installer_.get());
}

// Test that deferred installations are run before the wait time expires if the
// OnAllExternalProvidersReady() signal was called.
TEST_F(InstallLimiterTest, RunDeferredInstallsWhenAllExternalProvidersReady) {
  const base::FilePath path =
      extensions_install_dir().AppendASCII(kLargeExtensionCrx);
  extensions::CRXFileInfo crx_info_large =
      CreateTestExtensionCrx(path, kLargeExtensionSize);

  // Check that the large extension will not be installed immediately.
  EXPECT_CALL(*mock_installer_,
              InstallCrxFile(Field(&extensions::CRXFileInfo::path, path)))
      .Times(0);
  install_limiter_->Add(mock_installer_, crx_info_large);
  task_environment()->FastForwardBy(kLessThanExpectedWaitTime);
  task_environment()->RunUntilIdle();
  Mock::VerifyAndClearExpectations(mock_installer_.get());

  // The installation starts before the wait time is elapsed if
  // OnAllExternalProvidersReady() is called.
  EXPECT_CALL(*mock_installer_,
              InstallCrxFile(Field(&extensions::CRXFileInfo::path, path)));
  install_limiter_->OnAllExternalProvidersReady();
  task_environment()->RunUntilIdle();
  Mock::VerifyAndClearExpectations(mock_installer_.get());
}

// Test that small extensions are installed before large extensions.
TEST_F(InstallLimiterTest, InstallSmallBeforeLargeExtensions) {
  // Create a large test extension crx file.
  const base::FilePath crx_path_large =
      extensions_install_dir().AppendASCII(kLargeExtensionCrx);
  extensions::CRXFileInfo crx_info_large =
      CreateTestExtensionCrx(crx_path_large, kLargeExtensionSize);

  // Create a small test extension crx file.
  const base::FilePath crx_path_small =
      extensions_install_dir().AppendASCII(kSmallExtensionCrx);
  extensions::CRXFileInfo crx_info_small =
      CreateTestExtensionCrx(crx_path_small, kSmallExtensionSize);

  CrxInstaller::InstallerResultCallback installer_callback;

  base::RunLoop run_loop;

  // When adding a large extension and then a small extension, the small
  // extension will be installed first. The mock function call will trigger a
  // installer_callback which will notify the install limiter to
  // continue with any deferred installations. This will then start the
  // installation of the large extension.
  {
    testing::InSequence s;

    EXPECT_CALL(*mock_installer_, AddInstallerCallback(_))
        .WillOnce(Invoke([&](CrxInstaller::InstallerResultCallback callback) {
          installer_callback = std::move(callback);
        }));
    EXPECT_CALL(
        *mock_installer_,
        InstallCrxFile(Field(&extensions::CRXFileInfo::path, crx_path_small)))
        .WillOnce(Invoke([&] {
          std::optional<CrxInstallError> error;
          task_environment()->GetMainThreadTaskRunner()->PostTask(
              FROM_HERE, base::BindOnce(std::move(installer_callback), error));
        }));

    EXPECT_CALL(*mock_installer_, AddInstallerCallback(_));
    EXPECT_CALL(
        *mock_installer_,
        InstallCrxFile(Field(&extensions::CRXFileInfo::path, crx_path_large)))
        .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit));
  }

  install_limiter_->Add(mock_installer_, crx_info_large);
  // Ensure that AddWithSize() is called for the large extension before also
  // adding the small extension.
  task_environment()->RunUntilIdle();
  install_limiter_->Add(mock_installer_, crx_info_small);

  run_loop.Run();

  Mock::VerifyAndClearExpectations(mock_installer_.get());
}
