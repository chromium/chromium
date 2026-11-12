// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/installer/installer_win.h"

#include <shobjidl.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_variant.h"
#include "base/win/win_util.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "chrome/updater/branded_constants.h"
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
  base::FilePath GetExpectedHelperPath(bool is_system_install) {
    const int path_key = is_system_install
                             ? static_cast<int>(base::DIR_EXE)
                             : static_cast<int>(chrome::DIR_USER_DATA);
    return base::PathService::CheckedGet(path_key)
        .Append(L"PlatformExperienceHelper")
        .Append(L"platform_experience_helper.exe");
  }

  // Simulates that the Platform Experience Helper is already installed by
  // creating its executable in the expected location.
  void CreateFakeHelperExecutable(bool is_system_install) {
    const base::FilePath peh_exe_path =
        GetExpectedHelperPath(is_system_install);
    ASSERT_TRUE(base::CreateDirectory(peh_exe_path.DirName()));
    ASSERT_TRUE(base::WriteFile(peh_exe_path, ""));
  }

  base::Version CreateVersionedHelperExecutable(bool is_system_install) {
    const base::FilePath peh_exe_path =
        GetExpectedHelperPath(is_system_install);
    EXPECT_TRUE(base::CreateDirectory(peh_exe_path.DirName()));
    // Copy a small system DLL so FileVersionInfoWin reads a real
    // VS_FIXEDFILEINFO block without depending on external test data files.
    const base::FilePath source_pe =
        base::PathService::CheckedGet(base::DIR_SYSTEM).Append(L"version.dll");
    EXPECT_TRUE(base::CopyFile(source_pe, peh_exe_path));
    base::Version version = PehLauncher().GetBinaryVersion(peh_exe_path);
    EXPECT_TRUE(version.IsValid());
    return version;
  }

  void CreateOmahaClientsKey(bool is_system_scope,
                             std::optional<std::wstring_view> pv) {
    ASSERT_EQ(updater::kPlatformExperienceHelperAppId[0], '{');
    const std::wstring key_path = base::StrCat(
        {L"Software\\Google\\Update\\Clients\\",
         base::ASCIIToWide(updater::kPlatformExperienceHelperAppId)});
    base::win::RegKey key(
        is_system_scope ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
        key_path.c_str(), KEY_WRITE | KEY_WOW64_32KEY);
    ASSERT_TRUE(key.Valid());
    if (pv.has_value()) {
      ASSERT_EQ(key.WriteValue(L"pv", std::wstring(*pv).c_str()),
                ERROR_SUCCESS);
    }
  }

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
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
  registry_util::RegistryOverrideManager registry_override_manager_;
  // These members override the respective paths with temporary directories
  // for the duration of the test.
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
  base::ScopedPathOverride exe_dir_override_{base::DIR_EXE};
  base::ScopedPathOverride module_dir_override_{base::DIR_MODULE};

  // Ensure tests run consistently regardless of whether the host machine is
  // domain-joined or enterprise-managed.
  base::win::ScopedDomainStateForTesting scoped_domain_{false};
  base::win::ScopedDeviceRegisteredWithManagementForTesting scoped_mdm_{false};
  base::win::ScopedAzureADJoinStateForTesting scoped_aad_{std::nullopt};
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

TEST_F(PlatformExperienceInstallerWinTest, ManagedDevice_RecordsNoMetrics) {
  base::win::ScopedDomainStateForTesting domain_joined(true);
  RunMaybeInstallForSystem(/*is_system_install=*/false);

  histogram_tester_.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.InstallState.User", 0);
  histogram_tester_.ExpectTotalCount(
      "Windows.PlatformExperienceHelper.OtherScopeRegistration.User", 0);
}

enum class BinaryKind { kNone, kUnversioned, kVersioned };
enum class PvKind { kNoKey, kMissingValue, kMatchingBinary, kLiteral };

struct InstallStateTestCase {
  const char* name;
  bool is_system_install;
  BinaryKind binary;
  PvKind current_pv_kind;
  const wchar_t* current_pv_literal = L"";
  PvKind other_pv_kind = PvKind::kNoKey;
  const wchar_t* other_pv_literal = L"";
  PlatformExperienceHelperInstallState expected_state;
  PlatformExperienceHelperOtherScopeRegistration expected_other_scope =
      PlatformExperienceHelperOtherScopeRegistration::kNotRegistered;
};

class PlatformExperienceInstallStateMetricsTest
    : public PlatformExperienceInstallerWinTest,
      public testing::WithParamInterface<InstallStateTestCase> {};

TEST_P(PlatformExperienceInstallStateMetricsTest, RecordsExpectedMetrics) {
  const InstallStateTestCase& tc = GetParam();

  std::wstring binary_version_str;
  if (tc.binary == BinaryKind::kUnversioned) {
    CreateFakeHelperExecutable(tc.is_system_install);
  } else if (tc.binary == BinaryKind::kVersioned) {
    binary_version_str = base::ASCIIToWide(
        CreateVersionedHelperExecutable(tc.is_system_install).GetString());
  }

  auto write_reg = [&](bool system_scope, PvKind kind, const wchar_t* literal) {
    if (kind == PvKind::kMissingValue) {
      CreateOmahaClientsKey(system_scope, std::nullopt);
    } else if (kind == PvKind::kMatchingBinary) {
      CreateOmahaClientsKey(system_scope, binary_version_str);
    } else if (kind == PvKind::kLiteral) {
      CreateOmahaClientsKey(system_scope, literal);
    }
  };
  write_reg(tc.is_system_install, tc.current_pv_kind, tc.current_pv_literal);
  write_reg(!tc.is_system_install, tc.other_pv_kind, tc.other_pv_literal);

  if (tc.binary == BinaryKind::kNone) {
    if (tc.is_system_install) {
      EXPECT_CALL(mock_delegate_,
                  GetUpdaterAppCommand(std::wstring(installer::kCmdInstallPEH)))
          .WillOnce(testing::Return(nullptr));
    } else {
      EXPECT_CALL(mock_delegate_, LaunchProcess(testing::_, testing::_))
          .WillOnce(testing::Return(base::Process::Current()));
    }
  }

  RunMaybeInstallForSystem(tc.is_system_install);

  const std::string_view active = tc.is_system_install ? "System" : "User";
  const std::string_view inactive = tc.is_system_install ? "User" : "System";
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({"Windows.PlatformExperienceHelper.InstallState.", active}),
      tc.expected_state, 1);
  histogram_tester_.ExpectTotalCount(
      base::StrCat(
          {"Windows.PlatformExperienceHelper.InstallState.", inactive}),
      0);
  histogram_tester_.ExpectUniqueSample(
      base::StrCat(
          {"Windows.PlatformExperienceHelper.OtherScopeRegistration.", active}),
      tc.expected_other_scope, 1);
  histogram_tester_.ExpectTotalCount(
      base::StrCat({"Windows.PlatformExperienceHelper.OtherScopeRegistration.",
                    inactive}),
      0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PlatformExperienceInstallStateMetricsTest,
    testing::Values(
        InstallStateTestCase{
            .name = "User_NotInstalled",
            .is_system_install = false,
            .binary = BinaryKind::kNone,
            .current_pv_kind = PvKind::kNoKey,
            .expected_state =
                PlatformExperienceHelperInstallState::kNotInstalled,
        },
        InstallStateTestCase{
            .name = "User_Healthy",
            .is_system_install = false,
            .binary = BinaryKind::kVersioned,
            .current_pv_kind = PvKind::kMatchingBinary,
            .expected_state = PlatformExperienceHelperInstallState::kHealthy,
        },
        InstallStateTestCase{
            .name = "System_Healthy_WithUserOtherScopeVersion",
            .is_system_install = true,
            .binary = BinaryKind::kVersioned,
            .current_pv_kind = PvKind::kMatchingBinary,
            .other_pv_kind = PvKind::kLiteral,
            .other_pv_literal = L"2.0.0.0",
            .expected_state = PlatformExperienceHelperInstallState::kHealthy,
            .expected_other_scope =
                PlatformExperienceHelperOtherScopeRegistration::
                    kRegisteredWithVersion,
        },
        InstallStateTestCase{
            .name = "User_BinaryNoRegistration_NoKey",
            .is_system_install = false,
            .binary = BinaryKind::kVersioned,
            .current_pv_kind = PvKind::kNoKey,
            .expected_state =
                PlatformExperienceHelperInstallState::kBinaryNoRegistration,
        },
        InstallStateTestCase{
            .name = "User_BinaryNoRegistration_EmptyPv",
            .is_system_install = false,
            .binary = BinaryKind::kVersioned,
            .current_pv_kind = PvKind::kLiteral,
            .current_pv_literal = L"",
            .expected_state =
                PlatformExperienceHelperInstallState::kBinaryNoRegistration,
        },
        InstallStateTestCase{
            .name = "User_VersionMismatch_DifferentPv",
            .is_system_install = false,
            .binary = BinaryKind::kVersioned,
            .current_pv_kind = PvKind::kLiteral,
            .current_pv_literal = L"999.0.0.1",
            .expected_state =
                PlatformExperienceHelperInstallState::kVersionMismatch,
        },
        InstallStateTestCase{
            .name = "User_BinaryRegisteredZeroVersion_ZeroPv",
            .is_system_install = false,
            .binary = BinaryKind::kVersioned,
            .current_pv_kind = PvKind::kLiteral,
            .current_pv_literal = L"0.0.0.0",
            .expected_state = PlatformExperienceHelperInstallState::
                kBinaryRegisteredZeroVersion,
        },
        InstallStateTestCase{
            .name = "User_VersionMismatch_UnreadableBinary",
            .is_system_install = false,
            .binary = BinaryKind::kUnversioned,
            .current_pv_kind = PvKind::kLiteral,
            .current_pv_literal = L"1.2.3.4",
            .expected_state =
                PlatformExperienceHelperInstallState::kVersionMismatch,
        },
        InstallStateTestCase{
            .name = "User_RegisteredNoBinary_WithSystemZeroOtherScope",
            .is_system_install = false,
            .binary = BinaryKind::kNone,
            .current_pv_kind = PvKind::kLiteral,
            .current_pv_literal = L"1.2.3.4",
            .other_pv_kind = PvKind::kLiteral,
            .other_pv_literal = L"0.0.0.0",
            .expected_state =
                PlatformExperienceHelperInstallState::kRegisteredNoBinary,
            .expected_other_scope =
                PlatformExperienceHelperOtherScopeRegistration::
                    kRegisteredZeroVersion,
        },
        InstallStateTestCase{
            .name = "User_RegisteredZeroVersion_ZeroPv",
            .is_system_install = false,
            .binary = BinaryKind::kNone,
            .current_pv_kind = PvKind::kLiteral,
            .current_pv_literal = L"0.0.0.0",
            .expected_state =
                PlatformExperienceHelperInstallState::kRegisteredZeroVersion,
        },
        InstallStateTestCase{
            .name = "System_NotInstalled_MissingPv",
            .is_system_install = true,
            .binary = BinaryKind::kNone,
            .current_pv_kind = PvKind::kMissingValue,
            .other_pv_kind = PvKind::kMissingValue,
            .expected_state =
                PlatformExperienceHelperInstallState::kNotInstalled,
            .expected_other_scope =
                PlatformExperienceHelperOtherScopeRegistration::kNotRegistered,
        }),
    [](const testing::TestParamInfo<InstallStateTestCase>& info) {
      return info.param.name;
    });

}  // namespace platform_experience
