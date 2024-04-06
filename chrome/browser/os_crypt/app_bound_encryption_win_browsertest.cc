// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_win.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/os_crypt/test_support.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"

namespace os_crypt {

namespace {

void WaitForHistogram(const std::string& histogram_name) {
  // Continue if histogram was already recorded.
  if (base::StatisticsRecorder::FindHistogram(histogram_name))
    return;

  // Else, wait until the histogram is recorded.
  base::RunLoop run_loop;
  auto histogram_observer =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name,
          base::BindLambdaForTesting(
              [&](const char* histogram_name, uint64_t name_hash,
                  base::HistogramBase::Sample sample) { run_loop.Quit(); }));
  run_loop.Run();
}

}  // namespace

class AppBoundEncryptionWinTest : public InProcessBrowserTest {
 public:
  AppBoundEncryptionWinTest()
      : scoped_install_details_(std::make_unique<FakeInstallDetails>()) {}

 protected:
  void SetUp() override {
    if (base::GetCurrentProcessIntegrityLevel() != base::HIGH_INTEGRITY)
      GTEST_SKIP() << "Elevation is required for this test.";
    enable_metrics_feature_.InitAndEnableFeature(
        features::kAppBoundEncryptionMetrics);
    maybe_uninstall_service_ = InstallService();
    EXPECT_TRUE(maybe_uninstall_service_.has_value());
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
  }

  base::HistogramTester histogram_tester_;

 private:
  install_static::ScopedInstallDetails scoped_install_details_;
  base::test::ScopedFeatureList enable_metrics_feature_;
  std::optional<base::ScopedClosureRunner> maybe_uninstall_service_;
};

// Test App-Bound is supported for tests.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, Supported) {
  EXPECT_EQ(SupportLevel::kSupported, GetAppBoundEncryptionSupportLevel(
                                          g_browser_process->local_state()));
}

// Test the basic interface to Encrypt and Decrypt data.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, EncryptDecrypt) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  const std::string plaintext("plaintext");
  std::string ciphertext;
  DWORD last_error;

  HRESULT hr =
      EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                            plaintext, ciphertext, last_error);

  ASSERT_HRESULT_SUCCEEDED(hr);

  std::string returned_plaintext;
  hr = DecryptAppBoundString(ciphertext, returned_plaintext, last_error);

  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(plaintext, returned_plaintext);
}

// Test that invalid data is handled correctly.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, EncryptDecryptInvalid) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  std::string ciphertext("invalidciphertext");
  std::string returned_plaintext;
  DWORD last_error = 0;
  std::string log_message;
  const HRESULT hr = DecryptAppBoundString(ciphertext, returned_plaintext,
                                           last_error, &log_message);
  EXPECT_EQ(elevation_service::Elevator::kErrorCouldNotDecryptWithSystemContext,
            hr);
  EXPECT_TRUE(log_message.empty());
}

// These tests verify that the metrics are recorded correctly. The first load of
// browser in the PRE_ test stores the "Test Key" with app-bound encryption and
// the second stage of the test verifies it can be retrieved successfully.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, PRE_MetricsTest) {
  histogram_tester_.ExpectUniqueSample(
      "OSCrypt.AppBoundEncryption.SupportLevel", SupportLevel::kSupported, 1);
  // If the App-Bound provider is enabled, it does the metrics logging.
  if (base::FeatureList::IsEnabled(
          features::kRegisterAppBoundEncryptionProvider)) {
    // These histograms are recorded on a background worker thread, so the test
    // needs to wait until this task completes and the histograms are recorded.
    WaitForHistogram("OSCrypt.AppBoundProvider.Encrypt.ResultCode");
    histogram_tester_.ExpectBucketCount(
        "OSCrypt.AppBoundProvider.Encrypt.ResultCode", S_OK, 1);

    WaitForHistogram("OSCrypt.AppBoundProvider.Encrypt.Time");
  } else {
    WaitForHistogram(
        "OSCrypt.AppBoundEncryption.PathValidation.Encrypt.ResultCode2");
    histogram_tester_.ExpectBucketCount(
        "OSCrypt.AppBoundEncryption.PathValidation.Encrypt.ResultCode2", S_OK,
        1);

    WaitForHistogram("OSCrypt.AppBoundEncryption.PathValidation.Encrypt.Time2");
  }
}

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, MetricsTest) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  // If the App-Bound provider is enabled, it does the metrics logging.
  if (base::FeatureList::IsEnabled(
          features::kRegisterAppBoundEncryptionProvider)) {
    // These histograms are recorded on a background worker thread, so the test
    // needs to wait until this task completes and the histograms are recorded.
    WaitForHistogram("OSCrypt.AppBoundProvider.Decrypt.ResultCode");
    histogram_tester_.ExpectBucketCount(
        "OSCrypt.AppBoundProvider.Decrypt.ResultCode", S_OK, 1);

    WaitForHistogram("OSCrypt.AppBoundProvider.Decrypt.Time");
  } else {
    WaitForHistogram(
        "OSCrypt.AppBoundEncryption.PathValidation.Decrypt.ResultCode2");
    histogram_tester_.ExpectBucketCount(
        "OSCrypt.AppBoundEncryption.PathValidation.Decrypt.ResultCode2", S_OK,
        1);

    WaitForHistogram("OSCrypt.AppBoundEncryption.PathValidation.Decrypt.Time2");
  }
}

// Run this test manually to force uninstall the service using
// --gtest_filter=AppBoundEncryptionWinTest.MANUAL_Uninstall --run-manual.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, MANUAL_Uninstall) {}

using AppBoundEncryptionWinTestNoService = InProcessBrowserTest;

// TODO(https://crbug.com/328398409): Flakily fails
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestNoService, DISABLED_NoService) {
  const std::string plaintext("plaintext");
  std::string ciphertext;
  DWORD last_error;

  HRESULT hr =
      EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                            plaintext, ciphertext, last_error);

  EXPECT_EQ(REGDB_E_CLASSNOTREG, hr);
  EXPECT_EQ(DWORD{ERROR_GEN_FAILURE}, last_error);
}

// This policy test is here and not in chrome/browser/policy/test as it requires
// a fake system install to correctly show as kSupported, and this testing class
// already has the scaffolding in place to achieve this.
class AppBoundEncryptionWinTestWithPolicy
    : public AppBoundEncryptionWinTest,
      public ::testing::WithParamInterface<
          /*policy::key::kApplicationBoundEncryptionEnabled=*/std::optional<
              bool>> {
 private:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PolicyMap values;
    if (GetParam().has_value()) {
      values.Set(policy::key::kApplicationBoundEncryptionEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(*GetParam()),
                 nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    AppBoundEncryptionWinTest::SetUp();
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinTestWithPolicy,
                       TestPolicySupported) {
  const auto support_level =
      GetAppBoundEncryptionSupportLevel(g_browser_process->local_state());
  if (!GetParam().has_value()) {
    EXPECT_EQ(support_level, SupportLevel::kSupported);
    return;
  }

  EXPECT_EQ(support_level, *GetParam() ? SupportLevel::kSupported
                                       : SupportLevel::kDisabledByPolicy);
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    AppBoundEncryptionWinTestWithPolicy,
    ::testing::Values(
        /*policy::key::kApplicationBoundEncryptionEnabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    AppBoundEncryptionWinTestWithPolicy,
    ::testing::Values(
        /*policy::key::kApplicationBoundEncryptionEnabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    AppBoundEncryptionWinTestWithPolicy,
    ::testing::Values(
        /*policy::key::kApplicationBoundEncryptionEnabled=*/std::nullopt));

// These tests do not function correctly in component builds because they rely
// on being able to run a standalone executable child process in various
// different directories, and a component build has too many dynamic DLL
// dependencies to conveniently move around the file system hermetically.
#if !defined(COMPONENT_BUILD)
class AppBoundEncryptionWinTestMultiProcess : public AppBoundEncryptionWinTest {
 protected:
  enum class Operation {
    kEncrypt,
    kDecrypt,
  };

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    AppBoundEncryptionWinTest::SetUp();
  }

  void EncryptOrDecryptInTestProcess(
      base::FilePath::StringPieceType filename,
      std::optional<base::FilePath::StringPieceType> sub_dir,
      const std::string& input_data,
      std::string& output_data,
      Operation op,
      HRESULT& result) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    const auto input_file_path = temp_dir_.GetPath().Append(L"input-file");
    const auto output_file_path = temp_dir_.GetPath().Append(L"output-file");
    ASSERT_TRUE(base::WriteFile(input_file_path, input_data));

    auto executable_file_dir = temp_dir_.GetPath();
    if (sub_dir) {
      executable_file_dir = executable_file_dir.Append(*sub_dir);
      base::CreateDirectory(executable_file_dir);
    }

    const auto executable_file_path = executable_file_dir.Append(filename);
    std::ignore = base::DeleteFile(executable_file_path);

    const auto orig_exe = base::PathService::CheckedGet(base::DIR_EXE)
                              .Append(FILE_PATH_LITERAL("app_binary.exe"));
    ASSERT_TRUE(base::CopyFile(orig_exe, executable_file_path));

    base::CommandLine cmd(executable_file_path);

    cmd.AppendSwitchPath(switches::kAppBoundTestInputFilename, input_file_path);
    cmd.AppendSwitchPath(switches::kAppBoundTestOutputFilename,
                         output_file_path);
    switch (op) {
      case Operation::kEncrypt:
        cmd.AppendSwitch(switches::kAppBoundTestModeEncrypt);
        break;
      case Operation::kDecrypt:
        cmd.AppendSwitch(switches::kAppBoundTestModeDecrypt);
        break;
    }

    base::LaunchOptions options;
    options.start_hidden = true;
    options.wait = true;

    auto process = base::LaunchProcess(cmd, options);
    int exit_code;
    EXPECT_TRUE(process.WaitForExit(&exit_code));
    result = static_cast<HRESULT>(exit_code);
    if (SUCCEEDED(result)) {
      EXPECT_TRUE(base::ReadFileToString(output_file_path, &output_data));
    }
    // This ensures the process has really terminated before this function
    // returns, as base::Process destructor does not do this by default.
    process.Terminate(0, /*wait=*/true);
  }

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestMultiProcess,
                       EncryptDecryptProcess) {
  const std::string kSecret("secret");
  {
    std::string ciphertext;
    HRESULT result;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, kSecret, ciphertext, Operation::kEncrypt, result));
    EXPECT_EQ(S_OK, result);
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, ciphertext, plaintext, Operation::kDecrypt, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app2.exe", {}, ciphertext, plaintext, Operation::kDecrypt, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(
        EncryptOrDecryptInTestProcess(L"app1.exe", L"Application", ciphertext,
                                      plaintext, Operation::kDecrypt, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(
        EncryptOrDecryptInTestProcess(L"app1.exe", L"Temp", ciphertext,
                                      plaintext, Operation::kDecrypt, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(
        EncryptOrDecryptInTestProcess(L"app1.exe", L"Bad", ciphertext,
                                      plaintext, Operation::kDecrypt, result));
    EXPECT_EQ(elevation_service::Elevator::kValidationDidNotPass, result);
  }
  {
    // Explicitly test the most frequent chrome-specific cases.
    std::string ciphertext;
    HRESULT result;
    ASSERT_NO_FATAL_FAILURE(
        EncryptOrDecryptInTestProcess(L"chrome.exe", L"Application", kSecret,
                                      ciphertext, Operation::kEncrypt, result));
    EXPECT_EQ(S_OK, result);
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"new_chrome.exe", L"Application", ciphertext, plaintext,
        Operation::kDecrypt, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);
    ASSERT_NO_FATAL_FAILURE(
        EncryptOrDecryptInTestProcess(L"old_chrome.exe", L"Temp", ciphertext,
                                      plaintext, Operation::kDecrypt, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);
  }
}
#endif  // !defined(COMPONENT_BUILD)

}  // namespace os_crypt
