// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_win.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/containers/span.h"
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
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/browser/os_crypt/test_support.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/windows_services/service_program/test_support/scoped_log_grabber.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

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

os_crypt_async::Encryptor GetInstanceSync(
    os_crypt_async::OSCryptAsync& factory,
    os_crypt_async::Encryptor::Option option =
        os_crypt_async::Encryptor::Option::kNone) {
  base::RunLoop run_loop;
  std::optional<os_crypt_async::Encryptor> encryptor;
  auto sub = factory.GetInstance(
      base::BindLambdaForTesting(
          [&](os_crypt_async::Encryptor instance, bool result) {
            EXPECT_TRUE(result);
            encryptor.emplace(std::move(instance));
            run_loop.Quit();
          }),
      option);
  run_loop.Run();
  return std::move(*encryptor);
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
    maybe_uninstall_service_ = InstallService(log_grabber_);
    EXPECT_TRUE(maybe_uninstall_service_.has_value());
    InProcessBrowserTest::SetUp();
  }

  base::HistogramTester histogram_tester_;
  std::optional<base::ScopedClosureRunner> maybe_uninstall_service_;
  ScopedLogGrabber log_grabber_;

 private:
  install_static::ScopedInstallDetails scoped_install_details_;
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
  std::optional<std::string> maybe_new_ciphertext;
  hr = DecryptAppBoundString(ciphertext, returned_plaintext,
                             ProtectionLevel::PROTECTION_PATH_VALIDATION,
                             maybe_new_ciphertext, last_error);
  EXPECT_FALSE(maybe_new_ciphertext);
  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(plaintext, returned_plaintext);
}

// Test the basic interface to Encrypt and Decrypt data.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, EncryptDecryptWithFlags) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  const std::string plaintext("plaintext");
  std::string ciphertext;
  DWORD last_error;
  elevation_service::EncryptFlags flags;
  flags.use_latest_key = true;
  HRESULT hr =
      EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                            plaintext, ciphertext, last_error, &flags);

  ASSERT_HRESULT_SUCCEEDED(hr);

  std::string returned_plaintext;
  std::optional<std::string> maybe_new_ciphertext;
  hr = DecryptAppBoundString(ciphertext, returned_plaintext,
                             ProtectionLevel::PROTECTION_PATH_VALIDATION,
                             maybe_new_ciphertext, last_error);
  EXPECT_FALSE(maybe_new_ciphertext);
  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(plaintext, returned_plaintext);
}

// Test that invalid data is handled correctly.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, EncryptDecryptInvalid) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  std::string ciphertext("invalidciphertext");
  std::string returned_plaintext;
  DWORD last_error = 0;
  std::optional<std::string> maybe_new_ciphertext;
  const HRESULT hr =
      DecryptAppBoundString(ciphertext, returned_plaintext,
                            ProtectionLevel::PROTECTION_PATH_VALIDATION,
                            maybe_new_ciphertext, last_error);
  EXPECT_FALSE(maybe_new_ciphertext);
  EXPECT_EQ(elevation_service::Elevator::kErrorCouldNotDecryptWithSystemContext,
            hr);
}

// These tests verify that the metrics are recorded correctly. The first load of
// browser in the PRE_ test stores the "Test Key" with App-Bound encryption and
// the second stage of the test verifies it can be retrieved successfully.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, PRE_MetricsTest) {
  histogram_tester_.ExpectUniqueSample(
      "OSCrypt.AppBoundEncryption.SupportLevel", SupportLevel::kSupported, 1);

  // These histograms are recorded on a background worker thread, so the test
  // needs to wait until this task completes and the histograms are recorded.
  WaitForHistogram("OSCrypt.AppBoundProvider.Encrypt.ResultCode");
  histogram_tester_.ExpectBucketCount(
      "OSCrypt.AppBoundProvider.Encrypt.ResultCode", S_OK, 1);
}

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, MetricsTest) {
  ASSERT_TRUE(install_static::IsSystemInstall());

  // These histograms are recorded on a background worker thread, so the test
  // needs to wait until this task completes and the histograms are recorded.
  WaitForHistogram("OSCrypt.AppBoundProvider.Decrypt.ResultCode");
  histogram_tester_.ExpectBucketCount(
      "OSCrypt.AppBoundProvider.Decrypt.ResultCode", S_OK, 1);
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
class AppBoundEncryptionWinTestWithPolicyBase
    : public AppBoundEncryptionWinTest {
 protected:
  void MaybeEnablePolicy(std::optional<bool> policy_state) {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PolicyMap values;
    if (policy_state.has_value()) {
      values.Set(policy::key::kApplicationBoundEncryptionEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(*policy_state),
                 nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

class AppBoundEncryptionWinTestWithPolicy
    : public AppBoundEncryptionWinTestWithPolicyBase,
      public ::testing::WithParamInterface<
          /*policy::key::kApplicationBoundEncryptionEnabled=*/std::optional<
              bool>> {
 private:
  void SetUp() override {
    MaybeEnablePolicy(GetParam());
    AppBoundEncryptionWinTestWithPolicyBase::SetUp();
  }
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

class AppBoundEncryptionWinTestWithVariablePolicy
    : public AppBoundEncryptionWinTestWithPolicyBase {
 protected:
  void StoreData(base::span<const uint8_t> data) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const auto data_path =
        browser()->profile()->GetPath().Append(FILE_PATH_LITERAL("TestData"));
    ASSERT_FALSE(base::PathExists(data_path));
    EXPECT_TRUE(base::WriteFile(data_path, data));
  }

  std::optional<std::vector<uint8_t>> RetrieveData() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::ReadFileToBytes(
        browser()->profile()->GetPath().Append(FILE_PATH_LITERAL("TestData")));
  }

  static std::vector<uint8_t> GetEncryptedData(
      base::span<const uint8_t> expected) {
    base::RunLoop run_loop;
    std::vector<uint8_t> result;
    auto sub = g_browser_process->os_crypt_async()->GetInstance(
        base::BindLambdaForTesting(
            [&](os_crypt_async::Encryptor encryptor, bool success) {
              ASSERT_TRUE(success);
              result = encryptor.EncryptString("secret").value();
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

 private:
  static bool IsPreTest() {
    const std::string_view test_name(
        ::testing::UnitTest::GetInstance()->current_test_info()->name());
    return test_name.find("PRE_") != std::string_view::npos;
  }

  void SetUp() override {
    if (!IsPreTest()) {
      // Disable App-Bound in the second part of the test.
      MaybeEnablePolicy(false);
    }
    AppBoundEncryptionWinTestWithPolicyBase::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestWithVariablePolicy,
                       PRE_EncryptionDisabled) {
  EXPECT_EQ(GetAppBoundEncryptionSupportLevel(g_browser_process->local_state()),
            SupportLevel::kSupported);

  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());

  const auto app_bound_data = encryptor.EncryptString("app-bound secret");
  ASSERT_TRUE(app_bound_data);
  ASSERT_GT(app_bound_data->size(), 3u);
  // kAppBoundDataPrefix for App-Bound.
  constexpr uint8_t kV20Header[] = {'v', '2', '0'};
  EXPECT_THAT(base::span(*app_bound_data).first<3>(),
              ::testing::ElementsAreArray(kV20Header));
  ASSERT_NO_FATAL_FAILURE(StoreData(*app_bound_data));
}

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestWithVariablePolicy,
                       EncryptionDisabled) {
  EXPECT_EQ(GetAppBoundEncryptionSupportLevel(g_browser_process->local_state()),
            SupportLevel::kDisabledByPolicy);
  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());

  const auto data = encryptor.EncryptString("secret");
  ASSERT_TRUE(data);
  ASSERT_GT(data->size(), 3u);
  // kEncryptionVersionPrefix for DPAPI i.e. not App-Bound.
  constexpr uint8_t kV10Header[] = {'v', '1', '0'};
  EXPECT_THAT(base::span(*data).first<3>(),
              ::testing::ElementsAreArray(kV10Header));

  // Also decrypt the data that was previously encrypted in the PRE test, and
  // verify it decrypts even if App-Bound is disabled by policy. This is also
  // tested elsewhere.
  const auto previous_data = RetrieveData();
  ASSERT_TRUE(previous_data);
  os_crypt_async::Encryptor::DecryptFlags flags;
  const auto plaintext = encryptor.DecryptData(*previous_data, &flags);
  ASSERT_TRUE(plaintext);
  // App-Bound is now disabled, so App-Bound encrypted data should be
  // re-encrypted in order to ensure it's encrypted with the DPAPI key provider.
  EXPECT_TRUE(flags.should_reencrypt);
  EXPECT_EQ(*plaintext, "app-bound secret");
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

class AppBoundEncryptionWinReencryptTest
    : public AppBoundEncryptionWinTest,
      public ::testing::WithParamInterface<
          std::tuple</*fake_reencrypt*/ bool, /*enable_feature*/ bool>> {
 public:
  AppBoundEncryptionWinReencryptTest() {
    feature_list_.InitWithFeatureState(features::kAppBoundDataReencrypt,
                                       std::get<1>(GetParam()));
  }

 protected:
  // Re-encrypt should only happen if both the feature is enabled, and the
  // service is faking the re-encryption signal.
  static bool ExpectReencrypt() {
    return std::get<0>(GetParam()) && std::get<1>(GetParam());
  }
  void SetUp() override {
    if (base::GetCurrentProcessIntegrityLevel() != base::HIGH_INTEGRITY) {
      GTEST_SKIP() << "Elevation is required for this test.";
    }
    maybe_uninstall_service_ =
        InstallService(log_grabber_, std::get<0>(GetParam()));
    EXPECT_TRUE(maybe_uninstall_service_.has_value());
    // Note do not call SetUp from AppBoundEncryptionWinTest, call to
    // InProcessBrowserTest.
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test the basic interface to Encrypt and Decrypt data.
IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinReencryptTest, EncryptDecrypt) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  const std::string plaintext("plaintext");
  std::string ciphertext;
  DWORD last_error;
  base::HistogramTester histograms;
  HRESULT hr =
      EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                            plaintext, ciphertext, last_error);

  ASSERT_HRESULT_SUCCEEDED(hr);

  std::string returned_plaintext;
  std::optional<std::string> maybe_new_ciphertext;
  hr = DecryptAppBoundString(ciphertext, returned_plaintext,
                             ProtectionLevel::PROTECTION_PATH_VALIDATION,
                             maybe_new_ciphertext, last_error);
  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(plaintext, returned_plaintext);

  if (ExpectReencrypt()) {
    histograms.ExpectUniqueSample("OSCrypt.AppBound.ReEncrypt.ResultCode", S_OK,
                                  1u);
    ASSERT_TRUE(maybe_new_ciphertext);

    std::optional<std::string> even_newer_ciphertext;
    // Verify that the new replacement ciphertext returned can still be
    // decrypted.
    hr = DecryptAppBoundString(*maybe_new_ciphertext, returned_plaintext,
                               ProtectionLevel::PROTECTION_PATH_VALIDATION,
                               even_newer_ciphertext, last_error);
    ASSERT_HRESULT_SUCCEEDED(hr);
    EXPECT_EQ(plaintext, returned_plaintext);
  } else {
    histograms.ExpectTotalCount("OSCrypt.AppBound.ReEncrypt.ResultCode", 0);
    ASSERT_FALSE(maybe_new_ciphertext);
  }
  histograms.ExpectTotalCount("OSCrypt.AppBound.ReEncrypt.ResultLastError", 0);
}

// This could be a unit test, but it needs the service installed to work, so
// makes sense for it to be here alongside the other app-bound encryption tests.
IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinReencryptTest, KeyProviderTest) {
  const char* kPrefName = "os_crypt.app_bound_encrypted_key";
  ASSERT_TRUE(install_static::IsSystemInstall());

  TestingPrefServiceSimple prefs;
  MockPrefChangeCallback observer(&prefs);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(kPrefName, observer.GetCallback());
  // The first time the GetKey is called, the provider should generate a random
  // key, encrypt it with app-bound, then persist the encrypted key to store.
  EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(1);

  os_crypt_async::AppBoundEncryptionProviderWin::RegisterLocalPrefs(
      prefs.registry());

  // `Key` has no public constructor and is move-only so use a std::optional as
  // a handy container.
  std::optional<os_crypt_async::Encryptor::Key> encryption_key;
  std::string encrypted_key;
  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(
        &prefs, /*use_for_encryption=*/true);
    base::test::TestFuture<const std::string&,
                           std::optional<os_crypt_async::Encryptor::Key>>
        future;
    provider.GetKey(future.GetCallback());
    auto [tag, key] = future.Take();
    EXPECT_EQ(tag, "v20");
    ASSERT_TRUE(key);
    encryption_key.emplace(std::move(*key));
    encrypted_key = prefs.GetString(kPrefName);
    EXPECT_FALSE(encrypted_key.empty());
  }
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // The second time the GetKey is called, the provider should retrieve the key
  // from store then perform a decryption via app-bound. If re-encryption is
  // specified then a re-encryption call is made and a second write should
  // happen to the store with the new encrypted key.
  EXPECT_CALL(observer, OnPreferenceChanged(_))
      .Times(ExpectReencrypt() ? 1 : 0);
  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(
        &prefs, /*use_for_encryption=*/true);
    base::test::TestFuture<const std::string&,
                           std::optional<os_crypt_async::Encryptor::Key>>
        future;
    provider.GetKey(future.GetCallback());
    const auto& [_, key] = future.Get();
    ASSERT_TRUE(key);
    // The key returned should be the same as it's been decrypted from the
    // store, regardless of whether it's been re-encrypted or not.
    EXPECT_EQ(*key, *encryption_key);

    if (ExpectReencrypt()) {
      // Re-encryption should always change the encrypted value, because the
      // underlying encryption schemes use random IVs, nonces or salts.
      EXPECT_NE(prefs.GetString(kPrefName), encrypted_key);
    } else {
      EXPECT_EQ(prefs.GetString(kPrefName), encrypted_key);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AppBoundEncryptionWinReencryptTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()),
    [](const auto& info) {
      return base::StrCat(
          {std::get<0>(info.param) ? "FakeReencrypt" : "NoFakeReencrypt",
           std::get<1>(info.param) ? "FeatureOn" : "FeatureOff"});
    });

class AppBoundEncryptionWinTestFeatureMaybeDisabled
    : public AppBoundEncryptionWinTest,
      public ::testing::WithParamInterface</*feature enabled*/ bool> {
 public:
  AppBoundEncryptionWinTestFeatureMaybeDisabled() {
    feature_list_.InitWithFeatureState(
        ::features::kUseAppBoundEncryptionProviderForEncryption, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinTestFeatureMaybeDisabled,
                       ReEncrypt) {
  std::string ciphertext;
  ASSERT_TRUE(OSCrypt::EncryptString("secrets", &ciphertext));

  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());

  os_crypt_async::Encryptor::DecryptFlags flags;
  std::string plaintext;
  ASSERT_TRUE(encryptor.DecryptString(ciphertext, &plaintext, &flags));
  // With App-Bound enabled, a decryption of an OSCrypt Sync secret should
  // result in a request to re-encrypt to get full protection, otherwise no
  // re-encryption should be requested.
  EXPECT_EQ(flags.should_reencrypt, GetParam());
}

INSTANTIATE_TEST_SUITE_P(,
                         AppBoundEncryptionWinTestFeatureMaybeDisabled,
                         ::testing::Bool(),
                         [](auto& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

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

    // The binary must run from 'testdir' this is because otherwise the scoped
    // temp dir ends with a `scoped_dir` path which conflicts with a production
    // environment that path validation has to correctly cater for.
    auto executable_file_dir = temp_dir_.GetPath().Append(L"testdir");
    if (sub_dir) {
      executable_file_dir = executable_file_dir.Append(*sub_dir);
    }
    base::CreateDirectory(executable_file_dir);

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
