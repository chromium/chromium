// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/forced_extensions/force_installed_metrics.h"

#include <optional>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_test_base.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install/sandboxed_unpacker_failure_reason.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/updater/safe_manifest_parser.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_prefs.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// Intentionally invalid extension id.
constexpr char kExtensionId3[] = "cdefghijklmnopqrstuvwxyzabcdefgh";

const int kFetchTries = 5;
// HTTP_FORBIDDEN
const int kHttpCodeForbidden = 403;

constexpr char kLoadTimeStats[] = "Extensions.ForceInstalledLoadTime";
constexpr char kReadyTimeStats[] = "Extensions.ForceInstalledReadyTime";
constexpr char kTimedOutStats[] = "Extensions.ForceInstalledTimedOutCount";
constexpr char kTimedOutNotInstalledStats[] =
    "Extensions.ForceInstalledTimedOutAndNotInstalledCount";
constexpr char kInstallationFailureCacheStatus[] =
    "Extensions.ForceInstalledFailureCacheStatus";
constexpr char kFailureReasonsCWS[] =
    "Extensions.WebStore_ForceInstalledFailureReason3";
constexpr char kFailureReasonsSH[] =
    "Extensions.OffStore_ForceInstalledFailureReason3";
constexpr char kInstallationStages[] = "Extensions.ForceInstalledStage2";
constexpr char kInstallCreationStages[] =
    "Extensions.ForceInstalledCreationStage";
constexpr char kInstallationDownloadingStages[] =
    "Extensions.ForceInstalledDownloadingStage";
constexpr char kFailureCrxInstallErrorStats[] =
    "Extensions.ForceInstalledFailureCrxInstallError";
constexpr char kTotalCountStats[] =
    "Extensions.ForceInstalledTotalCandidateCount";
constexpr char kNetworkErrorCodeCrxFetchRetryStats[] =
    "Extensions.ForceInstalledCrxFetchRetryNetworkErrorCode";
constexpr char kHttpErrorCodeCrxFetchRetryStatsCWS[] =
    "Extensions.WebStore_ForceInstalledCrxFetchRetryHttpErrorCode2";
constexpr char kHttpErrorCodeCrxFetchRetryStatsSH[] =
    "Extensions.OffStore_ForceInstalledCrxFetchRetryHttpErrorCode2";
constexpr char kFetchRetriesCrxFetchRetryStats[] =
    "Extensions.ForceInstalledCrxFetchRetryFetchTries";
constexpr char kNetworkErrorCodeCrxFetchFailedStats[] =
    "Extensions.ForceInstalledNetworkErrorCode";
constexpr char kHttpErrorCodeCrxFetchFailedStats[] =
    "Extensions.ForceInstalledHttpErrorCode2";
constexpr char kHttpErrorCodeCrxFetchFailedStatsCWS[] =
    "Extensions.WebStore_ForceInstalledHttpErrorCode2";
constexpr char kHttpErrorCodeCrxFetchFailedStatsSH[] =
    "Extensions.OffStore_ForceInstalledHttpErrorCode2";
constexpr char kFetchRetriesCrxFetchFailedStats[] =
    "Extensions.ForceInstalledFetchTries";
constexpr char kNetworkErrorCodeManifestFetchRetryStats[] =
    "Extensions.ForceInstalledManifestFetchRetryNetworkErrorCode";
constexpr char kHttpErrorCodeManifestFetchRetryStatsCWS[] =
    "Extensions.WebStore_ForceInstalledManifestFetchRetryHttpErrorCode2";
constexpr char kHttpErrorCodeManifestFetchRetryStatsSH[] =
    "Extensions.OffStore_ForceInstalledManifestFetchRetryHttpErrorCode2";
constexpr char kFetchRetriesManifestFetchRetryStats[] =
    "Extensions.ForceInstalledManifestFetchRetryFetchTries";
constexpr char kNetworkErrorCodeManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedNetworkErrorCode";
constexpr char kHttpErrorCodeManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedHttpErrorCode2";
constexpr char kHttpErrorCodeManifestFetchFailedStatsCWS[] =
    "Extensions.WebStore_ForceInstalledManifestFetchFailedHttpErrorCode2";
constexpr char kHttpErrorCodeManifestFetchFailedStatsSH[] =
    "Extensions.OffStore_ForceInstalledManifestFetchFailedHttpErrorCode2";
constexpr char kFetchRetriesManifestFetchFailedStats[] =
    "Extensions.ForceInstalledManifestFetchFailedFetchTries";
constexpr char kSandboxUnpackFailureReason[] =
    "Extensions.ForceInstalledFailureSandboxUnpackFailureReason2";
#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kFailureSessionStats[] =
    "Extensions.ForceInstalledFailureSessionType";
constexpr char kStuckInCreateStageSessionType[] =
    "Extensions.ForceInstalledFailureSessionType."
    "ExtensionStuckInInitialCreationStage";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kPossibleNonMisconfigurationFailures[] =
    "Extensions.ForceInstalledSessionsWithNonMisconfigurationFailureOccured";
constexpr char kDisableReason[] =
    "Extensions.ForceInstalledNotLoadedDisableReason";
constexpr char kBlocklisted[] = "Extensions.ForceInstalledAndBlockListed";
constexpr char kWebStoreExtensionManifestInvalid[] =
    "Extensions.WebStore_ForceInstalledFailureManifestInvalidErrorDetail2";
constexpr char kOffStoreExtensionManifestInvalid[] =
    "Extensions.OffStore_ForceInstalledFailureManifestInvalidErrorDetail2";
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
constexpr char kCrxHeaderInvalidFailureIsCWS[] =
    "Extensions.ForceInstalledFailureWithCrxHeaderInvalidIsCWS";
constexpr char kCrxHeaderInvalidFailureFromCache[] =
    "Extensions.ForceInstalledFailureWithCrxHeaderInvalidIsFromCache";
constexpr char kStuckInCreatedStageAreExtensionsEnabled[] =
    "Extensions."
    "ForceInstalledFailureStuckInInitialCreationStageAreExtensionsEnabled";

}  // namespace

namespace extensions {

using ExtensionStatus = ForceInstalledTracker::ExtensionStatus;
using ExtensionOrigin = ForceInstalledTestBase::ExtensionOrigin;
using testing::_;
using testing::Return;

class ForceInstalledMetricsTest : public ForceInstalledTestBase {
 public:
  ForceInstalledMetricsTest() = default;

  ForceInstalledMetricsTest(const ForceInstalledMetricsTest&) = delete;
  ForceInstalledMetricsTest& operator=(const ForceInstalledMetricsTest&) =
      delete;

  void SetUp() override {
    ForceInstalledTestBase::SetUp();
    auto fake_timer = std::make_unique<base::MockOneShotTimer>();
    fake_timer_ = fake_timer.get();
    metrics_ = std::make_unique<ForceInstalledMetrics>(
        registry(), profile(), force_installed_tracker(),
        std::move(fake_timer));
  }

  void SetupExtensionManagementPref() {
    base::Value::Dict extension_entry =
        base::Value::Dict()
            .Set("installation_mode", "allowed")
            .Set(ExternalProviderImpl::kExternalUpdateUrl, kExtensionUpdateUrl);
    prefs()->SetManagedPref(
        pref_names::kExtensionManagement,
        base::Value::Dict().Set(kExtensionId1, std::move(extension_entry)));
  }

  void CreateExtensionService(bool extensions_enabled) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    if (!extensions_enabled) {
      command_line.AppendSwitch(::switches::kDisableExtensions);
    }
    extensions::TestExtensionSystem* test_ext_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    test_ext_system->CreateExtensionService(&command_line, base::FilePath(),
                                            false);
  }

  // Report downloading manifest stage for both the extensions.
  void ReportDownloadingManifestStage() {
    install_stage_tracker()->ReportDownloadingStage(
        kExtensionId1,
        ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
    install_stage_tracker()->ReportDownloadingStage(
        kExtensionId2,
        ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
  }

  void ReportInstallationStarted(std::optional<base::TimeDelta> install_time) {
    install_stage_tracker()->ReportDownloadingStage(
        kExtensionId1, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);
    install_stage_tracker()->ReportDownloadingStage(
        kExtensionId1, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX);
    if (install_time) {
      task_environment_.FastForwardBy(install_time.value());
    }
    install_stage_tracker()->ReportDownloadingStage(
        kExtensionId1, ExtensionDownloaderDelegate::Stage::FINISHED);
    install_stage_tracker()->ReportInstallationStage(
        kExtensionId1, InstallStageTracker::Stage::INSTALLING);
  }

 protected:
  base::HistogramTester histogram_tester_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> fake_timer_;
  std::unique_ptr<ForceInstalledMetrics> metrics_;
};

TEST_F(ForceInstalledMetricsTest, EmptyForcelist) {
  SetupEmptyForceList();
  // ForceInstalledMetrics is notified only when Forcelist is not empty.
  EXPECT_TRUE(fake_timer_->IsRunning());
  // Don't report metrics when the Forcelist is empty.
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kReadyTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsSH, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsInstalled) {
  SetupForceList(ExtensionOrigin::kWebStore);

  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);

  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kReadyTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsSH, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectUniqueSample(
      kTotalCountStats,
      prefs()->GetManagedPref(pref_names::kInstallForceList)->GetDict().size(),
      1);
}

// Verifies that failure is reported for the extensions which are listed in
// forced list, and their installation mode are overridden by ExtensionSettings
// policy to something else.
TEST_F(ForceInstalledMetricsTest, ExtensionSettingsOverrideForcedList) {
  SetupForceList(ExtensionOrigin::kWebStore);
  SetupExtensionManagementPref();
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS,
      InstallStageTracker::FailureReason::OVERRIDDEN_BY_SETTINGS, 1);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsInstallationTimedOut) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  registry()->AddEnabled(ext.get());
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  // Metrics are reported due to timeout.
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
      prefs()->GetManagedPref(pref_names::kInstallForceList)->GetDict().size(),
      1);
}

// Reporting the time for downloading the manifest of an extension and verifying
// that it is correctly recorded in the histogram.
TEST_F(ForceInstalledMetricsTest, ExtensionsManifestDownloadTime) {
  SetupForceList(ExtensionOrigin::kWebStore);
  ReportDownloadingManifestStage();
  const base::TimeDelta manifest_download_time = base::Milliseconds(200);
  task_environment_.FastForwardBy(manifest_download_time);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  ReportDownloadingManifestStage();
  const base::TimeDelta install_time = base::Milliseconds(200);
  ReportInstallationStarted(install_time);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  ReportDownloadingManifestStage();
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::MANIFEST_LOADED);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::FINISHED);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId1, InstallStageTracker::Stage::INSTALLING);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  ReportDownloadingManifestStage();
  ReportInstallationStarted(std::nullopt);
  install_stage_tracker()->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kVerification);

  const base::TimeDelta installation_stage_time = base::Milliseconds(200);
  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker()->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kCopying);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker()->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kUnpacking);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker()->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kCheckingExpectations);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker()->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kFinalizing);

  task_environment_.FastForwardBy(installation_stage_time);
  install_stage_tracker()->ReportCRXInstallationStage(
      kExtensionId1, InstallationStage::kComplete);

  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  registry()->AddDisabled(ext1.get());
  ExtensionPrefs::Get(profile())->AddDisableReason(
      kExtensionId1, disable_reason::DisableReason::DISABLE_NOT_VERIFIED);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  registry()->AddDisabled(ext1.get());
  ExtensionPrefs::Get(profile())->AddDisableReasons(
      kExtensionId1,
      disable_reason::DisableReason::DISABLE_NOT_VERIFIED |
          disable_reason::DisableReason::DISABLE_UNSUPPORTED_REQUIREMENT);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  registry()->AddEnabled(ext1.get());
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics should still keep running as kExtensionId1 is
  // installed but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kDisableReason, disable_reason::DisableReason::DISABLE_NONE, 1);
}

// Verifies if the metrics related to whether the extensions are enabled or not
// are recorded correctly for extensions stuck in
// NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED stage.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsStuckInCreatedStageAreExtensionsEnabled) {
  SetupForceList(ExtensionOrigin::kWebStore);
  CreateExtensionService(/*extensions_enabled=*/false);

  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::CREATED);
  install_stage_tracker()->ReportInstallCreationStage(
      kExtensionId2, InstallStageTracker::InstallCreationStage::
                         NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED);

  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 1);
  histogram_tester_.ExpectBucketCount(kInstallationStages,
                                      InstallStageTracker::Stage::CREATED, 1);
  histogram_tester_.ExpectBucketCount(
      kInstallCreationStages,
      InstallStageTracker::InstallCreationStage::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED,
      1);
  histogram_tester_.ExpectUniqueSample(kStuckInCreatedStageAreExtensionsEnabled,
                                       false, 1);
}

TEST_F(ForceInstalledMetricsTest, ExtensionForceInstalledAndBlocklisted) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  registry()->AddBlocklisted(ext1.get());
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics should still keep running as kExtensionId1 is
  // installed but not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(kBlocklisted, 1, 1);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsInstallationCancelled) {
  SetupForceList(ExtensionOrigin::kWebStore);
  SetupEmptyForceList();
  // ForceInstalledMetrics does not shut down the timer, because it's still
  // waiting for the initial extensions to install.
  EXPECT_TRUE(fake_timer_->IsRunning());
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
  install_stage_tracker()->ReportFailure(
      kExtensionId3, InstallStageTracker::FailureReason::INVALID_ID);
  // ForceInstalledMetrics should keep running as the forced extensions are
  // still not loaded.
  EXPECT_TRUE(fake_timer_->IsRunning());
  SetupForceList(ExtensionOrigin::kWebStore);

  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kReady);
  install_stage_tracker()->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  // ForceInstalledMetrics shuts down timer because kExtensionId1 was loaded and
  // kExtensionId2 was failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::INVALID_ID, 1);
}

TEST_F(ForceInstalledMetricsTest,
       ExtensionsInstallationTimedOutDifferentReasons) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker()->ReportCrxInstallError(
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
      prefs()->GetManagedPref(pref_names::kInstallForceList)->GetDict().size(),
      1);
}

// Reporting SandboxedUnpackerFailureReason when the force installed extension
// fails to install with error CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsCrxInstallErrorSandboxUnpackFailure) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportSandboxedUnpackerFailureReason(
      kExtensionId1,
      CrxInstallError(SandboxedUnpackerFailureReason::CRX_FILE_NOT_READABLE,
                      std::u16string()));
  install_stage_tracker()->ReportSandboxedUnpackerFailureReason(
      kExtensionId2,
      CrxInstallError(SandboxedUnpackerFailureReason::UNZIP_FAILED,
                      std::u16string()));
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

// Reporting when the extension is downloaded from cache and it fails to install
// with error CRX_HEADER_INVALID.
TEST_F(ForceInstalledMetricsTest, ExtensionsCrxHeaderInvalidFromCache) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportDownloadingCacheStatus(
      kExtensionId1, ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT);
  install_stage_tracker()->ReportSandboxedUnpackerFailureReason(
      kExtensionId1,
      CrxInstallError(SandboxedUnpackerFailureReason::CRX_HEADER_INVALID,
                      std::u16string()));
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kSandboxUnpackFailureReason, 1);
  histogram_tester_.ExpectBucketCount(
      kSandboxUnpackFailureReason,
      SandboxedUnpackerFailureReason::CRX_HEADER_INVALID, 1);
  histogram_tester_.ExpectBucketCount(kCrxHeaderInvalidFailureIsCWS, true, 1);
  histogram_tester_.ExpectBucketCount(kCrxHeaderInvalidFailureFromCache, true,
                                      1);
}

// Reporting when the extension is not downloaded from cache and it fails to
// install with error CRX_HEADER_INVALID.
TEST_F(ForceInstalledMetricsTest, ExtensionsCrxHeaderInvalidNotFromCache) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportDownloadingCacheStatus(
      kExtensionId1, ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS);
  install_stage_tracker()->ReportSandboxedUnpackerFailureReason(
      kExtensionId1,
      CrxInstallError(SandboxedUnpackerFailureReason::CRX_HEADER_INVALID,
                      std::u16string()));
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kSandboxUnpackFailureReason, 1);
  histogram_tester_.ExpectBucketCount(
      kSandboxUnpackFailureReason,
      SandboxedUnpackerFailureReason::CRX_HEADER_INVALID, 1);
  histogram_tester_.ExpectBucketCount(kCrxHeaderInvalidFailureIsCWS, true, 1);
  histogram_tester_.ExpectBucketCount(kCrxHeaderInvalidFailureFromCache, false,
                                      1);
}

// Verifies that offstore extension that is downloaded from the update server
// and fails with CRX_HEADER_INVALID error is considered as a misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsCrxHeaderInvalidIsMisconfiguration) {
  SetupForceList(ExtensionOrigin::kOffStore);
  install_stage_tracker()->ReportDownloadingCacheStatus(
      kExtensionId1, ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS);
  install_stage_tracker()->ReportSandboxedUnpackerFailureReason(
      kExtensionId1,
      CrxInstallError(SandboxedUnpackerFailureReason::CRX_HEADER_INVALID,
                      std::u16string()));
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kSandboxUnpackFailureReason, 1);
  histogram_tester_.ExpectBucketCount(
      kSandboxUnpackFailureReason,
      SandboxedUnpackerFailureReason::CRX_HEADER_INVALID, 1);
  histogram_tester_.ExpectBucketCount(kCrxHeaderInvalidFailureIsCWS, false, 1);
  histogram_tester_.ExpectBucketCount(kCrxHeaderInvalidFailureFromCache, false,
                                      1);
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Verifies that extension failing with REPLACED_BY_SYSTEM_APP error is
// considered as a misconfiguration in ChromeOS.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsFailureReplacedBySystemAppIsMisconfiguration) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportFailure(
      kExtensionId1,
      InstallStageTracker::FailureReason::REPLACED_BY_SYSTEM_APP);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS,
      InstallStageTracker::FailureReason::REPLACED_BY_SYSTEM_APP, 1);
  bool expected_non_misconfiguration_failure = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  expected_non_misconfiguration_failure = false;
#endif
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures,
                                      expected_non_misconfiguration_failure, 1);
}

// Reporting info when the force installed extension fails to install with error
// CRX_FETCH_URL_EMPTY due to no updates from the server.
TEST_F(ForceInstalledMetricsTest, ExtensionsNoUpdatesInfoReporting) {
  SetupForceList(ExtensionOrigin::kWebStore);

  install_stage_tracker()->ReportInfoOnNoUpdatesFailure(kExtensionId1,
                                                        "disabled by client");
  install_stage_tracker()->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::CRX_FETCH_URL_EMPTY);
  install_stage_tracker()->ReportInfoOnNoUpdatesFailure(kExtensionId2, "");
  install_stage_tracker()->ReportFailure(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::ALREADY_INSTALLED);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kLoaded);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
}

// Regression test to check if the metrics are collected properly for the
// extensions which are in state |kReady|. Also verifies that the failure
// reported after |kReady| state is not reflected in the statistics.
TEST_F(ForceInstalledMetricsTest, ExtensionsReady) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kReady);
  install_stage_tracker()->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::ALREADY_INSTALLED);
  scoped_refptr<const Extension> ext2 = CreateNewExtension(
      kExtensionName2, kExtensionId2, ExtensionStatus::kReady);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kReadyTimeStats, 1);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
}

// Regression test to check if no metrics are reported for |kReady| state when
// some extensions are failed.
TEST_F(ForceInstalledMetricsTest, AllExtensionsNotReady) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kReady);
  install_stage_tracker()->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::INVALID_ID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectTotalCount(kLoadTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kReadyTimeStats, 0);
  histogram_tester_.ExpectBucketCount(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::INVALID_ID, 1);
}

// Verifies that the installation stage is not overwritten by a previous stage.
TEST_F(ForceInstalledMetricsTest,
       ExtensionsPreviousInstallationStageReportedAgain) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::CREATED);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::PENDING);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::CREATED);
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 1);
  histogram_tester_.ExpectBucketCount(kInstallationStages,
                                      InstallStageTracker::Stage::PENDING, 1);
}

// Verifies that the installation stage is overwritten if DOWNLOADING stage is
// reported again after INSTALLING stage.
TEST_F(ForceInstalledMetricsTest, ExtensionsDownloadingStageReportedAgain) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::INSTALLING);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId2, ExtensionDownloaderDelegate::Stage::PENDING);
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 1);
  histogram_tester_.ExpectBucketCount(
      kInstallationStages, InstallStageTracker::Stage::DOWNLOADING, 1);
}

TEST_F(ForceInstalledMetricsTest, ExtensionsStuck) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId1, InstallStageTracker::Stage::PENDING);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker()->ReportDownloadingStage(
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
      prefs()->GetManagedPref(pref_names::kInstallForceList)->GetDict().size(),
      1);
}

TEST_F(ForceInstalledMetricsTest, ExtensionStuckInCreatedStage) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::CREATED);
  install_stage_tracker()->ReportInstallCreationStage(
      kExtensionId2, InstallStageTracker::InstallCreationStage::
                         NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED);
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 1);
  histogram_tester_.ExpectUniqueSample(kInstallationStages,
                                       InstallStageTracker::Stage::CREATED, 1);
  histogram_tester_.ExpectUniqueSample(
      kInstallCreationStages,
      InstallStageTracker::InstallCreationStage::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_NOT_FORCED,
      1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ForceInstalledMetricsTest, ReportManagedGuestSessionOnExtensionFailure) {
  auto* fake_user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  const AccountId account_id =
      AccountId::FromUserEmail(profile()->GetProfileUserName());
  user_manager::User* user =
      fake_user_manager->AddPublicAccountUser(account_id);
  fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker()->ReportCrxInstallError(
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
  auto* fake_user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  user_manager::User* user = fake_user_manager->AddGuestUser();
  fake_user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker()->ReportCrxInstallError(
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

// Verified that the metrics related to user type are reported correctly for
// extension stuck in NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED stage.
TEST_F(ForceInstalledMetricsTest,
       ReportGuestSessionForExtensionsStuckInCreatedStage) {
  auto* fake_user_manager = new ash::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_user_manager(
      base::WrapUnique(fake_user_manager));
  user_manager::User* user = fake_user_manager->AddGuestUser();
  fake_user_manager->UserLoggedIn(user->GetAccountId(), user->username_hash(),
                                  false /* browser_restart */,
                                  false /* is_child */);

  SetupForceList(ExtensionOrigin::kWebStore);
  CreateExtensionService(/*extensions_enabled=*/true);

  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::CREATED);
  install_stage_tracker()->ReportInstallCreationStage(
      kExtensionId2, InstallStageTracker::InstallCreationStage::
                         NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED);

  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS, InstallStageTracker::FailureReason::IN_PROGRESS, 1);
  histogram_tester_.ExpectBucketCount(kInstallationStages,
                                      InstallStageTracker::Stage::CREATED, 1);
  histogram_tester_.ExpectBucketCount(
      kInstallCreationStages,
      InstallStageTracker::InstallCreationStage::
          NOTIFIED_FROM_MANAGEMENT_INITIAL_CREATION_FORCED,
      1);
  histogram_tester_.ExpectUniqueSample(kStuckInCreatedStageAreExtensionsEnabled,
                                       true, 1);
  histogram_tester_.ExpectBucketCount(
      kFailureSessionStats, ForceInstalledMetrics::UserType::USER_TYPE_GUEST,
      1);
  histogram_tester_.ExpectBucketCount(
      kStuckInCreateStageSessionType,
      ForceInstalledMetrics::UserType::USER_TYPE_GUEST, 1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ForceInstalledMetricsTest, ExtensionsAreDownloading) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId1, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId1, ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST);
  install_stage_tracker()->ReportInstallationStage(
      kExtensionId2, InstallStageTracker::Stage::DOWNLOADING);
  install_stage_tracker()->ReportDownloadingStage(
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
      prefs()->GetManagedPref(pref_names::kInstallForceList)->GetDict().size(),
      1);
}

// Error Codes in case of CRX_FETCH_FAILED for CWS extensions.
TEST_F(ForceInstalledMetricsTest, ExtensionCrxFetchFailedCWS) {
  SetupForceList(ExtensionOrigin::kWebStore);
  ExtensionDownloaderDelegate::FailureData data1(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId1, InstallStageTracker::FailureReason::CRX_FETCH_FAILED,
      data1);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId2, InstallStageTracker::FailureReason::CRX_FETCH_FAILED,
      data2);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeCrxFetchFailedStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeCrxFetchFailedStats,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeCrxFetchFailedStatsCWS,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeCrxFetchFailedStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesCrxFetchFailedStats,
                                      kFetchTries, 2);
}

// Error Codes in case of CRX_FETCH_FAILED for self hosted extension.
TEST_F(ForceInstalledMetricsTest, ExtensionCrxFetchFailedSelfHosted) {
  SetupForceList(ExtensionOrigin::kOffStore);
  ExtensionDownloaderDelegate::FailureData data1(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId1, InstallStageTracker::FailureReason::CRX_FETCH_FAILED,
      data1);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId2, InstallStageTracker::FailureReason::CRX_FETCH_FAILED,
      data2);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeCrxFetchFailedStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeCrxFetchFailedStats,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeCrxFetchFailedStatsSH,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeCrxFetchFailedStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesCrxFetchFailedStats,
                                      kFetchTries, 2);
}

// Error Codes in case of MANIFEST_FETCH_FAILED for CWS extensions.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestFetchFailedCWS) {
  SetupForceList(ExtensionOrigin::kWebStore);
  ExtensionDownloaderDelegate::FailureData data1(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId1, InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED,
      data1);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED,
      data2);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchFailedStats,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchFailedStatsCWS,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesManifestFetchFailedStats,
                                      kFetchTries, 2);
}

// Error Codes in case of MANIFEST_FETCH_FAILED for self hosted extensions. This
// test verifies that failure MANIFEST_FETCH_FAILED with 403 http error code
// (Forbidden) is considered as misconfiguration.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestFetchFailedSelfHosted) {
  SetupForceList(ExtensionOrigin::kOffStore);
  ExtensionDownloaderDelegate::FailureData data1(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId1, InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED,
      data1);
  ExtensionDownloaderDelegate::FailureData data2(
      -net::Error::ERR_INVALID_ARGUMENT, kFetchTries);
  install_stage_tracker()->ReportFetchError(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_FETCH_FAILED,
      data2);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchFailedStats,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchFailedStatsSH,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchFailedStats,
                                      -net::Error::ERR_INVALID_ARGUMENT, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesManifestFetchFailedStats,
                                      kFetchTries, 2);
}

// Error Codes in case of CWS extensions stuck in DOWNLOADING_MANIFEST_RETRY
// stage.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestFetchRetryCWS) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  ExtensionDownloaderDelegate::FailureData data(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId2,
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST_RETRY);
  install_stage_tracker()->ReportFetchErrorCodes(kExtensionId2, data);
  // ForceInstalledMetrics timer is still running as |kExtensionId2| is still in
  // progress.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchRetryStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchRetryStatsCWS,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesManifestFetchRetryStats,
                                      kFetchTries, 1);
}

// This test verifies that failure MANIFEST_INVALID in case of offstore
// extensions is considered as misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       OffStoreExtensionManifestInvalidIsMisconfiguration) {
  SetupForceList(ExtensionOrigin::kOffStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::MANIFEST_INVALID);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsSH, InstallStageTracker::FailureReason::MANIFEST_INVALID,
      1);
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// This test verifies that failure OVERRIDDEN_BY_SETTINGS in case of offstore
// extensions is considered as misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       ExtensionOverridenBySettingsFailureIsMisconfiguration) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
      kExtensionId2,
      InstallStageTracker::FailureReason::OVERRIDDEN_BY_SETTINGS);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectUniqueSample(
      kFailureReasonsCWS,
      InstallStageTracker::FailureReason::OVERRIDDEN_BY_SETTINGS, 1);
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Error Codes in case of self hosted extensions stuck in
// DOWNLOADING_MANIFEST_RETRY stage.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestFetchRetrySelfHosted) {
  SetupForceList(ExtensionOrigin::kOffStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  ExtensionDownloaderDelegate::FailureData data(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId2,
      ExtensionDownloaderDelegate::Stage::DOWNLOADING_MANIFEST_RETRY);
  install_stage_tracker()->ReportFetchErrorCodes(kExtensionId2, data);
  // ForceInstalledMetrics timer is still running as |kExtensionId2| is still in
  // progress.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeManifestFetchRetryStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeManifestFetchRetryStatsSH,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesManifestFetchRetryStats,
                                      kFetchTries, 1);
}

// Error Codes in case of CWS extensions stuck in DOWNLOADING_CRX_RETRY stage.
TEST_F(ForceInstalledMetricsTest, ExtensionCrxFetchRetryCWS) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  ExtensionDownloaderDelegate::FailureData data(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId2, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX_RETRY);
  install_stage_tracker()->ReportFetchErrorCodes(kExtensionId2, data);
  // ForceInstalledMetrics timer is still running as |kExtensionId2| is still in
  // progress.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeCrxFetchRetryStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeCrxFetchRetryStatsCWS,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesCrxFetchRetryStats,
                                      kFetchTries, 1);
}

// Error Codes in case of self hosted extensions stuck in DOWNLOADING_CRX_RETRY
// stage.
TEST_F(ForceInstalledMetricsTest, ExtensionCrxFetchRetrySelfHosted) {
  SetupForceList(ExtensionOrigin::kOffStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  ExtensionDownloaderDelegate::FailureData data(
      net::Error::OK, kHttpCodeForbidden, kFetchTries);
  install_stage_tracker()->ReportDownloadingStage(
      kExtensionId2, ExtensionDownloaderDelegate::Stage::DOWNLOADING_CRX_RETRY);
  install_stage_tracker()->ReportFetchErrorCodes(kExtensionId2, data);
  // ForceInstalledMetrics timer is still running as |kExtensionId2| is still in
  // progress.
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  histogram_tester_.ExpectBucketCount(kNetworkErrorCodeCrxFetchRetryStats,
                                      net::Error::OK, 1);
  histogram_tester_.ExpectBucketCount(kHttpErrorCodeCrxFetchRetryStatsSH,
                                      kHttpCodeForbidden, 1);
  histogram_tester_.ExpectBucketCount(kFetchRetriesCrxFetchRetryStats,
                                      kFetchTries, 1);
}

// Errors occurred because the fetched update manifest for webstore extension
// was invalid.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestInvalidWebStore) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportManifestInvalidFailure(
      kExtensionId2,
      ExtensionDownloaderDelegate::FailureData(
          ManifestInvalidError::INVALID_PROTOCOL_ON_GUPDATE_TAG));
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectUniqueSample(
      kWebStoreExtensionManifestInvalid,
      ManifestInvalidError::INVALID_PROTOCOL_ON_GUPDATE_TAG, 1);
}

// Errors occurred because the fetched update manifest for offstore extension
// was invalid.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestInvalidOffStore) {
  SetupForceList(ExtensionOrigin::kOffStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportManifestInvalidFailure(
      kExtensionId2,
      ExtensionDownloaderDelegate::FailureData(
          ManifestInvalidError::INVALID_PROTOCOL_ON_GUPDATE_TAG));
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectUniqueSample(
      kOffStoreExtensionManifestInvalid,
      ManifestInvalidError::INVALID_PROTOCOL_ON_GUPDATE_TAG, 1);
}

// Errors occurred because the fetched update manifest was invalid because app
// status was not OK. Verifies that this error with app status error as
// "error-unknownApplication" is considered as a misconfiguration.
TEST_F(ForceInstalledMetricsTest, ExtensionManifestInvalidAppStatusError) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportManifestInvalidFailure(
      kExtensionId2,
      ExtensionDownloaderDelegate::FailureData(
          ManifestInvalidError::BAD_APP_STATUS, "error-unknownApplication"));
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectUniqueSample(kWebStoreExtensionManifestInvalid,
                                       ManifestInvalidError::BAD_APP_STATUS, 1);
  histogram_tester_.ExpectUniqueSample(
      kExtensionManifestInvalidAppStatusError,
      InstallStageTracker::AppStatusError::kErrorUnknownApplication, 1);
  // Verify that the session with either all the extensions installed
  // successfully, or all failures as admin-side misconfigurations is recorded
  // here and BAD_APP_STATUS error is considered as a misconfiguration.
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 0,
                                      1);
}

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// CRX_INSTALL_ERROR with detailed error KIOSK_MODE_ONLY is considered as
// misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentKioskModeOnlyError) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportCrxInstallError(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  // Set TYPE_EXTENSION and TYPE_THEME as the allowed extension types.
  base::Value::List list =
      base::Value::List().Append("extension").Append("theme");
  prefs()->SetManagedPref(pref_names::kAllowedTypes, std::move(list));

  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  // Hosted app is not a valid extension type, so this should report an error.
  install_stage_tracker()->ReportExtensionType(kExtensionId2,
                                               Manifest::Type::TYPE_HOSTED_APP);
  install_stage_tracker()->ReportCrxInstallError(
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
  SetupForceList(ExtensionOrigin::kWebStore);

  // Set TYPE_EXTENSION and TYPE_THEME as the allowed extension types.
  base::Value::List list =
      base::Value::List().Append("extension").Append("theme");
  prefs()->SetManagedPref(pref_names::kAllowedTypes, std::move(list));

  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportExtensionType(kExtensionId2,
                                               Manifest::Type::TYPE_EXTENSION);
  install_stage_tracker()->ReportCrxInstallError(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportFailure(
      kExtensionId1, InstallStageTracker::FailureReason::INVALID_ID);
  install_stage_tracker()->ReportCrxInstallError(
      kExtensionId2,
      InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_DECLINED,
      CrxInstallErrorDetail::KIOSK_MODE_ONLY);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 1,
                                      1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// REPLACED_BY_ARC_APP is not considered as misconfiguration when ARC++ is
// enabled for the profile.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentReplacedByArcAppErrorArcEnabled) {
  // Enable ARC++ for this profile.
  prefs()->SetManagedPref(arc::prefs::kArcEnabled,
                          std::make_unique<base::Value>(true));
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
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
  prefs()->SetManagedPref(arc::prefs::kArcEnabled,
                          std::make_unique<base::Value>(false));
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
      kExtensionId2, InstallStageTracker::FailureReason::REPLACED_BY_ARC_APP);
  // ForceInstalledMetrics shuts down timer because all extension are either
  // loaded or failed.
  EXPECT_FALSE(fake_timer_->IsRunning());
  histogram_tester_.ExpectBucketCount(kPossibleNonMisconfigurationFailures, 1,
                                      1);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Session in which either all the extensions installed successfully, or all
// failures are admin-side misconfigurations. This test verifies that failure
// NOT_PERFORMING_NEW_INSTALL is considered as misconfiguration.
TEST_F(ForceInstalledMetricsTest,
       NonMisconfigurationFailureNotPresentNotPerformingNewInstallError) {
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportFailure(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportInfoOnNoUpdatesFailure(kExtensionId2, "");
  install_stage_tracker()->ReportFailure(
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
  SetupForceList(ExtensionOrigin::kWebStore);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kLoaded);
  install_stage_tracker()->ReportInfoOnNoUpdatesFailure(kExtensionId2,
                                                        "rate limit");
  install_stage_tracker()->ReportFailure(
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
  histogram_tester_.ExpectTotalCount(kReadyTimeStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutStats, 0);
  histogram_tester_.ExpectTotalCount(kTimedOutNotInstalledStats, 0);
  histogram_tester_.ExpectTotalCount(kFailureReasonsCWS, 0);
  histogram_tester_.ExpectTotalCount(kInstallationStages, 0);
  histogram_tester_.ExpectTotalCount(kFailureCrxInstallErrorStats, 0);
  histogram_tester_.ExpectTotalCount(kTotalCountStats, 0);
}

TEST_F(ForceInstalledMetricsTest, CachedExtensions) {
  SetupForceList(ExtensionOrigin::kWebStore);
  install_stage_tracker()->ReportDownloadingCacheStatus(
      kExtensionId1, ExtensionDownloaderDelegate::CacheStatus::CACHE_HIT);
  install_stage_tracker()->ReportDownloadingCacheStatus(
      kExtensionId2, ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS);
  scoped_refptr<const Extension> ext1 = CreateNewExtension(
      kExtensionName1, kExtensionId1, ExtensionStatus::kPending);
  registry()->AddEnabled(ext1.get());
  EXPECT_TRUE(fake_timer_->IsRunning());
  fake_timer_->Fire();
  // If an extension was installed successfully, don't mention it in statistics.
  histogram_tester_.ExpectUniqueSample(
      kInstallationFailureCacheStatus,
      ExtensionDownloaderDelegate::CacheStatus::CACHE_MISS, 1);
}

}  // namespace extensions
