// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/installer/installer_win.h"

#include <shobjidl.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_variant.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_experience {

namespace {

// Mock implementation of IAppCommandWeb for testing system-level installs.
class MockAppCommand
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IAppCommandWeb> {
 public:
  // IDispatch methods.
  MOCK_METHOD(HRESULT,
              GetTypeInfoCount,
              (UINT * pctinfo),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetTypeInfo,
              (UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              GetIDsOfNames,
              (REFIID riid,
               LPOLESTR* rgszNames,
               UINT cNames,
               LCID lcid,
               DISPID* rgDispId),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              Invoke,
              (DISPID dispIdMember,
               REFIID riid,
               LCID lcid,
               WORD wFlags,
               DISPPARAMS* pDispParams,
               VARIANT* pVarResult,
               EXCEPINFO* pExcepInfo,
               UINT* puArgErr),
              (override, Calltype(STDMETHODCALLTYPE)));

  // IAppCommandWeb methods.
  MOCK_METHOD(HRESULT,
              get_status,
              (UINT*),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              get_exitCode,
              (DWORD*),
              (override, Calltype(STDMETHODCALLTYPE)));
  MOCK_METHOD(HRESULT,
              get_output,
              (BSTR*),
              (override, Calltype(STDMETHODCALLTYPE)));

  MOCK_METHOD(HRESULT,
              execute,
              (VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT,
               VARIANT),
              (override, Calltype(STDMETHODCALLTYPE)));
};

// Mock implementation of the InstallerLauncherDelegate to intercept external
// calls.
class MockInstallerLauncherDelegate : public InstallerLauncherDelegate {
 public:
  MOCK_METHOD(Microsoft::WRL::ComPtr<IAppCommandWeb>,
              GetUpdaterAppCommand,
              (const std::wstring& command_name),
              (override));
  MOCK_METHOD(base::Process,
              LaunchProcess,
              (const base::CommandLine& cmd_line,
               const base::LaunchOptions& options),
              (override));
};

}  // namespace

class PlatformExperienceInstallerWinTest : public testing::Test {
 protected:
  // Simulates that the Platform Experience Helper is already installed by
  // creating its executable in the expected location.
  void CreateFakeHelperExecutable(bool is_system_install) {
    const int path_key = is_system_install
                             ? static_cast<int>(base::DIR_EXE)
                             : static_cast<int>(chrome::DIR_USER_DATA);
    base::FilePath peh_dir = base::PathService::CheckedGet(path_key).Append(
        L"PlatformExperienceHelper");
    ASSERT_TRUE(base::CreateDirectory(peh_dir));
    base::FilePath peh_exe_path =
        peh_dir.Append(L"platform_experience_helper.exe");
    ASSERT_TRUE(base::WriteFile(peh_exe_path, ""));
  }

  void SetUp() override {
    platform_experience::SetInstallerLauncherDelegateForTesting(
        &mock_delegate_);
  }

  void TearDown() override {
    platform_experience::SetInstallerLauncherDelegateForTesting(nullptr);
  }

  // A helper to run the main function under test with mocks and system-level
  // install settings.
  void RunMaybeInstallForSystem(bool is_system_install) {
    install_static::ScopedInstallDetails scoped_install_details(
        is_system_install);
    MaybeInstallPlatformExperienceHelper();
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  testing::StrictMock<MockInstallerLauncherDelegate> mock_delegate_;

 private:
  // These members override the respective paths with temporary directories
  // for the duration of the test.
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
  base::ScopedPathOverride exe_dir_override_{base::DIR_EXE};
  base::ScopedPathOverride module_dir_override_{base::DIR_MODULE};
};

// Test that no installation is attempted if the helper already exists.
TEST_F(PlatformExperienceInstallerWinTest,
       HelperAlreadyInstalled_NoActionTaken) {
  CreateFakeHelperExecutable(/*is_system_install=*/false);
  // No expectations on mock_delegate_ are set. If any of its methods are
  // called, the StrictMock will fail the test.
  RunMaybeInstallForSystem(/*is_system_install=*/false);

  // No histograms should be recorded.
  histogram_tester_.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User", 0);
  histogram_tester_.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.System", 0);
}

// Test system install case where the AppCommand cannot be found.
TEST_F(PlatformExperienceInstallerWinTest, SystemInstall_AppCommandNotFound) {
  EXPECT_CALL(mock_delegate_,
              GetUpdaterAppCommand(std::wstring(installer::kCmdInstallPEH)))
      .WillOnce(testing::Return(nullptr));

  RunMaybeInstallForSystem(/*is_system_install=*/true);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.System",
      /*SystemInstallerLaunchStatus::kAppCommandNotFound*/ 1, 1);
}

// Test system install case where executing the AppCommand fails.
TEST_F(PlatformExperienceInstallerWinTest, SystemInstall_AppCommandExecFails) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(
      *mock_app_command.Get(),
      execute(testing::_, testing::_, testing::_, testing::_, testing::_,
              testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(E_FAIL));

  EXPECT_CALL(mock_delegate_,
              GetUpdaterAppCommand(std::wstring(installer::kCmdInstallPEH)))
      .WillOnce(testing::Return(mock_app_command));

  RunMaybeInstallForSystem(/*is_system_install=*/true);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.System",
      /*SystemInstallerLaunchStatus::kAppCommandExecutionFailed*/ 2, 1);
}

// Test the successful system install case.
TEST_F(PlatformExperienceInstallerWinTest, SystemInstall_Success) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(
      *mock_app_command.Get(),
      execute(testing::_, testing::_, testing::_, testing::_, testing::_,
              testing::_, testing::_, testing::_, testing::_))
      .WillOnce(testing::Return(S_OK));

  EXPECT_CALL(mock_delegate_,
              GetUpdaterAppCommand(std::wstring(installer::kCmdInstallPEH)))
      .WillOnce(testing::Return(mock_app_command));

  RunMaybeInstallForSystem(/*is_system_install=*/true);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.System",
      /*SystemInstallerLaunchStatus::kSuccess*/ 0, 1);
}

// Test the successful user-level install case.
TEST_F(PlatformExperienceInstallerWinTest, UserInstall_LaunchSuccess) {
  EXPECT_CALL(mock_delegate_, LaunchProcess(testing::_, testing::_))
      .WillOnce(testing::Return(base::Process::Current()));

  RunMaybeInstallForSystem(/*is_system_install=*/false);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User",
      /*UserInstallerLaunchStatus::kSuccess*/ 0, 1);
}

// Test user install failure due to ERROR_FILE_NOT_FOUND.
TEST_F(PlatformExperienceInstallerWinTest,
       UserInstall_LaunchFails_FileNotFound) {
  EXPECT_CALL(mock_delegate_, LaunchProcess(testing::_, testing::_))
      .WillOnce([](const auto&, const auto&) {
        ::SetLastError(ERROR_FILE_NOT_FOUND);
        return base::Process();  // Return invalid process
      });

  RunMaybeInstallForSystem(/*is_system_install=*/false);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User",
      /*UserInstallerLaunchStatus::kFileNotFound*/ 1, 1);
}

// Test user install failure due to ERROR_ACCESS_DENIED.
TEST_F(PlatformExperienceInstallerWinTest,
       UserInstall_LaunchFails_AccessDenied) {
  EXPECT_CALL(mock_delegate_, LaunchProcess(testing::_, testing::_))
      .WillOnce([](const auto&, const auto&) {
        ::SetLastError(ERROR_ACCESS_DENIED);
        return base::Process();  // Return invalid process
      });

  RunMaybeInstallForSystem(/*is_system_install=*/false);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User",
      /*UserInstallerLaunchStatus::kAccessDenied*/ 2, 1);
}

// Test user install failure due to ERROR_INVALID_PARAMETER.
TEST_F(PlatformExperienceInstallerWinTest,
       UserInstall_LaunchFails_InvalidParameter) {
  EXPECT_CALL(mock_delegate_, LaunchProcess(testing::_, testing::_))
      .WillOnce([](const auto&, const auto&) {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return base::Process();  // Return invalid process
      });

  RunMaybeInstallForSystem(/*is_system_install=*/false);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User",
      /*UserInstallerLaunchStatus::kInvalidParameter*/ 4, 1);
}

// Test user install failure due to ERROR_ELEVATION_REQUIRED.
TEST_F(PlatformExperienceInstallerWinTest,
       UserInstall_LaunchFails_ElevationRequired) {
  EXPECT_CALL(mock_delegate_, LaunchProcess(testing::_, testing::_))
      .WillOnce([](const auto&, const auto&) {
        ::SetLastError(ERROR_ELEVATION_REQUIRED);
        return base::Process();  // Return invalid process
      });

  RunMaybeInstallForSystem(/*is_system_install=*/false);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User",
      /*UserInstallerLaunchStatus::kElevationRequired*/ 5, 1);
}

// Test user install failure due to an error other than the specific ones
// handled.
TEST_F(PlatformExperienceInstallerWinTest,
       UserInstall_LaunchFails_OtherFailure) {
  EXPECT_CALL(mock_delegate_, LaunchProcess(testing::_, testing::_))
      .WillOnce([](const auto&, const auto&) {
        ::SetLastError(ERROR_INVALID_FUNCTION);  // An arbitrary error
        return base::Process();                  // Return invalid process
      });

  RunMaybeInstallForSystem(/*is_system_install=*/false);

  histogram_tester_.ExpectUniqueSample(
      "Windows.PlatformExperienceHelper.InstallerLaunchStatus.User",
      /*UserInstallerLaunchStatus::kOtherFailure*/ 3, 1);
}

}  // namespace platform_experience
