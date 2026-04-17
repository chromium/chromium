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
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/browser/os_crypt/test_support.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/win/isolated_browser_support.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/windows_services/service_program/test_support/service_environment.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace os_crypt {

namespace {

// Useful in tests, to compare bytewise with data.
constexpr uint8_t kV20Header[] = {
    static_cast<uint8_t>(os_crypt_async::kAppBoundDataPrefix[0]),
    static_cast<uint8_t>(os_crypt_async::kAppBoundDataPrefix[1]),
    static_cast<uint8_t>(os_crypt_async::kAppBoundDataPrefix[2]),
};

void WaitForHistogram(const std::string& histogram_name) {
  // Continue if histogram was already recorded.
  if (base::StatisticsRecorder::FindHistogram(histogram_name)) {
    return;
  }

  // Else, wait until the histogram is recorded.
  base::RunLoop run_loop;
  auto histogram_observer =
      std::make_unique<base::StatisticsRecorder::ScopedHistogramSampleObserver>(
          histogram_name, run_loop.QuitClosure());
  run_loop.Run();
}

os_crypt_async::Encryptor GetInstanceSync(
    os_crypt_async::OSCryptAsync& factory,
    os_crypt_async::Encryptor::Option option =
        os_crypt_async::Encryptor::Option::kNone) {
  base::test::TestFuture<os_crypt_async::Encryptor> future;
  factory.GetInstance(future.GetCallback(), option);
  return future.Take();
}

}  // namespace

class AppBoundEncryptionWinTestBase : public InProcessBrowserTest {
 public:
  explicit AppBoundEncryptionWinTestBase(bool use_old_elevator_interface)
      : scoped_install_details_(
            std::make_unique<FakeInstallDetails>(use_old_elevator_interface)) {}

 protected:
  void SetUp() override {
    if (base::GetCurrentProcessIntegrityLevel() != base::HIGH_INTEGRITY) {
      GTEST_SKIP() << "Elevation is required for this test.";
    }
#if defined(ARCH_CPU_32_BITS)
    // Flaky on 32-bit win-rel-ready bot. See crbug.com/430106357.
    GTEST_SKIP() << "Temporarily disabled on 32-bit. See crbug.com/430106357.";
#else
    if (should_install_service_) {
      maybe_service_environment_.emplace(
          install_static::GetElevationServiceName(),
          installer::kElevationServiceExe,
          std::array<std::string_view, 3>{
              elevation_service::switches::kAllowUntrustedPathForTesting,
              elevation_service::switches::kElevatorClsIdForTestingSwitch,
              elevation_service::switches::kAllowUntrustedSwitchesForTesting},
          install_static::GetElevatorClsid(), install_static::GetElevatorIid());
    }
    // Browser tests use a custom user data dir, which would normally result in
    // App-Bound encryption being disabled with
    // `SupportLevel::kNotUsingDefaultUserDataDir`, so this call forces the
    // non-standard testing data dir to be considered a default one, except if
    // a test is explicitly requesting to use a non-standard one see
    // `AppBoundEncryptionWinTestWithUserDataDir`.
    chrome::SetUsingDefaultUserDataDirectoryForTesting(
        set_default_user_data_dir_);
    InProcessBrowserTest::SetUp();
#endif  // defined(ARCH_CPU_32_BITS)
  }

  void TearDown() override { maybe_service_environment_.reset(); }

  // Used by multi-stage tests to persist data between each part of the test.
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

  static bool IsPreTest() {
    const std::string_view test_name(
        ::testing::UnitTest::GetInstance()->current_test_info()->name());
    return test_name.find("PRE_") != std::string_view::npos;
  }

  base::HistogramTester histogram_tester_;
  std::optional<ServiceEnvironment> maybe_service_environment_;
  bool set_default_user_data_dir_ = true;
  bool should_install_service_ = true;

 private:
  install_static::ScopedInstallDetails scoped_install_details_;
};

// App-Bound tests are expensive to run so only run the basic EncryptDecrypt
// test with both elevator interface variants.
class AppBoundEncryptionWinTest : public AppBoundEncryptionWinTestBase {
 public:
  AppBoundEncryptionWinTest()
      : AppBoundEncryptionWinTestBase(/*use_old_elevator_interface=*/false) {}
};

// Test App-Bound is supported for tests.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, Supported) {
  EXPECT_EQ(SupportLevel::kSupported, GetAppBoundEncryptionSupportLevel(
                                          g_browser_process->local_state()));
}

class AppBoundEncryptionWinTestWithLegacy
    : public AppBoundEncryptionWinTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  AppBoundEncryptionWinTestWithLegacy()
      : AppBoundEncryptionWinTestBase(GetParam()) {}
};

// Test the basic interface to Encrypt and Decrypt data.
IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinTestWithLegacy, EncryptDecrypt) {
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

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         AppBoundEncryptionWinTestWithLegacy,
                         ::testing::Bool(),
                         [](const auto& info) {
                           return info.param ? "IElevator" : "IElevator2";
                         });

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

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestNoService, NoService) {
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

class AppBoundEncryptionWinDecryptionNotAvailableTest
    : public AppBoundEncryptionWinTest {
  void SetUp() override {
    // Install the service only for the pre-test part.
    should_install_service_ = IsPreTest();
    AppBoundEncryptionWinTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinDecryptionNotAvailableTest,
                       PRE_DecryptionTemporaryFailure) {
  EXPECT_EQ(GetAppBoundEncryptionSupportLevel(g_browser_process->local_state()),
            SupportLevel::kSupported);
  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());

  const auto app_bound_data = encryptor.EncryptString("app-bound secret");
  ASSERT_TRUE(app_bound_data);
  ASSERT_GT(app_bound_data->size(), 3u);
  EXPECT_THAT(base::span(*app_bound_data).first<3>(),
              ::testing::ElementsAreArray(kV20Header));
  ASSERT_NO_FATAL_FAILURE(StoreData(*app_bound_data));
}

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinDecryptionNotAvailableTest,
                       DecryptionTemporaryFailure) {
  // Supported, but the service is not present. Decryptions should fail.
  EXPECT_EQ(GetAppBoundEncryptionSupportLevel(g_browser_process->local_state()),
            SupportLevel::kSupported);
  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());

  {
    const auto previous_data = RetrieveData();
    ASSERT_TRUE(previous_data);

    os_crypt_async::Encryptor::DecryptFlags flags;
    const auto plaintext = encryptor.DecryptData(*previous_data, &flags);
    EXPECT_FALSE(plaintext);
    // Decryption is temporarily unavailable, as the service is not present.
    EXPECT_TRUE(flags.temporarily_unavailable);
  }

  {
    os_crypt_async::Encryptor::DecryptFlags flags;
    auto plaintext =
        encryptor.DecryptData(base::as_byte_span("invalid_data"), &flags);
    EXPECT_FALSE(plaintext);
    // Invalid data is permanently unavailable.
    EXPECT_FALSE(flags.temporarily_unavailable);
  }
}

class AppBoundEncryptionWinTestWithVariablePolicy
    : public AppBoundEncryptionWinTestWithPolicyBase {
 private:
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
    std::vector<std::string_view> switches = {
        elevation_service::switches::kElevatorClsIdForTestingSwitch};
    if (std::get<0>(GetParam())) {
      switches.push_back(
          elevation_service::switches::kFakeReencryptForTestingSwitch);
    }
    maybe_service_environment_.emplace(
        install_static::GetElevationServiceName(),
        installer::kElevationServiceExe, switches,
        install_static::GetElevatorClsid(), install_static::GetElevatorIid());
    // Service already installed, do not try installing again.
    should_install_service_ = false;
    AppBoundEncryptionWinTest::SetUp();
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
  elevation_service::EncryptFlags flags;
  HRESULT hr =
      EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                            plaintext, ciphertext, last_error, &flags);

  ASSERT_HRESULT_SUCCEEDED(hr);

  std::string returned_plaintext;
  std::optional<std::string> maybe_new_ciphertext;
  hr = DecryptAppBoundString(ciphertext, returned_plaintext,
                             ProtectionLevel::PROTECTION_PATH_VALIDATION,
                             maybe_new_ciphertext, last_error);
  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(plaintext, returned_plaintext);

  if (ExpectReencrypt()) {
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
    ASSERT_FALSE(maybe_new_ciphertext);
  }
}

// This could be a unit test, but it needs the service installed to work, so
// makes sense for it to be here alongside the other app-bound encryption tests.
IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinReencryptTest, KeyProviderTest) {
  ASSERT_TRUE(install_static::IsSystemInstall());

  TestingPrefServiceSimple prefs;
  MockPrefChangeCallback observer(&prefs);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs);
  registrar.Add(os_crypt_async::kAppBoundEncryptedKeyPrefName,
                observer.GetCallback());
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
        &prefs, /*force_protection_level=*/std::nullopt);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    auto [tag, key] = future.Take();
    EXPECT_EQ(tag, os_crypt_async::kAppBoundDataPrefix);
    ASSERT_TRUE(key.has_value());
    encryption_key.emplace(*std::move(key));
    encrypted_key =
        prefs.GetString(os_crypt_async::kAppBoundEncryptedKeyPrefName);
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
        &prefs, /*force_protection_level=*/std::nullopt);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    const auto& [_, key] = future.Get();
    ASSERT_TRUE(key.has_value());
    // The key returned should be the same as it's been decrypted from the
    // store, regardless of whether it's been re-encrypted or not.
    EXPECT_EQ(*key, *encryption_key);

    if (ExpectReencrypt()) {
      // Re-encryption should always change the encrypted value, because the
      // underlying encryption schemes use random IVs, nonces or salts.
      EXPECT_NE(prefs.GetString(os_crypt_async::kAppBoundEncryptedKeyPrefName),
                encrypted_key);
      // Verify the encrypted key pref (base64, with the header "APPB") is long
      // enough to be a valid encrypted key, and not just empty or truncated. A
      // truncated key will be 'QVBQQg==' which is base64 for 'APPB'.
      EXPECT_GT(prefs.GetString(os_crypt_async::kAppBoundEncryptedKeyPrefName)
                    .length(),
                10u);
    } else {
      EXPECT_EQ(prefs.GetString(os_crypt_async::kAppBoundEncryptedKeyPrefName),
                encrypted_key);
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

class AppBoundEncryptionWinTestForceReencrypt
    : public AppBoundEncryptionWinTest {
 private:
  base::test::ScopedFeatureList feature_list_{features::kAppBoundDataReencrypt};
};

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestForceReencrypt,
                       ForceReencrypt) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  TestingPrefServiceSimple prefs;
  os_crypt_async::AppBoundEncryptionProviderWin::RegisterLocalPrefs(
      prefs.registry());

  std::optional<os_crypt_async::Encryptor::Key> encryption_key;
  std::string encrypted_key;
  {
    // Use default protection level.
    os_crypt_async::AppBoundEncryptionProviderWin provider(
        &prefs, /*force_protection_level=*/std::nullopt);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    auto [tag, key] = future.Take();
    EXPECT_EQ(tag, os_crypt_async::kAppBoundDataPrefix);
    ASSERT_TRUE(key.has_value());
    encryption_key.emplace(*std::move(key));
    encrypted_key =
        prefs.GetString(os_crypt_async::kAppBoundEncryptedKeyPrefName);
    EXPECT_THAT(encrypted_key, Not(::testing::IsEmpty()));
    EXPECT_FALSE(encrypted_key.empty());
  }

  {
    os_crypt_async::AppBoundEncryptionProviderWin provider(
        &prefs, ProtectionLevel::PROTECTION_NONE);
    base::test::TestFuture<
        const std::string&,
        base::expected<os_crypt_async::Encryptor::Key,
                       os_crypt_async::KeyProvider::KeyError>>
        future;
    provider.GetKey(future.GetCallback());
    const auto& [_, key] = future.Get();
    // The key returned should be the same as it's been decrypted from the
    // store, regardless of whether it's been re-encrypted or not.
    EXPECT_THAT(key,
                base::test::ValueIs(::testing::Eq(std::ref(*encryption_key))));
    // Because the encrypted_key is encrypted, it's not possible to simply
    // inspect the data - but PROTECTION_NONE contains less data than real
    // protection which encodes a path, so should always be shorter.
    EXPECT_LT(
        prefs.GetString(os_crypt_async::kAppBoundEncryptedKeyPrefName).length(),
        encrypted_key.size());
  }
}

// These tests do not function correctly in component builds because they rely
// on being able to run a standalone executable child process in various
// different directories, and a component build has too many dynamic DLL
// dependencies to conveniently move around the file system hermetically.
#if !defined(COMPONENT_BUILD)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr bool kExpectFullIsolation = true;
#else
inline constexpr bool kExpectFullIsolation = false;
#endif

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
      base::FilePath::StringViewType filename,
      std::optional<base::FilePath::StringViewType> sub_dir,
      const std::string& input_data,
      std::string& output_data,
      Operation op,
      ProtectionLevel level,
      bool launch_isolated,
      bool expect_reencrypt,
      HRESULT& result) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    const auto input_file_path = temp_dir_.GetPath().Append(L"input-file");
    const auto output_file_path = temp_dir_.GetPath().Append(L"output-file");
    const auto reencrypt_file_path =
        temp_dir_.GetPath().Append(L"reencrypt-file");
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
    std::ignore = base::DeleteFile(output_file_path);
    std::ignore = base::DeleteFile(reencrypt_file_path);

    const auto orig_exe = base::PathService::CheckedGet(base::DIR_EXE)
                              .Append(FILE_PATH_LITERAL("app_binary.exe"));
    bool success = false;

    // Copy file triggers AV sometimes. Implement a very basic retry loop here
    // to avoid tests flaking.
    for (size_t i = 0; i < 5; ++i) {
      success = base::CopyFile(orig_exe, executable_file_path);
      if (success) {
        break;
      }
      base::PlatformThread::Sleep(base::Milliseconds(100));
    }

    ASSERT_TRUE(success);
    base::CommandLine cmd(executable_file_path);

    cmd.AppendSwitchPath(switches::kAppBoundTestInputFilename, input_file_path);
    cmd.AppendSwitchPath(switches::kAppBoundTestOutputFilename,
                         output_file_path);
    cmd.AppendSwitchPath(switches::kAppBoundTestReencryptionOutputFilename,
                         reencrypt_file_path);
    cmd.AppendSwitchASCII(switches::kAppBoundTestProtectionLevel,
                          base::NumberToString(level));

    switch (op) {
      case Operation::kEncrypt:
        cmd.AppendSwitch(switches::kAppBoundTestModeEncrypt);
        break;
      case Operation::kDecrypt:
        cmd.AppendSwitch(switches::kAppBoundTestModeDecrypt);
        break;
    }

    base::Process process;
    if (launch_isolated) {
      ASSERT_OK_AND_ASSIGN(process, chrome::LaunchIsolatedBrowser(cmd));
    } else {
      base::LaunchOptions options;
      options.start_hidden = true;
      options.wait = true;

      process = base::LaunchProcess(cmd, options);
    }
    ASSERT_TRUE(process.IsValid());
    int exit_code;
    EXPECT_TRUE(process.WaitForExit(&exit_code));
    result = static_cast<HRESULT>(exit_code);
    if (SUCCEEDED(result)) {
      EXPECT_TRUE(base::ReadFileToString(output_file_path, &output_data));
    }
    if (op == Operation::kDecrypt) {
      EXPECT_EQ(base::PathExists(reencrypt_file_path), expect_reencrypt);
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
  static constexpr char kSecret[] = "secret";
  {
    std::string ciphertext;
    HRESULT result;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, kSecret, ciphertext, Operation::kEncrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app2.exe", {}, ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", L"Application", ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", L"Temp", ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);

    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", L"Bad", ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(elevation_service::Elevator::kValidationDidNotPass, result);
  }
  {
    // Explicitly test the most frequent chrome-specific cases.
    std::string ciphertext;
    HRESULT result;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"chrome.exe", L"Application", kSecret, ciphertext, Operation::kEncrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"new_chrome.exe", L"Application", ciphertext, plaintext,
        Operation::kDecrypt, ProtectionLevel::PROTECTION_PATH_VALIDATION,
        /*launch_isolated=*/false, /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"old_chrome.exe", L"Temp", ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
        /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    EXPECT_EQ(kSecret, plaintext);
  }
}

// Launching an isolated process that's compiled with ASAN does not work right
// now. See https://crbug.com/492374385.
#if !defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestMultiProcess,
                       EncryptDecryptIsolatedProcess) {
  static constexpr char kSecret[] = "secret";
  {
    std::string ciphertext;
    HRESULT result;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, kSecret, ciphertext, Operation::kEncrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
        /*launch_isolated=*/false, /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    std::string plaintext;
    // Isolated process can always read previously unisolated encrypted data. If
    // isolation is supported, the elevated service will return a hint to
    // re-encrypt weak data.
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
        /*launch_isolated=*/true, /*expect_reencrypt=*/kExpectFullIsolation,
        result));
    EXPECT_HRESULT_SUCCEEDED(result);
    EXPECT_EQ(kSecret, plaintext);
  }
  {
    std::string ciphertext;
    HRESULT result;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, kSecret, ciphertext, Operation::kEncrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
        /*launch_isolated=*/true, /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
        /*launch_isolated=*/true, /*expect_reencrypt=*/false, result));
    // Isolated process can always read previously isolated encrypted data.
    EXPECT_HRESULT_SUCCEEDED(result);
    EXPECT_EQ(kSecret, plaintext);
  }
  {
    std::string ciphertext;
    HRESULT result;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, kSecret, ciphertext, Operation::kEncrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
        /*launch_isolated=*/true, /*expect_reencrypt=*/false, result));
    EXPECT_EQ(S_OK, result);
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", {}, ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
        /*launch_isolated=*/false, /*expect_reencrypt=*/false, result));
    if constexpr (kExpectFullIsolation) {
      // Only builds with true isolated processes validate the isolation state.
      EXPECT_EQ(elevation_service::Elevator::kIsolationStateInvalid, result);
    } else {
      EXPECT_HRESULT_SUCCEEDED(result);
      EXPECT_EQ(kSecret, plaintext);
    }
  }
}
#endif  // !defined(ADDRESS_SANITIZER)

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestMultiProcess,
                       DowngradeProtectionLevel) {
  static constexpr char kSecret[] = "secret";
  // App-Bound test App will only encrypt/decrypt data with the TESTDATAHEADER
  // on it. See comments in app_bound_encryption_test_main.cc.
  static constexpr char kSecretForTestApp[] = "TESTDATAHEADERsecret";

  // Create some ciphertext and bind it to the main test process.
  std::string ciphertext;
  {
    DWORD last_error;

    HRESULT result =
        EncryptAppBoundString(ProtectionLevel::PROTECTION_PATH_VALIDATION,
                              kSecretForTestApp, ciphertext, last_error);
    EXPECT_HRESULT_SUCCEEDED(result);
  }

  // Fail to decrypt this data from a path that does not match the path
  // validation.
  {
    HRESULT result;
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", L"Bad", ciphertext, plaintext, Operation::kDecrypt,
        ProtectionLevel::PROTECTION_PATH_VALIDATION,
        /*launch_isolated=*/false, /*expect_reencrypt=*/false, result));
    // This should fail due to a path validation error.
    EXPECT_EQ(elevation_service::Elevator::kValidationDidNotPass, result);
  }

  // Now take this ciphertext and downgrade it to PROTECTION_NONE.
  std::optional<std::string> new_ciphertext;
  {
    std::string plaintext;
    DWORD last_error;
    elevation_service::EncryptFlags flags{.force_reencrypt = true};
    HRESULT result = DecryptAppBoundString(ciphertext, plaintext,
                                           ProtectionLevel::PROTECTION_NONE,
                                           new_ciphertext, last_error, &flags);
    EXPECT_HRESULT_SUCCEEDED(result);
    EXPECT_TRUE(new_ciphertext.has_value());
    EXPECT_EQ(kSecretForTestApp, plaintext);
  }

  // Since validation is PROTECTION_NONE the decrypt should now pass from any
  // process.
  {
    HRESULT result;
    std::string plaintext;
    ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
        L"app1.exe", L"Bad", new_ciphertext.value(), plaintext,
        Operation::kDecrypt, ProtectionLevel::PROTECTION_NONE,
        /*launch_isolated=*/false, /*expect_reencrypt=*/false, result));
    // Should now pass.
    EXPECT_HRESULT_SUCCEEDED(result);
    EXPECT_EQ(kSecret, plaintext);
  }
}

// Launching an isolated process that's compiled with ASAN does not work right
// now. See https://crbug.com/492374385.
#if !defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestMultiProcess,
                       UpgradeUnIsolatedData) {
  static constexpr char kSecret[] = "secret";
  std::string ciphertext;
  HRESULT result;
  ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
      L"app1.exe", {}, kSecret, ciphertext, Operation::kEncrypt,
      ProtectionLevel::PROTECTION_PATH_VALIDATION, /*launch_isolated=*/false,
      /*expect_reencrypt=*/false, result));
  EXPECT_EQ(S_OK, result);
  std::string plaintext;
  // Re-encrypt is requested if an isolated process decrypts data previously
  // encrypted with PROTECTION_PATH_VALIDATION.
  ASSERT_NO_FATAL_FAILURE(EncryptOrDecryptInTestProcess(
      L"app1.exe", {}, ciphertext, plaintext, Operation::kDecrypt,
      ProtectionLevel::PROTECTION_PATH_VALIDATION_WITH_ISOLATION,
      /*launch_isolated=*/true, /*expect_reencrypt=*/kExpectFullIsolation,
      result));
  EXPECT_HRESULT_SUCCEEDED(result);
  EXPECT_EQ(kSecret, plaintext);
}
#endif  // !defined(ADDRESS_SANITIZER)
#endif  // !defined(COMPONENT_BUILD)

struct AppBoundTestCase {
  // Test case name.
  std::string name;
  // If true, then the temporary dir used by tests for --user-data-dir is
  // considered a 'default user data dir' by the PRE part of the test.
  bool allow_non_standard_udd_in_pre;
  // If true, then the temporary dir used by tests for --user-data-dir is
  // considered a 'default user data dir' by the main part of the test.
  bool allow_non_standard_udd_in_main;
  // Whether or not the data will start with the 'v20' app-bound header,
  // indicating that it's secured with App-Bound encryption.
  bool expect_encrypt_with_app_bound;
  // Whether or not the Decrypt operation succeeds. If false, then the
  // `temporarily_unavailable` is also expected to be true.
  bool expect_decrypt_works;
  // If decrypt works, whether or not OSCrypt indicates that the data should be
  // re-encrypted.
  bool should_reencrypt = false;
  // Support level for the PRE part of the test.
  SupportLevel expected_support_level_in_pre;
  // Support level for the main part of the test.
  SupportLevel expected_support_level_in_main;
};

class AppBoundEncryptionWinTestWithUserDataDir
    : public AppBoundEncryptionWinTest,
      public testing::WithParamInterface<AppBoundTestCase> {
 public:
  void SetUp() override {
    set_default_user_data_dir_ =
        IsPreTest() ? GetParam().allow_non_standard_udd_in_pre
                    : GetParam().allow_non_standard_udd_in_main;
    AppBoundEncryptionWinTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinTestWithUserDataDir,
                       PRE_EncryptionDecryption) {
  EXPECT_EQ(GetAppBoundEncryptionSupportLevel(g_browser_process->local_state()),
            GetParam().expected_support_level_in_pre);

  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());

  const auto encrypted_data = encryptor.EncryptString("super secret");
  ASSERT_TRUE(encrypted_data);
  if (GetParam().expect_encrypt_with_app_bound) {
    ASSERT_GT(encrypted_data->size(), 3u);
    EXPECT_THAT(base::span(*encrypted_data).first<3>(),
                ::testing::ElementsAreArray(kV20Header));
  }
  ASSERT_NO_FATAL_FAILURE(StoreData(*encrypted_data));
}

IN_PROC_BROWSER_TEST_P(AppBoundEncryptionWinTestWithUserDataDir,
                       EncryptionDecryption) {
  EXPECT_EQ(GetAppBoundEncryptionSupportLevel(g_browser_process->local_state()),
            GetParam().expected_support_level_in_main);

  auto encryptor = GetInstanceSync(*g_browser_process->os_crypt_async());

  const auto previous_data = RetrieveData();
  ASSERT_TRUE(previous_data);

  os_crypt_async::Encryptor::DecryptFlags flags;
  const auto plaintext = encryptor.DecryptData(*previous_data, &flags);
  if (GetParam().expect_decrypt_works) {
    ASSERT_TRUE(plaintext);
    EXPECT_EQ("super secret", *plaintext);
    EXPECT_EQ(flags.should_reencrypt, GetParam().should_reencrypt);
  } else {
    EXPECT_FALSE(plaintext);
    EXPECT_TRUE(flags.temporarily_unavailable);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AppBoundEncryptionWinTestWithUserDataDir,
    testing::ValuesIn<AppBoundTestCase>({
        {.name = "standard_udd",
         .allow_non_standard_udd_in_pre = true,
         .allow_non_standard_udd_in_main = true,
         .expect_encrypt_with_app_bound = true,
         .expect_decrypt_works = true,
         .should_reencrypt = false,
         .expected_support_level_in_pre = SupportLevel::kSupported,
         .expected_support_level_in_main = SupportLevel::kSupported},
        {.name = "non_standard_udd",
         .allow_non_standard_udd_in_pre = false,
         .allow_non_standard_udd_in_main = false,
         .expect_encrypt_with_app_bound = false,
         .expect_decrypt_works = true,
         .should_reencrypt = false,
         .expected_support_level_in_pre =
             SupportLevel::kNotUsingDefaultUserDataDir,
         .expected_support_level_in_main =
             SupportLevel::kNotUsingDefaultUserDataDir},
        // Switch from a standard UDD to a non-standard UDD. The encrypt that
        // took place in the first stage was app-bound and should not be
        // decryptable by the second stage.
        {.name = "was_standard_udd_now_non_standard_udd",
         .allow_non_standard_udd_in_pre = true,
         .allow_non_standard_udd_in_main = false,
         .expect_encrypt_with_app_bound = true,
         // This is the only configuration where decryption should fail.
         .expect_decrypt_works = false,
         .expected_support_level_in_pre = SupportLevel::kSupported,
         .expected_support_level_in_main =
             SupportLevel::kNotUsingDefaultUserDataDir},
        // Switch from a non-standard UDD to a standard UDD. The encrypt that
        // took place in the first stage would have been using non-app-bound
        // since it did not provide a key, but should still be decryptable with
        // the other Key Provider(s) (e.g. DPAPI). Because DPAPI is weaker than
        // App-Bound, OSCrypt indicates `should_reencrypt` as true.
        {.name = "was_non_standard_udd_now_standard_udd",
         .allow_non_standard_udd_in_pre = false,
         .allow_non_standard_udd_in_main = true,
         .expect_encrypt_with_app_bound = false,
         .expect_decrypt_works = true,
         .should_reencrypt = true,
         .expected_support_level_in_pre =
             SupportLevel::kNotUsingDefaultUserDataDir,
         .expected_support_level_in_main = SupportLevel::kSupported},
    }),
    [](const auto& info) { return info.param.name; });

}  // namespace os_crypt
