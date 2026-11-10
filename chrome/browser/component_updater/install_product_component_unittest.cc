// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/install_product_component.h"

#include <array>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/child_module/child_module_paths.h"  // nogncheck
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "content/public/test/browser_task_environment.h"

namespace component_updater {

using ::testing::_;
using Step = ProductComponentInstallOutcome::Step;

// Prints an outcome in test failure messages.
void PrintTo(const ProductComponentInstallOutcome& outcome, std::ostream* os) {
  *os << "{step: " << static_cast<int>(outcome.step)
      << ", extra_code: " << outcome.extra_code << "}";
}

namespace {

// The tag of the histograms in which tests record outcomes.
constexpr char kTag[] = "Test";

}  // namespace

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

class MockProductComponentInstallerDelegate
    : public ProductComponentInstallerDelegate {
 public:
  MockProductComponentInstallerDelegate() = default;

  MOCK_METHOD((base::expected<Microsoft::WRL::ComPtr<IAppCommandWeb>, HRESULT>),
              GetAppCommand,
              (const std::wstring&),
              (override));
  MOCK_METHOD(base::Process,
              LaunchProcess,
              (const base::CommandLine&, const base::LaunchOptions&),
              (override));
  MOCK_METHOD(void, WaitBeforeRetry, (base::TimeDelta), (override));
};

// Returns an AppCommand that runs to completion with `exit_code`.
Microsoft::WRL::ComPtr<MockAppCommand> MakeCompletedAppCommand(
    DWORD exit_code) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([exit_code](DWORD* code) {
        *code = exit_code;
        return S_OK;
      });
  return mock_app_command;
}

// Matches a VARIANT holding a BSTR equal to `expected`.
MATCHER_P(IsBstrVariant, expected, "") {
  return V_VT(&arg) == VT_BSTR && V_BSTR(&arg) &&
         std::wstring_view(V_BSTR(&arg)) == expected;
}

class InstallProductComponentWindowsTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    inner_crx_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test.crx3"));
    ASSERT_TRUE(base::WriteFile(inner_crx_, "dummy_crx_data"));
  }

  // Expects `mock_delegate_` to provide the "install-component" App Command,
  // which completes with `exit_code`.
  void ExpectAppCommandExitCode(DWORD exit_code) {
    EXPECT_CALL(mock_delegate_,
                GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
        .WillOnce(testing::Return(MakeCompletedAppCommand(exit_code)));
  }

  content::BrowserTaskEnvironment env_;
  install_static::ScopedInstallDetails scoped_install_details_{
      /*system_level=*/true};
  base::ScopedTempDir temp_dir_;
  base::FilePath inner_crx_;
  MockProductComponentInstallerDelegate mock_delegate_;
};

TEST_F(InstallProductComponentWindowsTest, Succeeded) {
  EXPECT_TRUE((ProductComponentInstallOutcome{
                   .step = Step::kInstallerExited,
                   .extra_code = installer::INSTALL_COMPONENT_SUCCESS})
                  .Succeeded());
  EXPECT_TRUE((ProductComponentInstallOutcome{
                   .step = Step::kInstallerExited,
                   .extra_code = installer::INSTALL_COMPONENT_ALREADY_EXISTS})
                  .Succeeded());
  EXPECT_FALSE((ProductComponentInstallOutcome{
                    .step = Step::kInstallerExited,
                    .extra_code = installer::INSTALL_COMPONENT_FAILED_INTERNAL})
                   .Succeeded());
  // Codes are only meaningful in the context of their step.
  EXPECT_FALSE((ProductComponentInstallOutcome{
                    .step = Step::kLaunchFailed,
                    .extra_code = installer::INSTALL_COMPONENT_SUCCESS})
                   .Succeeded());
}

TEST_F(InstallProductComponentWindowsTest, MissingInnerCrx) {
  base::FilePath missing_crx =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("nonexistent.crx3"));
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, missing_crx, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{.step = Step::kInvalidInput}));
}

TEST_F(InstallProductComponentWindowsTest, GetAppCommandFailed) {
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(
          base::unexpected(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kGetAppCommandFailed,
                .extra_code = static_cast<int>(
                    HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))}));
}

TEST_F(InstallProductComponentWindowsTest, ExecuteAccessDenied) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(E_ACCESSDENIED));

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kComError,
                .extra_code = static_cast<int>(E_ACCESSDENIED)}));
}

TEST_F(InstallProductComponentWindowsTest, GetStatusServerDied) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce(testing::Return(RPC_E_SERVER_DIED));

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kComError,
                .extra_code = static_cast<int>(RPC_E_SERVER_DIED)}));
}

TEST_F(InstallProductComponentWindowsTest, CommandStatusError) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_ERROR;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{.step = Step::kLaunchFailed}));
  // The App Command reports no code when it fails to launch setup.exe.
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "ComponentUpdater.ProductComponent.Test.InstallCode."),
              testing::IsEmpty());
}

TEST_F(InstallProductComponentWindowsTest, Success) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_SUCCESS;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code =
                    static_cast<int>(installer::INSTALL_COMPONENT_SUCCESS)}));
}

TEST_F(InstallProductComponentWindowsTest, AlreadyExists) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_ALREADY_EXISTS;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = static_cast<int>(
                    installer::INSTALL_COMPONENT_ALREADY_EXISTS)}));
}

TEST_F(InstallProductComponentWindowsTest, SignatureFailure) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_FAILED_SIGNATURE;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = static_cast<int>(
                    installer::INSTALL_COMPONENT_FAILED_SIGNATURE)}));
}

TEST_F(InstallProductComponentWindowsTest, InvalidInput) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_INVALID_INPUT;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = static_cast<int>(
                    installer::INSTALL_COMPONENT_INVALID_INPUT)}));
}

TEST_F(InstallProductComponentWindowsTest, InternalFailure) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_FAILED_INTERNAL;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = static_cast<int>(
                    installer::INSTALL_COMPONENT_FAILED_INTERNAL)}));
}

// setup.exe's exit code is returned even if it is not one of the codes that
// setup.exe reports for component installation.
TEST_F(InstallProductComponentWindowsTest, OtherExitCode) {
  ExpectAppCommandExitCode(installer::OS_ERROR);

  EXPECT_EQ(
      InstallProductComponentForTesting(kTag, inner_crx_, /*user_data_dir=*/{},
                                        &mock_delegate_),
      (ProductComponentInstallOutcome{.step = Step::kInstallerExited,
                                      .extra_code = installer::OS_ERROR}));
}

// The step and setup.exe's exit code are recorded for the component.
TEST_F(InstallProductComponentWindowsTest, RecordsInstallerExitCode) {
  ExpectAppCommandExitCode(installer::OS_ERROR);

  base::HistogramTester histogram_tester;
  InstallProductComponentForTesting(kTag, inner_crx_, /*user_data_dir=*/{},
                                    &mock_delegate_);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallStep.SystemLevel",
      Step::kInstallerExited, 1);
  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallCode.InstallerExited."
      "SystemLevel",
      installer::OS_ERROR, 1);
  histogram_tester.ExpectTotalCount(
      "ComponentUpdater.ProductComponent.Test.InstallDuration.SystemLevel", 1);
  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InnerCrxOnNetworkDrive."
      "SystemLevel",
      false, 1);
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "ComponentUpdater.ProductComponent."),
              testing::SizeIs(4));
}

// The HRESULT of a failed call to the App Command is recorded.
TEST_F(InstallProductComponentWindowsTest, RecordsComError) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(), execute(_, _, _, _, _, _, _, _, _))
      .WillOnce(testing::Return(E_ACCESSDENIED));
  EXPECT_CALL(mock_delegate_,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  base::HistogramTester histogram_tester;
  InstallProductComponentForTesting(kTag, inner_crx_, /*user_data_dir=*/{},
                                    &mock_delegate_);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallStep.SystemLevel",
      Step::kComError, 1);
  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallCode.ComError.SystemLevel",
      static_cast<int>(E_ACCESSDENIED), 1);
}

// No code is recorded for steps that have none.
TEST_F(InstallProductComponentWindowsTest, RecordsInvalidInput) {
  base::HistogramTester histogram_tester;
  InstallProductComponentForTesting(
      kTag, temp_dir_.GetPath().Append(FILE_PATH_LITERAL("nonexistent.crx3")),
      /*user_data_dir=*/{}, &mock_delegate_);

  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallStep.SystemLevel",
      Step::kInvalidInput, 1);
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "ComponentUpdater.ProductComponent.Test.InstallCode."),
              testing::IsEmpty());
}

// Installation is retried after increasing delays while setup.exe cannot
// acquire the setup singleton, and succeeds once it can. Only the outcome of
// the last attempt is recorded.
TEST_F(InstallProductComponentWindowsTest, SetupSingletonBusyThenSuccess) {
  auto busy1 =
      MakeCompletedAppCommand(installer::SETUP_SINGLETON_ACQUISITION_FAILED);
  auto busy2 =
      MakeCompletedAppCommand(installer::SETUP_SINGLETON_ACQUISITION_FAILED);
  auto success = MakeCompletedAppCommand(installer::INSTALL_COMPONENT_SUCCESS);
  {
    testing::InSequence sequence;
    EXPECT_CALL(mock_delegate_,
                GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
        .WillOnce(testing::Return(busy1));
    EXPECT_CALL(mock_delegate_, WaitBeforeRetry(base::Seconds(10)));
    EXPECT_CALL(mock_delegate_,
                GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
        .WillOnce(testing::Return(busy2));
    EXPECT_CALL(mock_delegate_, WaitBeforeRetry(base::Seconds(20)));
    EXPECT_CALL(mock_delegate_,
                GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
        .WillOnce(testing::Return(success));
  }

  base::HistogramTester histogram_tester;
  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate_),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = installer::INSTALL_COMPONENT_SUCCESS}));
  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallStep.SystemLevel",
      Step::kInstallerExited, 1);
  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallCode.InstallerExited."
      "SystemLevel",
      installer::INSTALL_COMPONENT_SUCCESS, 1);
}

// Installation gives up after a bounded number of retries if setup.exe
// repeatedly cannot acquire the setup singleton.
TEST_F(InstallProductComponentWindowsTest, SetupSingletonBusyRetriesExhausted) {
  // One initial attempt plus three retries.
  std::vector<Microsoft::WRL::ComPtr<MockAppCommand>> busy_commands;
  for (int i = 0; i < 4; ++i) {
    busy_commands.push_back(
        MakeCompletedAppCommand(installer::SETUP_SINGLETON_ACQUISITION_FAILED));
  }
  {
    testing::InSequence sequence;
    constexpr auto kDelays = std::to_array<base::TimeDelta>(
        {base::Seconds(10), base::Seconds(20), base::Seconds(40)});
    for (size_t i = 0; i < busy_commands.size(); ++i) {
      EXPECT_CALL(mock_delegate_,
                  GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
          .WillOnce(testing::Return(busy_commands[i]));
      if (i < kDelays.size()) {
        EXPECT_CALL(mock_delegate_, WaitBeforeRetry(kDelays[i]));
      }
    }
  }

  base::HistogramTester histogram_tester;
  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate_),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = installer::SETUP_SINGLETON_ACQUISITION_FAILED}));
  histogram_tester.ExpectUniqueSample(
      "ComponentUpdater.ProductComponent.Test.InstallCode.InstallerExited."
      "SystemLevel",
      installer::SETUP_SINGLETON_ACQUISITION_FAILED, 1);
}

// Failures other than setup singleton contention are not retried.
TEST_F(InstallProductComponentWindowsTest, OtherFailuresNotRetried) {
  ExpectAppCommandExitCode(installer::INSTALL_COMPONENT_FAILED_INTERNAL);
  EXPECT_CALL(mock_delegate_, WaitBeforeRetry(_)).Times(0);

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate_),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = installer::INSTALL_COMPONENT_FAILED_INTERNAL}));
}

// A component for a specific User Data directory is installed via the
// "install-component-for-user" command, which takes the canonicalized User Data
// directory as its second parameter.
TEST_F(InstallProductComponentWindowsTest, ParametersPassedToExecute) {
  const base::FilePath udd =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("User Data"));
  ASSERT_TRUE(base::CreateDirectory(udd));
  const base::FilePath canonical_udd =
      child_module::CanonicalizeUserDataDir(udd);
  ASSERT_FALSE(canonical_udd.empty());

  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(*mock_app_command.Get(),
              execute(IsBstrVariant(inner_crx_.value()),
                      IsBstrVariant(canonical_udd.value()),
                      testing::Field(&VARIANT::vt, VT_EMPTY), _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_SUCCESS;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(
      mock_delegate,
      GetAppCommand(std::wstring(installer::kCmdInstallComponentForUser)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(
      InstallProductComponentForTesting(kTag, inner_crx_, udd, &mock_delegate),
      (ProductComponentInstallOutcome{
          .step = Step::kInstallerExited,
          .extra_code =
              static_cast<int>(installer::INSTALL_COMPONENT_SUCCESS)}));
}

// A component that is not for a specific User Data directory is installed via
// the "install-component" command, which takes only the CRX path.
TEST_F(InstallProductComponentWindowsTest,
       ParametersPassedToExecuteWithoutUdd) {
  auto mock_app_command = Microsoft::WRL::Make<MockAppCommand>();
  EXPECT_CALL(
      *mock_app_command.Get(),
      execute(IsBstrVariant(inner_crx_.value()),
              testing::Field(&VARIANT::vt, VT_EMPTY), _, _, _, _, _, _, _))
      .WillOnce(testing::Return(S_OK));
  EXPECT_CALL(*mock_app_command.Get(), get_status(_))
      .WillOnce([](UINT* status) {
        *status = COMMAND_STATUS_COMPLETE;
        return S_OK;
      });
  EXPECT_CALL(*mock_app_command.Get(), get_exitCode(_))
      .WillOnce([](DWORD* exit_code) {
        *exit_code = installer::INSTALL_COMPONENT_SUCCESS;
        return S_OK;
      });

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate,
              GetAppCommand(std::wstring(installer::kCmdInstallComponent)))
      .WillOnce(testing::Return(mock_app_command));

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code =
                    static_cast<int>(installer::INSTALL_COMPONENT_SUCCESS)}));
}

// No command is run if the User Data directory cannot be canonicalized.
TEST_F(InstallProductComponentWindowsTest, UddNotFound) {
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, GetAppCommand(_)).Times(0);

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_,
                temp_dir_.GetPath().Append(FILE_PATH_LITERAL("nonexistent")),
                &mock_delegate),
            (ProductComponentInstallOutcome{.step = Step::kInvalidInput}));
}

class InstallProductComponentWindowsPerUserTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    inner_crx_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("test.crx3"));
    ASSERT_TRUE(base::WriteFile(inner_crx_, "dummy_crx_data"));

    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    SetInstalled(true);
  }

  void SetInstalled(bool installed) {
    base::FilePath setup_dir = base::PathService::CheckedGet(base::DIR_EXE)
                                   .AppendASCII("1.0.0.0")
                                   .Append(installer::kInstallerDir);
    if (!installed) {
      base::DeletePathRecursively(
          base::PathService::CheckedGet(base::DIR_EXE).AppendASCII("1.0.0.0"));
    } else {
      base::CreateDirectory(setup_dir);
      base::WriteFile(setup_dir.Append(installer::kSetupExe), "dummy");
    }

    if (!installed) {
      base::win::RegKey client_state_key(
          HKEY_CURRENT_USER, install_static::GetClientStateKeyPath().c_str(),
          KEY_SET_VALUE | KEY_WOW64_32KEY);
      client_state_key.DeleteValue(installer::kUninstallStringField);
    } else {
      base::win::RegKey clients_key(HKEY_CURRENT_USER,
                                    install_static::GetClientsKeyPath().c_str(),
                                    KEY_SET_VALUE | KEY_WOW64_32KEY);
      clients_key.WriteValue(google_update::kRegVersionField, L"1.0.0.0");

      base::win::RegKey client_state_key(
          HKEY_CURRENT_USER, install_static::GetClientStateKeyPath().c_str(),
          KEY_SET_VALUE | KEY_WOW64_32KEY);
      client_state_key.WriteValue(
          installer::kUninstallStringField,
          base::PathService::CheckedGet(base::DIR_EXE)
              .AppendASCII("1.0.0.0\\Installer\\setup.exe")
              .value()
              .c_str());
    }
  }

  content::BrowserTaskEnvironment env_;
  install_static::ScopedInstallDetails scoped_install_details_{
      /*system_level=*/false};
  base::ScopedTempDir temp_dir_;
  base::FilePath inner_crx_;
  base::ScopedPathOverride dir_exe_override_{base::DIR_EXE};
  base::ScopedPathOverride file_exe_override_{
      base::FILE_EXE, base::PathService::CheckedGet(base::DIR_EXE)
                          .Append(installer::kChromeExe)};
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(InstallProductComponentWindowsPerUserTest, NotInstalled) {
  SetInstalled(false);
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, LaunchProcess(_, _)).Times(0);
  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{.step = Step::kNotInstalled}));
}

// A missing setup.exe is reported as a launch failure with its error code.
TEST_F(InstallProductComponentWindowsPerUserTest, SetupNotFound) {
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, LaunchProcess(_, _))
      .WillOnce([](const base::CommandLine&, const base::LaunchOptions&) {
        ::SetLastError(ERROR_FILE_NOT_FOUND);
        return base::Process();
      });

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kLaunchFailed,
                .extra_code = static_cast<int>(ERROR_FILE_NOT_FOUND)}));
}

TEST_F(InstallProductComponentWindowsPerUserTest, LaunchFailed) {
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, LaunchProcess(_, _))
      .WillOnce([](const base::CommandLine&, const base::LaunchOptions&) {
        ::SetLastError(ERROR_ACCESS_DENIED);
        return base::Process();
      });

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kLaunchFailed,
                .extra_code = static_cast<int>(ERROR_ACCESS_DENIED)}));
}

TEST_F(InstallProductComponentWindowsPerUserTest, CommandLineParameters) {
  const base::FilePath udd =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("User Data"));
  ASSERT_TRUE(base::CreateDirectory(udd));
  const base::FilePath canonical_udd =
      child_module::CanonicalizeUserDataDir(udd);
  ASSERT_FALSE(canonical_udd.empty());

  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, LaunchProcess(_, _))
      .WillOnce([&](const base::CommandLine& cmd,
                    const base::LaunchOptions& options) {
        EXPECT_TRUE(cmd.HasSwitch(installer::switches::kInstallComponent));
        EXPECT_EQ(
            cmd.GetSwitchValuePath(installer::switches::kInstallComponent),
            inner_crx_);
        EXPECT_TRUE(cmd.HasSwitch(installer::switches::kUdd));
        EXPECT_EQ(cmd.GetSwitchValuePath(installer::switches::kUdd),
                  canonical_udd);
        // A per-user setup.exe uses the SID of its own process.
        EXPECT_FALSE(cmd.HasSwitch(installer::switches::kUserSid));
        return base::Process();
      });

  EXPECT_EQ(
      InstallProductComponentForTesting(kTag, inner_crx_, udd, &mock_delegate)
          .step,
      Step::kLaunchFailed);
}

// setup.exe is not launched if the User Data directory cannot be canonicalized.
TEST_F(InstallProductComponentWindowsPerUserTest, UddNotFound) {
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, LaunchProcess(_, _)).Times(0);

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_,
                temp_dir_.GetPath().Append(FILE_PATH_LITERAL("nonexistent")),
                &mock_delegate),
            (ProductComponentInstallOutcome{.step = Step::kInvalidInput}));
}

TEST_F(InstallProductComponentWindowsPerUserTest, CommandLineWithoutUdd) {
  MockProductComponentInstallerDelegate mock_delegate;
  EXPECT_CALL(mock_delegate, LaunchProcess(_, _))
      .WillOnce(
          [](const base::CommandLine& cmd, const base::LaunchOptions& options) {
            EXPECT_TRUE(cmd.HasSwitch(installer::switches::kInstallComponent));
            EXPECT_FALSE(cmd.HasSwitch(installer::switches::kUdd));
            return base::Process();
          });

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate)
                .step,
            Step::kLaunchFailed);
}

// Per-user installation is also retried while setup.exe cannot acquire the
// setup singleton.
TEST_F(InstallProductComponentWindowsPerUserTest,
       SetupSingletonBusyThenSuccess) {
  auto launch_exiting_with = [](int exit_code) {
    return [exit_code](const base::CommandLine&,
                       const base::LaunchOptions& options) {
      base::CommandLine cmd(base::FilePath(FILE_PATH_LITERAL("cmd.exe")));
      cmd.AppendArg("/c");
      cmd.AppendArg("exit " + base::NumberToString(exit_code));
      return base::LaunchProcess(cmd, options);
    };
  };

  MockProductComponentInstallerDelegate mock_delegate;
  {
    testing::InSequence sequence;
    EXPECT_CALL(mock_delegate, LaunchProcess(_, _))
        .WillOnce(
            launch_exiting_with(installer::SETUP_SINGLETON_ACQUISITION_FAILED));
    EXPECT_CALL(mock_delegate, WaitBeforeRetry(base::Seconds(10)));
    EXPECT_CALL(mock_delegate, LaunchProcess(_, _))
        .WillOnce(launch_exiting_with(installer::INSTALL_COMPONENT_SUCCESS));
  }

  EXPECT_EQ(InstallProductComponentForTesting(
                kTag, inner_crx_, /*user_data_dir=*/{}, &mock_delegate),
            (ProductComponentInstallOutcome{
                .step = Step::kInstallerExited,
                .extra_code = installer::INSTALL_COMPONENT_SUCCESS}));
}

}  // namespace component_updater

#endif  // BUILDFLAG(IS_WIN)
