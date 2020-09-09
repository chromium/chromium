// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/force_installed_metrics.h"

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/safe_manifest_parser.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/arc/arc_prefs.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#endif  // defined(OS_CHROMEOS)

namespace {

// The extension ids used here should be valid extension ids.
constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "bcdefghijklmnopabcdefghijklmnopa";
constexpr char kExtensionId3[] = "cdefghijklmnopqrstuvwxyzabcdefgh";
constexpr char kExtensionName1[] = "name1";
constexpr char kExtensionName2[] = "name2";
constexpr char kExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";  // URL of Chrome Web
                                                        // Store backend.

const int kFetchTries = 5;
// HTTP_UNAUTHORIZED
const int kResponseCode = 401;

constexpr char kLoadTimeStats[] = "Extensions.ForceInstalledLoadTime";
constexpr char kTimedOutStats[] = "Extensions.ForceInstalledTimedOutCount";
constexpr char kTimedOutNotInstalledStats[] =
    "Extensions.ForceInstalledTimedOutAndNotInstalledCount";
constexpr char kInstallationFailureCacheStatus[] =
    "Extensions.ForceInstalledFailureCacheStatus";
constexpr char kFailureReasonsCWS[] =
    "Extensions.WebStore_ForceInstalledFailureReason3";
constexpr char kFailureReasonsSH[] =
    "Extensions.OffStore_ForceInstalledFailureReason3";
constexpr char kInstallationStages[] = "Extensions.ForceInstalledStage";
constexpr char kInstallationDownloadingStages[] =
    "Extensions.ForceInstalledDownloadingStage";
constexpr char kFailureCrxInstallErrorStats[] =
    "Extensions.ForceInstalledFailureCrxInstallError";
constexpr char kTotalCountStats[] =
    "Extensions.ForceInstalledTotalCandidateCount";
constexpr char kNetworkErrorCodeStats[] =
    "Extensions.ForceInstalledNetworkErrorCode";
constexpr char kHttpErrorCodeStats[] =
    "Extensions.ForceInstalledHttpErrorCode2";
constexpr char kFetchRetriesStats[] = "Extensions.ForceInstalledFetchTries";
constexpr char kNetworkErrorCodeManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedNetworkErrorCode";
constexpr char kHttpErrorCodeManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedHttpErrorCode2";
constexpr char kFetchRetriesManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedFetchTries";
constexpr char kSandboxUnpackFailureReason[] =
    "Extensions.ForceInstalledFailureSandboxUnpackFailureReason";
#if defined(OS_CHROMEOS)
constexpr char kFailureSessionStats[] =
    "Extensions.ForceInstalledFailureSessionType";
#endif  // defined(OS_CHROMEOS)
constexpr char kPossibleNonMisconfigurationFailures[] =
    "Extensions.ForceInstalledSessionsWithNonMisconfigurationFailureOccured";
constexpr char kDisableReason[] =
    "Extensions.ForceInstalledNotLoadedDisableReason";
constexpr char kBlocklisted[] = "Extensions.ForceInstalledAndBlackListed";
constexpr char kExtensionManifestInvalid[] =
    "Extensions.ForceInstalledFailureManifestInvalidErrorDetail2";
constexpr char kManifestNoUpdatesInfo[] =
    "Extensions.ForceInstalledFailureNoUpdatesInfo";
constexpr char kExtensionManifestInvalidAppStatusError[] =
    "Extensions.ForceInstalledFailureManifestInvalidAppStatusError";
constexpr char kManifestDownloadTimeStats[] =
    "Extensions.ForceInstalledTime.DownloadingStartTo.ManifestDownloadComplete";
constexpr char kCRXDownloadTimeStats[] =
    "Extensions.ForceInstalledTime.ManifestDownloadCompleteTo."
    "CRXDownloadComplete";
constexpr char kVerificationTimeStats[] =
    "Extensions.ForceInstalledTime.VerificationStartTo.CopyingStart";
constexpr char kCopyingTimeStats[] =
    "Extensions.ForceInstalledTime.CopyingStartTo.UnpackingStart";
constexpr char kUnpackingTimeStats[] =
    "Extensions.ForceInstalledTime.UnpackingStartTo.CheckingExpectationsStart";
constexpr char kCheckingExpectationsTimeStats[] =
    "Extensions.ForceInstalledTime.CheckingExpectationsStartTo.FinalizingStart";
constexpr char kFinalizingTimeStats[] =
    "Extensions.ForceInstalledTime.FinalizingStartTo.CRXInstallComplete";

}  // namespace

namespace extensions {

using testing::_;
using testing::Return;

class ForceInstalledMetricsTest : public testing::Test,
                                  public ForceInstalledTracker::Observer {
 public:
  ForceInstalledMetricsTest() = default;

  ForceInstalledMetricsTest(const ForceInstalledMetricsTest&) = delete;
  ForceInstalledMetricsTest& operator=(const ForceInstalledMetricsTest&) =
      delete;

  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(false));

    auto policy_service = std::make_unique<policy::PolicyServiceImpl>(
        std::vector<policy::ConfigurationPolicyProvider*>{&policy_provider_});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile(
        "p1", nullptr, base::UTF8ToUTF16("p1"), 0, "",
        TestingProfile::TestingFactories(), base::nullopt,
        std::move(policy_service));

    prefs_ = profile_->GetTestingPrefService();
    registry_ = ExtensionRegistry::Get(profile_);
    install_stage_tracker_ = InstallStageTracker::Get(profile_);
    auto fake_timer = std::make_unique<base::MockOneShotTimer>();
    fake_timer_ = fake_timer.get();
    tracker_ = std::make_unique<ForceInstalledTracker>(registry_, profile_);
    scoped_observer_.Add(tracker_.get());
    metrics_ = std::make_unique<ForceInstalledMetrics>(
        registry_, profile_, tracker_.get(), std::move(fake_timer));
  }

  void SetupForceList() {
    base::Value list(base::Value::Type::LIST);
    list.Append(base::StrCat({kExtensionId1, ";", kExtensionUpdateUrl}));
    list.Append(base::StrCat({kExtensionId2, ";", kExtensionUpdateUrl}));
    std::unique_ptr<base::Value> dict =
        DictionaryBuilder()
            .Set(kExtensionId1,
                 DictionaryBuilder()
                     .Set(ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Set(kExtensionId2,
                 DictionaryBuilder()
                     .Set(ExternalProviderImpl::kExternalUpdateUrl,
                          kExtensionUpdateUrl)
                     .Build())
            .Build();
    prefs_->SetManagedPref(pref_names::kInstallForceList, std::move(dict));

    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::PolicyMap map;
    map.Set("ExtensionInstallForcelist", policy::POLICY_LEVEL_MANDATORY,
            policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
            std::move(list), nullptr);
    policy_provider_.UpdateChromePolicy(map);
    base::RunLoop().RunUntilIdle();
  }

  void SetupEmptyForceList() {
    std::unique_ptr<base::Value> dict = DictionaryBuilder().Build();
    prefs_->SetManagedPref(pref_names::kInstallForceList, std::move(dict));

    EXPECT_CALL(policy_provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    policy::PolicyMap map;
    policy_provider_.UpdateChromePolicy(std::move(map));
    base::RunLoop().RunUntilIdle();
  }

  // Report downloading manifest stage for both the extensions.
  void ReportDownloadingManifestStage() {
    install_stage_tracker_->ReportDownloadingStage(
        kExtensionId1,
        ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
    install_stage_tracker_->ReportDownloadingStage(
        kExtensionId2,
        ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
  }

  void ReportInstallationStarted(base::Optional<base::TimeDelta> install_time) {
    install_stage_tracker_->ReportDownloadingStage(
        kExtensionId1, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);
    install_stage_tracker_->ReportDownloadingStage(
        kExtensionId1, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
    if (install_time)
      task_environment_.FastForwardBy(install_time.value());
    install_stage_tracker_->ReportDownloadingStage(
        kExtensionId1, ExtensionDownloaderDelegate::Stage::FINISHED);
    install_stage_tracker_->ReportInstallationStage(
        kExtensionId1, InstallStageTracker::Stage::INSTALLING);
  }

  // ForceInstalledTracker::Observer overrides:
  void OnForceInstalledExtensionsLoaded() override { loaded_call_count_++; }
  void OnForceInstalledExtensionsReady() override { ready_call_count_++; }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  policy::MockConfigurationPolicyProvider policy_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  sync_preferences::TestingPrefServiceSyncable* prefs_;
  ExtensionRegistry* registry_;
  InstallStageTracker* install_stage_tracker_;
  base::HistogramTester histogram_tester_;

  base::MockOneShotTimer* fake_timer_;
  std::unique_ptr<ForceInstalledTracker> tracker_;
  std::unique_ptr<ForceInstalledMetrics> metrics_;

  ScopedObserver<ForceInstalledTracker, ForceInstalledTracker::Observer>
      scoped_observer_{this};
  size_t loaded_call_count_ = 0;
  size_t ready_call_count_ = 0;
};

TEST_F(ForceInstalledMetricsTest, EmptyForcelist) {
  SetupEmptyForceList();
  EXPECT_FALSE(fake_timer_->IsRunning());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
  // Don't report metrics when the Forcelist is empty.
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsSH, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsInstalled) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();

  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsSH, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
  tracker_->OnExtensionReady(profile_, ext1.get());
  tracker_->OnExtensionReady(profile_, ext2.get());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
}

TEST_F(ForceInstalledMetricsTest, ObserversOnlyCalledOnce) {
  // Start with a non-empty force-list, and install them, which triggers
  // observer.
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  EXPECT_EQ(1u, loaded_call_count_);
  tracker_->OnExtensionReady(profile_, ext1.get());
  tracker_->OnExtensionReady(profile_, ext2.get());
  EXPECT_EQ(1u, ready_call_count_);

  // Then apply a new set of policies, which shouldn't trigger observers again.
  SetupEmptyForceList();
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsInstallationTimedOut) {
  SetupForceList();
  EXPECT_EQ(0u, loaded_call_count_);
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddEnabled(ext1.get());
  EXPECT_TRUE(fake_timer_->IsRunning());
  EXPECT_EQ(0u, loaded_call_count_);
  fake_timer_->Fire();
  // Metrics are reported due to timeout, but ForceInstalledTracker::Observer
  // never fired.
  EXPECT_EQ(0u, loaded_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 1, 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 1);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 1);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

// Reporting the time for downloading the manifest of an extension and verifying
// that it is correctly recorded in the histogram.
TEST_F(ForceInstalledMetricsTest, ExtensionsManifestDownloadTime) {
  SetupForceList();
  ReportDownloadingManifestStage();
  const base::TimeDelta manifest_download_time =
      base::TimeDelta::FromMilliseconds(200);
  task_environment_.FastForwardBy(manifest_download_time);
  install_stage_tracker_->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_INVALID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kManifestDownloadTimeStats, 1);
  histogram_tester_.ExpectTimeBucketCount(kManifestDownloadTimeStats,
                                          manifest_download_time, 1);
}

// Reporting the time for downloading the CRX file of an extension and verifying
// that it is correctly recorded in the histogram.
TEST_F(ForceInstalledMetricsTest, ExtensionsCrxDownloadTime) {
  SetupForceList();
  ReportDownloadingManifestStage();
  const base::TimeDelta install_time = base::TimeDelta::FromMilliseconds(200);
  ReportInstallationStarted(install_time);
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_INVALID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kCRXDownloadTimeStats, 1);
  histogram_tester_.ExpectTimeBucketCount(kCRXDownloadTimeStats, install_time,
                                          1);
}

TEST_F(ForceInstalledMetricsTest,
       ExtensionsCrxDownloadTimeWhenFetchedFromCache) {
  SetupForceList();
  ReportDownloadingManifestStage();
  install_stage_tracker_->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);
  install_stage_tracker_->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::FINISHED);
  install_stage_tracker_->ReportInstallationStage(
      kExtensionId1, InstallStageTracker::Stage::INSTALLING);
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_INVALID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  // Time should not be recorded when CRX is fetched from cache.
  histogram_tester_.ExpectTotalCount(kCRXDownloadTimeStats, 0);
}

// Reporting the times for various stages in the extension installation process
// and verifying that the time consumed at each stage is correctly recorded in
// the histogram.
TEST_F(ForceInstalledMetricsTest, ExtensionsReportInstallationStageTimes) {
  SetupForceList();
  ReportDownloadingManifestStage();
  ReportInstallationStarted(base::nullopt);
  install_stage_tracker_->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kVerification);

  const base::TimeDelta installation_stage_time =
      base::TimeDelta::FromMilliseconds(200);
  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker_->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kCopying);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker_->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kUnpacking);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker_->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kCheckingExpectations);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker_->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kFinalizing);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker_->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kComplete);

  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_INVALID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kVerificationTimeStats, 1);
  histogram_tester_.ExpectTimeBucketCount(kVerificationTimeStats,
                                          installation_stage_time, 1);
  histogram_tester_.ExpectTotalCount(kCopyingTimeStats, 1);
  histogram_tester_.ExpectTimeBucketCount(kCopyingTimeStats,
                                          installation_stage_time, 1);
  histogram_tester_.ExpectTotalCount(kUnpackingTimeStats, 1);
  histogram_tester_.ExpectTimeBucketCount(kUnpackingTimeStats,
                                          installation_stage_time, 1);
  histogram_tester_.ExpectTotalCount(kCheckingExpectationsTimeStats, 1);
  histogram_tester_.ExpectTimeBucketCount(kCheckingExpectationsTimeStats,
                                          installation_stage_time, 1);
  histogram_tester_.ExpectTotalCount(kFinalizingTimeStats, 1);
  histogram_tester_.ExpectTimeBucketCount(kFinalizingTimeStats,
                                          installation_stage_time, 1);
}

// Reporting disable reason for the force installed extensions which are
// installed but not loaded when extension is disable due to single reason.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsInstalledButNotLoadedUniqueDisableReason) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddDisabled(ext1.get());
  ExtensionPrefs::Get(profile_)->AddDisableReason(
      kExtensionId1, disable_reason::DisableReason::DISABLE_NOT_VERIFIED);
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // ForceInstalledMetrics should still keep running as kExtensionId1 is
  // installed but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kDisableReason, disable_reason::DisableReason::DISABLE_NOT_VERIFIED, 1);
}

// Reporting disable reasons for the force installed extensions which are
// installed but not loaded when extension is disable due to multiple reasons.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsInstalledButNotLoadedMultipleDisableReason) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddDisabled(ext1.get());
  ExtensionPrefs::Get(profile_)->AddDisableReasons(
      kExtensionId1,
      disable_reason::DisableReason::DISABLE_NOT_VERIFIED |
          disable_reason::DisableReason::DISABLE_UNSUPPORTED_REQUIREMENT);
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // ForceInstalledMetrics should still keep running as kExtensionId1 is
  // installed but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  // Verifies that only one disable reason is reported;
  histogram_tester_.ExpectUniqueSample(
      kDisableReason,
      disable_reason::DisableReason::DISABLE_UNSUPPORTED_REQUIREMENT, 1);
}

// Reporting DisableReason::DISABLE_NONE for the force installed extensions
// which are installed but not loaded when extension is enabled.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsInstalledButNotLoadedNoDisableReason) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddEnabled(ext1.get());
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // ForceInstalledMetrics should still keep running as kExtensionId1 is
  // installed but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kDisableReason, disable_reason::DisableReason::DISABLE_NONE, 1);
}

TEST_F(ForceInstalledMetricsTest, ExtensionForceInstalledAndBlocklisted) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddBlocklisted(ext1.get());
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  registry_->AddEnabled(ext2.get());
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // ForceInstalledMetrics should still keep running as kExtensionId1 is
  // installed but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(kBlocklisted, 1, 1);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsInstallationCancelled) {
  SetupForceList();
  SetupEmptyForceList();
  // ForceInstalledMetrics does not shut down the timer, because it's still
  // waiting for the initial extensions to install.
  EXPECT_TRUE(fake_timer_->IsRunning());
  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForceInstalledMetricsTest, ForcedExtensionsAddedAfterManualExtensions) {
  // Report failure for an extension which is not in forced list.
  install_stage_tracker_->ReportFailure(
      kExtensionId3, InstallStageTracker::FailureReason::INVALID_ID);
  // ForceInstalledMetrics should keep running as the forced extensions are
  // still not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  EXPECT_EQ(0u, loaded_call_count_);
  EXPECT_EQ(0u, ready_call_count_);
  SetupForceList();

  auto ext = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext.get());
  tracker_->OnExtensionReady(profile_, ext.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  // ForceInstalledMetrics shuts down timer because kExtensionId1 was loaded and
  // kExtensionId2 was failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  EXPECT_EQ(1u, loaded_call_count_);
  EXPECT_EQ(1u, ready_call_count_);
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::INVALID_ID, 1);
}

TEST_F(ForceInstalledMetricsTest,
       ExtensionsInstallationTimedOutDifferentReasons) {
  SetupForceList();
  install_stage_tracker_->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker_->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_OTHER,
      CrxInstallErrorDetail::UNEXPECTED_ID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 2, 1);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 2);
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::INVALID_ID, 1);
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_OTHER, 1);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectUniqueSample(kFailureCrxInstallErrorStats,
                                       CrxInstallErrorDetail::UNEXPECTED_ID, 1);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

// Reporting SandboxedUnpackerFailureReason when the force installed extension
// fails to install with error CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsCrxInstallErrorSandboxUnpackFailure) {
  SetupForceList();
  install_stage_tracker_->ReportSandboxedUnpackerFailureReason(
      kExtensionId1, SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE);
  install_stage_tracker_->ReportSandboxedUnpackerFailureReason(
      kExtensionId2, SandboxedUnpackerFailureReason::UNZIP_FAILED);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kSandboxUnpackFailureReason, 2);
  histogram_tester_.ExpectBucketCount(
      kSandboxUnpackFailureReason,
      SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE, 1);
  histogram_tester_.ExpectBucketCount(
      kSandboxUnpackFailureReason, SandboxedUnpackerFailureReason::UNZIP_FAILED,
      1);
}

// Reporting info when the force installed extension fails to install with error
// CRX_FETCH_URL_EMPTY due to no updates from the server.
TEST_F(ForceInstalledMetricsTest, ExtensionsNoUpdatesInfoReporting) {
  SetupForceList();

  install_stage_tracker_->ReportInfoOnNoUpdatesFailure(kExtensionId1,
                                                       "disabled by client");
  install_stage_tracker_->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);
  install_stage_tracker_->ReportInfoOnNoUpdatesFailure(kExtensionId2, "");
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);

  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kManifestNoUpdatesInfo, 2);
  histogram_tester_.ExpectBucketCount(
      kManifestNoUpdatesInfo, InstallStageTracker::NoUpdatesInfo::kEmpty, 1);
  histogram_tester_.ExpectBucketCount(
      kManifestNoUpdatesInfo,
      InstallStageTracker::NoUpdatesInfo::kDisabledByClient, 1);
}

// Regression test to check if the metrics are collected properly for the
// extensions which are already installed and loaded and then fail with error
// ALREADY_INSTALLED.
TEST_F(ForceInstalledMetricsTest,
       ExtensionLoadedThenFailedWithAlreadyInstalledError) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::ALREADY_INSTALLED);
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
}

// Regression test to check if the metrics are collected properly for the
// extensions which are in state READY.
TEST_F(ForceInstalledMetricsTest, ExtensionsReady) {
  SetupForceList();
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, ext1.get());
  tracker_->OnExtensionReady(profile_, ext1.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::ALREADY_INSTALLED);
  auto ext2 = ExtensionBuilder(kExtensionName2).SetID(kExtensionId2).Build();
  tracker_->OnExtensionLoaded(profile_, ext2.get());
  tracker_->OnExtensionReady(profile_, ext2.get());
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsStuck) {
  SetupForceList();
  install_stage_tracker_->ReportInstallationStage(
      kExtensionId1, InstallStageTracker::Stage::PENDING);
  install_stage_tracker_->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker_->ReportDownloadingStage(
      kExtensionId2, ExtensionDownloaderDelegate::Stage::PENDING);
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 2);
  histogram_tester_.ExpectBucketCount(kInstallationStages,
                                      InstallStageTracker::Stage::PENDING, 1);
  histogram_tester_.ExpectBucketCount(
      kInstallationStages, InstallStageTracker::Stage::DOWNLOADING, 1);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

#if defined(OS_CHROMEOS)
TEST_F(ForceInstalledMetricsTest, ReportManagedGuestSessionOnExtensionFailure) {
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  const AccountId account_id =
      AccountId::FromUserEmail(profile_->GetProfileUserName());
  user_manager::User* user =
      fake_user_manager->AddPublicAccountUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  SetupForceList();
  install_stage_tracker_->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker_->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_OTHER,
      CrxInstallErrorDetail::UNEXPECTED_ID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kFailureSessionStats,
      ForceInstalledMetrics::UserType::USER_TYPE_PUBLIC_ACCOUNT, 2);
}

TEST_F(ForceInstalledMetricsTest, ReportGuestSessionOnExtensionFailure) {
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  const AccountId account_id =
      AccountId::FromUserEmail(profile_->GetProfileUserName());
  user_manager::User* user = fake_user_manager->AddGuestUser();
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(user);
  SetupForceList();
  install_stage_tracker_->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker_->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_OTHER,
      CrxInstallErrorDetail::UNEXPECTED_ID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kFailureSessionStats, ForceInstalledMetrics::UserType::USER_TYPE_GUEST,
      2);
}
#endif  // defined(OS_CHROMEOS)

TEST_F(ForceInstalledMetricsTest, ExtensionsAreDownloading) {
  SetupForceList();
  install_stage_tracker_->ReportInstallationStage(
      kExtensionId1, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker_->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
  install_stage_tracker_->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker_->ReportDownloadingStage(
      kExtensionId2, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectUniqueSample(kTimedOutStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(kTimedOutNotInstalledStats, 2, 1);
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 2);
  histogram_tester_.ExpectUniqueSample(
      kInstallationStages, InstallStageTracker::Stage::DOWNLOADING, 2);
  histogram_tester_.ExpectTotalCount(kInstallationDownloadingStages, 2);
  histogram_tester_.ExpectBucketCount(
      kInstallationDownloadingStages,
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST, 1);
  histogram_tester_.ExpectBucketCount(
      kInstallationDownloadingStages,
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX, 1);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs_->GetManagedPref(pref_names::kInstallForceList)->DictSize(), 1);
}

// Error Codes in case of CRX_FETCH_FAILED.
TEST_F(ForceInstalledMetricsTest, ExtensionCrxFetchFailed) {
  SetupForceList();
  ExtensionDownloaderDelegate::FailureData data1(net::Error::OK, kResponseCode,
                                                 kFetchTries);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  install_stage_tracker_->ReportFetchError(
      kExtensionId1, InstallStageTracker::FailureReason::CRX_FETCH_FAILED,
      data1);
  install_stage_tracker_->ReportFetchError(
      kExtensionId2, InstallStageTracker::FailureReason::CRX_FETCH_FAILED,
      data2);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeStats, net::Error::OK,
                                      1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeStats, kResponseCode, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesStats, kFetchTries, 2);
}

// Error Codes in case of MANIFEST_FETCH_FAILED.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestFetchFailed) {
  SetupForceList();
  ExtensionDownloaderDelegate::FailureData data1(net::Error::OK, kResponseCode,
                                                 kFetchTries);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  install_stage_tracker_->ReportFetchError(
      kExtensionId1, InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED,
      data1);
  install_stage_tracker_->ReportFetchError(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED,
      data2);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchFailedStats,
                                      kResponseCode, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesManifestFetchFailedStats,
                                      kFetchTries, 2);
}

// Errors occurred because the fetched update manifest was invalid.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestInvalid) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportManifestInvalidFailure(
      kExtensionId2,
      ExtensionDownloaderDelegate::FailureData(
          ManifestInvalidError::INVALID_PROTOCOL_ON_GUPDATE_TAG));
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectUniqueSample(
      kExtensionManifestInvalid,
      ManifestInvalidError::INVALID_PROTOCOL_ON_GUPDATE_TAG, 1);
}

// Errors occurred because the fetched update manifest was invalid because app
// status was not OK.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestInvalidAppStatusError) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportManifestInvalidFailure(
      kExtensionId2,
      ExtensionDownloaderDelegate::FailureData(
          ManifestInvalidError::BAD_APP_STATUS, "error-unknownApplication"));
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectUniqueSample(kExtensionManifestInvalid,
                                       ManifestInvalidError::BAD_APP_STATUS, 1);
  histogram_tester_.ExpectUniqueSample(
      kExtensionManifestInvalidAppStatusError,
      InstallStageTracker::AppStatusError::kErrorUnknownApplication, 1);
}

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// CRX_INSTALL_ERROR with detailed error KIOSK_MODE_ONLY is considered as
// misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentKioskModeOnlyError) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::KIOSK_MODE_ONLY);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// CRX_INSTALL_ERROR with detailed error DISALLOWED_BY_POLICY and when extension
// type which is not allowed to install according to policy
// kExtensionAllowedTypes is considered as misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentDisallowedByPolicyTypeError) {
  SetupForceList();
  // Set TYPE_EXTENSION and TYPE_THEME as the allowed extension types.
  std::unique_ptr<base::Value> list =
      ListBuilder().Append("extension").Append("theme").Build();
  prefs_->SetManagedPref(pref_names::kAllowedTypes, std::move(list));

  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  // Hosted app is not a valid extension type, so this should report an error.
  install_stage_tracker_->ReportExtensionType(kExtensionId2,
                                              Manifest::Type::TYPE_HOSTED_APP);
  install_stage_tracker_->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::DISALLOWED_BY_POLICY);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kPossibleNonMisconfigurationFailures,
      0 /*Misconfiguration failure not present*/, 1 /*Count of the sample*/);
}

// Session in which at least one non misconfiguration failure occurred. One of
// the extension fails to install with DISALLOWED_BY_POLICY error but has
// extension type which is allowed by policy ExtensionAllowedTypes. This is not
// a misconfiguration failure.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailurePresentDisallowedByPolicyError) {
  SetupForceList();

  // Set TYPE_EXTENSION and TYPE_THEME as the allowed extension types.
  std::unique_ptr<base::Value> list =
      ListBuilder().Append("extension").Append("theme").Build();
  prefs_->SetManagedPref(pref_names::kAllowedTypes, std::move(list));

  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportExtensionType(kExtensionId2,
                                              Manifest::Type::TYPE_EXTENSION);
  install_stage_tracker_->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::DISALLOWED_BY_POLICY);

  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures,
                                      1 /*Misconfiguration failure present*/,
                                      1 /*Count of the sample*/);
}

// Session in which at least one non misconfiguration failure occurred.
// Misconfiguration failure includes error KIOSK_MODE_ONLY, when force installed
// extension fails to install with failure reason CRX_INSTALL_ERROR.
TEST_F(ForceInstalledMetricsTest, NonMisconfigurationFailurePresent) {
  SetupForceList();
  install_stage_tracker_->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker_->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::KIOSK_MODE_ONLY);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 1,
                                      1);
}

#if defined(OS_CHROMEOS)
// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// REPLACED_BY_ARC_APP is not considered as misconfiguration when ARC++ is
// enabled for the profile.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentReplacedByArcAppErrorArcEnabled) {
  // Enable ARC++ for this profile.
  prefs_->SetManagedPref(arc::prefs::kArcEnabled,
                         std::make_unique<base::Value>(true));
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::REPLACED_BY_ARC_APP);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Session in which at least one non misconfiguration failure occurred. This
// test verifies that failure REPLACED_BY_ARC_APP is not considered as
// misconfiguration when ARC++ is disabled for the profile.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentReplacedByArcAppErrorArcDisabled) {
  // Enable ARC++ for this profile.
  prefs_->SetManagedPref(arc::prefs::kArcEnabled,
                         std::make_unique<base::Value>(false));
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::REPLACED_BY_ARC_APP);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 1,
                                      1);
}
#endif  // defined(OS_CHROMEOS)

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// NOT_PERFORMING_NEW_INSTALL is considered as misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentNotPerformingNewInstallError) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportFailure(
      kExtensionId2,
      InstallStageTracker::FailureReason::NOT_PERFORMING_NEW_INSTALL);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// CRX_FETCH_URL_EMPTY with empty info field is considered as misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentCrxFetchUrlEmptyError) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportInfoOnNoUpdatesFailure(kExtensionId2, "");
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// This test verifies that failure CRX_FETCH_URL_EMPTY with non empty info field
// is not considered as a misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailurePresentCrxFetchUrlEmptyError) {
  SetupForceList();
  auto extension =
      ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  tracker_->OnExtensionLoaded(profile_, extension.get());
  install_stage_tracker_->ReportInfoOnNoUpdatesFailure(kExtensionId2,
                                                       "rate limit");
  install_stage_tracker_->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      0);
}

TEST_F(ForceInstalledMetricsTest, NoExtensionsConfigured) {
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForceInstalledMetricsTest, CachedExtensions) {
  SetupForceList();
  install_stage_tracker_->ReportDownloadingCacheStatus(
      kExtensionId1, ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT);
  install_stage_tracker_->ReportDownloadingCacheStatus(
      kExtensionId2, ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS);
  auto ext1 = ExtensionBuilder(kExtensionName1).SetID(kExtensionId1).Build();
  registry_->AddEnabled(ext1.get());
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  // If an extension was installed successfully, don't mention it in statistics.
  histogram_tester_.ExpectUniqueSample(
      kInstallationFailureCacheStatus,
      ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS, 1);
}

}  // namespace extensions
