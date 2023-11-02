// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_win.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/process/process_info.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/buildflags.h"
#include "chrome/install_static/install_constants.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/version_info_values.h"
#include "content/public/test/browser_test.h"

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

// This class allows system-level tests to be carried out that do not interfere
// with an existing system-level install.
class FakeInstallDetails : public install_static::PrimaryInstallDetails {
 public:
  // Copy template from first mode from install modes. Some of the values will
  // then be overridden.
  FakeInstallDetails() : constants_(install_static::kInstallModes[0]) {
    // AppGuid determines registry locations, so use a test one.
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
    constants_.app_guid = L"testguid";
#endif

    // This is the CLSID of the test interface, used if
    // kElevatorClsIdForTestingSwitch is supplied on the command line of the
    // elevation service.
    constants_.elevator_clsid = {elevation_service::kTestElevatorClsid};

    // This is the IID of the non-channel specific IElevator Interface. See
    // chrome/elevation_service/elevation_service_idl.idl.
    constants_.elevator_iid = {
        0xA949CB4E,
        0xC4F9,
        0x44C4,
        {0xB2, 0x13, 0x6B, 0xF8, 0xAA, 0x9A, 0xC6,
         0x9C}};  // IElevator IID and TypeLib
                  // {A949CB4E-C4F9-44C4-B213-6BF8AA9AC69C}

    // These are used to generate the name of the service, so keep them
    // different from any real installs.
    constants_.base_app_name = L"testapp";
    constants_.base_app_id = L"testapp";

    // This is needed for shell_integration::GetDefaultBrowser which runs on
    // startup.
    constants_.prog_id_prefix = L"TestHTM";

    set_mode(&constants_);
    set_system_level(true);
  }

  FakeInstallDetails(const FakeInstallDetails&) = delete;
  FakeInstallDetails& operator=(const FakeInstallDetails&) = delete;

 private:
  install_static::InstallConstants constants_;
};

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
    InstallService();
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    if (base::GetCurrentProcessIntegrityLevel() != base::HIGH_INTEGRITY)
      return;
    InProcessBrowserTest::TearDown();
    UnInstallService();
  }

  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList enable_metrics_feature_;

 private:
  static bool InstallService() {
    base::FilePath exe_dir;
    base::PathService::Get(base::DIR_EXE, &exe_dir);
    base::CommandLine service_cmd(
        exe_dir.Append(installer::kElevationServiceExe));
    service_cmd.AppendSwitch(
        elevation_service::switches::kElevatorClsIdForTestingSwitch);
    installer::InstallServiceWorkItem install_service_work_item(
        install_static::GetElevationServiceName(),
        install_static::GetElevationServiceDisplayName(), SERVICE_DEMAND_START,
        service_cmd, base::CommandLine(base::CommandLine::NO_PROGRAM),
        install_static::GetClientStateKeyPath(),
        {install_static::GetElevatorClsid()},
        {install_static::GetElevatorIid()});
    install_service_work_item.set_best_effort(true);
    install_service_work_item.set_rollback_enabled(false);
    return install_service_work_item.Do();
  }

  static bool UnInstallService() {
    return installer::InstallServiceWorkItem::DeleteService(
        install_static::GetElevationServiceName(),
        install_static::GetClientStateKeyPath(),
        {install_static::GetElevatorClsid()},
        {install_static::GetElevatorIid()});
  }

  install_static::ScopedInstallDetails scoped_install_details_;
};

// Test the basic interface to Encrypt and Decrypt data.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, EncryptDecrypt) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  const std::string plaintext("plaintext");
  std::string ciphertext;
  DWORD last_error;

  HRESULT hr = os_crypt::EncryptAppBoundString(
      ProtectionLevel::PATH_VALIDATION, plaintext, ciphertext, last_error);

  ASSERT_HRESULT_SUCCEEDED(hr);

  std::string returned_plaintext;
  hr = os_crypt::DecryptAppBoundString(ciphertext, returned_plaintext,
                                       last_error);

  ASSERT_HRESULT_SUCCEEDED(hr);
  EXPECT_EQ(plaintext, returned_plaintext);
}

// These tests verify that the metrics are recorded correctly. The first load of
// browser in the PRE_ test stores the "Test Key" with app-bound encryption and
// the second stage of the test verifies it can be retrieved successfully.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, PRE_MetricsTest) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  // These histograms are recorded on a background worker thread, so the test
  // needs to wait until this task completes and the histograms are recorded.
  WaitForHistogram("OSCrypt.AppBoundEncryption.Encrypt.ResultCode");
  histogram_tester_.ExpectBucketCount(
      "OSCrypt.AppBoundEncryption.Encrypt.ResultCode", S_OK, 1);

  WaitForHistogram("OSCrypt.AppBoundEncryption.Encrypt.Time");
}

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, MetricsTest) {
  ASSERT_TRUE(install_static::IsSystemInstall());
  // These histograms are recorded on a background worker thread, so the test
  // needs to wait until this task completes and the histograms are recorded.
  WaitForHistogram("OSCrypt.AppBoundEncryption.Decrypt.ResultCode");
  histogram_tester_.ExpectBucketCount(
      "OSCrypt.AppBoundEncryption.Decrypt.ResultCode", S_OK, 1);

  WaitForHistogram("OSCrypt.AppBoundEncryption.Decrypt.Time");
}

// Run this test manually to force uninstall the service.
IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTest, MANUAL_Uninstall) {}

class AppBoundEncryptionWinTestNoService : public InProcessBrowserTest {
 public:
  AppBoundEncryptionWinTestNoService()
      : scoped_install_details_(std::make_unique<FakeInstallDetails>()) {}

 private:
  install_static::ScopedInstallDetails scoped_install_details_;
};

IN_PROC_BROWSER_TEST_F(AppBoundEncryptionWinTestNoService, NoService) {
  const std::string plaintext("plaintext");
  std::string ciphertext;
  DWORD last_error;

  HRESULT hr = os_crypt::EncryptAppBoundString(
      ProtectionLevel::PATH_VALIDATION, plaintext, ciphertext, last_error);

  EXPECT_EQ(REGDB_E_CLASSNOTREG, hr);
  EXPECT_EQ(DWORD{ERROR_GEN_FAILURE}, last_error);
}
