// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_uploader.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ExtensionId;
using extensions::mojom::ManifestLocation;

using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo;
using TelemetryReport = safe_browsing::ExtensionTelemetryReportRequest;

namespace safe_browsing {

namespace {

constexpr const char* kExtensionId[] = {"aaaaaaaabbbbbbbbccccccccdddddddd",
                                        "eeeeeeeeffffffffgggggggghhhhhhhh"};
constexpr const char* kExtensionName[] = {"Test Extension 1",
                                          "Test Extension 2"};
constexpr const char kExtensionVersion[] = "1";
constexpr const char kScriptCode[] = "document.write('Hello World')";

}  // namespace

class ExtensionTelemetryServiceTest : public ::testing::Test {
 protected:
  ExtensionTelemetryServiceTest();
  void SetUp() override { ASSERT_NE(telemetry_service_, nullptr); }

  void RegisterExtensionWithExtensionService(const ExtensionId& extension_id,
                                             const std::string& extension_name,
                                             const base::Time& install_time,
                                             const std::string& version);
  void UnregisterExtensionWithExtensionService(const ExtensionId& extension_id);
  void PrimeTelemetryServiceWithSignal();

  bool IsTelemetryServiceEnabled() {
    return telemetry_service_->enabled() &&
           !telemetry_service_->signal_processors_.empty() &&
           telemetry_service_->timer_.IsRunning();
  }

  bool IsExtensionStoreEmpty() {
    return telemetry_service_->extension_store_.empty();
  }

  const ExtensionInfo* GetExtensionInfoFromExtensionStore(
      const ExtensionId& extension_id) {
    auto iter = telemetry_service_->extension_store_.find(extension_id);
    if (iter == telemetry_service_->extension_store_.end())
      return nullptr;
    return iter->second.get();
  }

  std::unique_ptr<TelemetryReport> GetTelemetryReport() {
    return telemetry_service_->CreateReport();
  }

  std::unique_ptr<ExtensionInfo> GetExtensionInfo(const Extension& extension) {
    return telemetry_service_->GetExtensionInfoForReport(extension);
  }

  base::test::ScopedFeatureList scoped_feature_list;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ExtensionTelemetryService> telemetry_service_;
  raw_ptr<extensions::ExtensionService> extension_service_;
  raw_ptr<extensions::ExtensionPrefs> extension_prefs_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  base::TimeDelta kStartupUploadCheckDelaySeconds = base::Seconds(20);
};

ExtensionTelemetryServiceTest::ExtensionTelemetryServiceTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  // Create extension prefs and registry instances.
  extension_prefs_ = extensions::ExtensionPrefs::Get(&profile_);
  extension_registry_ = extensions::ExtensionRegistry::Get(&profile_);

  profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  // Create telemetry service instance.
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(
      &profile_, test_url_loader_factory_.GetSafeWeakWrapper(),
      extension_registry_, extension_prefs_);

  // Create fake extension service instance.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  auto* test_extension_system = static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(&profile_));
  extension_service_ = test_extension_system->CreateExtensionService(
      &command_line, base::FilePath() /* install_directory */,
      false /* autoupdate_enabled */);

  // Register test extensions with the extension service.
  RegisterExtensionWithExtensionService(kExtensionId[0], kExtensionName[0],
                                        base::Time::Now(), kExtensionVersion);
  RegisterExtensionWithExtensionService(kExtensionId[1], kExtensionName[1],
                                        base::Time::Now(), kExtensionVersion);
}

void ExtensionTelemetryServiceTest::RegisterExtensionWithExtensionService(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const base::Time& install_time,
    const std::string& version) {
  scoped_refptr<const extensions::Extension> extension =
      ExtensionBuilder()
          .SetID(extension_id)
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", extension_name)
                           .Set("version", version)
                           .Set("manifest_version", 2)
                           .Build())
          .Build();

  // Register the extension with the extension service.
  extension_service_->AddExtension(extension.get());

  extension_prefs_->UpdateExtensionPref(
      extension_id, "last_update_time",
      std::make_unique<base::Value>(
          base::NumberToString(install_time.ToJavaTime())));
}

void ExtensionTelemetryServiceTest::UnregisterExtensionWithExtensionService(
    const ExtensionId& extension_id) {
  extension_service_->UnloadExtension(
      extension_id, extensions::UnloadedExtensionReason::UNINSTALL);
  extension_prefs_->DeleteExtensionPrefs(extension_id);
}

void ExtensionTelemetryServiceTest::PrimeTelemetryServiceWithSignal() {
  // Verify that service is enabled.
  EXPECT_TRUE(IsTelemetryServiceEnabled());

  // Add a tabs.executeScript API invocation signal to the telemetry service.
  auto signal =
      std::make_unique<TabsExecuteScriptSignal>(kExtensionId[0], kScriptCode);
  telemetry_service_->AddSignal(std::move(signal));
}

TEST_F(ExtensionTelemetryServiceTest, IsEnabledOnlyWhenESBIsEnabled) {
  // Test fixture enables ESB and creates telemetry service.
  // Verify that service is enabled.
  EXPECT_TRUE(IsTelemetryServiceEnabled());

  // Disable ESB, service should become disabled.
  profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_FALSE(IsTelemetryServiceEnabled());

  // Destruct and restart service and verify that it starts disabled.
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(
      &profile_, test_url_loader_factory_.GetSafeWeakWrapper(),
      extension_registry_, extension_prefs_);
  EXPECT_FALSE(IsTelemetryServiceEnabled());

  // Re-enable ESB, service should become enabled.
  profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_TRUE(IsTelemetryServiceEnabled());
}

TEST_F(ExtensionTelemetryServiceTest, ProcessesSignal) {
  PrimeTelemetryServiceWithSignal();
  // Verify that the registered extension information is saved in the
  // telemetry service's extension store.
  const ExtensionInfo* info =
      GetExtensionInfoFromExtensionStore(kExtensionId[0]);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->id(), kExtensionId[0]);
  EXPECT_EQ(info->name(), kExtensionName[0]);
  EXPECT_EQ(info->version(), kExtensionVersion);
  EXPECT_EQ(info->install_timestamp_msec(),
            extension_prefs_->GetLastUpdateTime(kExtensionId[0]).ToJavaTime());
}

TEST_F(ExtensionTelemetryServiceTest, DiscardsInvalidSignal) {
  // Verify that service is enabled.
  EXPECT_TRUE(IsTelemetryServiceEnabled());

  // Add a tabs.executeScript API invocation signal to the telemetry service
  // with an invalid extension id.
  auto signal = std::make_unique<TabsExecuteScriptSignal>("", kScriptCode);
  telemetry_service_->AddSignal(std::move(signal));

  // Verify that the signal was not processed by checking that the extension
  // store is empty.
  EXPECT_TRUE(IsExtensionStoreEmpty());
}

TEST_F(ExtensionTelemetryServiceTest, GeneratesReportAtProperIntervals) {
  for (int i = 0; i < 2; i++) {
    PrimeTelemetryServiceWithSignal();
    {
      const ExtensionInfo* info =
          GetExtensionInfoFromExtensionStore(kExtensionId[0]);
      EXPECT_NE(info, nullptr);
    }
    // Check that extension store still has extension info stored before
    // reporting interval elapses.
    base::TimeDelta interval = telemetry_service_->current_reporting_interval();
    profile_.GetPrefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
                                 base::Time::NowFromSystemTime());
    task_environment_.FastForwardBy(interval - base::Seconds(1));
    {
      const ExtensionInfo* info =
          GetExtensionInfoFromExtensionStore(kExtensionId[0]);
      EXPECT_NE(info, nullptr);
    }
    // Check that extension store is cleared after reporting interval elapses.
    task_environment_.FastForwardBy(base::Seconds(1));
    {
      const ExtensionInfo* info =
          GetExtensionInfoFromExtensionStore(kExtensionId[0]);
      EXPECT_EQ(info, nullptr);
    }
  }
}

TEST_F(ExtensionTelemetryServiceTest, DoesNotGenerateEmptyTelemetryReport) {
  // Check that telemetry service does not generate a telemetry report
  // when there are no signals and no installed extensions.
  UnregisterExtensionWithExtensionService(kExtensionId[0]);
  UnregisterExtensionWithExtensionService(kExtensionId[1]);
  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());

  // Verify that no telemetry report is generated.
  EXPECT_FALSE(GetTelemetryReport());
}

TEST_F(ExtensionTelemetryServiceTest, GeneratesTelemetryReportWithNoSignals) {
  // Check that telemetry service generates a telemetry report even when
  // there are no signals to report. The report consists of only extension
  // info for the installed extensions.
  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  // Verify the contents of telemetry report generated.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  // Telemetry report should contain reports for both test extensions.
  ASSERT_EQ(telemetry_report_pb->reports_size(), 2);
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(telemetry_report_pb->reports(i).extension().id(),
              kExtensionId[i]);
    EXPECT_EQ(telemetry_report_pb->reports(i).extension().name(),
              kExtensionName[i]);
    EXPECT_EQ(telemetry_report_pb->reports(i).extension().version(),
              kExtensionVersion);
    EXPECT_EQ(
        telemetry_report_pb->reports(i).extension().install_timestamp_msec(),
        extension_prefs_->GetLastUpdateTime(kExtensionId[i]).ToJavaTime());
    // Verify that there is no signal data associated with the extension.
    EXPECT_EQ(telemetry_report_pb->reports(i).signals().size(), 0);
  }

  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());
}

TEST_F(ExtensionTelemetryServiceTest, GeneratesTelemetryReportWithSignal) {
  PrimeTelemetryServiceWithSignal();
  // Verify the contents of telemetry report generated.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  // Telemetry report should contain reports for both test extensions.
  ASSERT_EQ(telemetry_report_pb->reports_size(), 2);
  for (int i = 0; i < 2; i++) {
    EXPECT_EQ(telemetry_report_pb->reports(i).extension().id(),
              kExtensionId[i]);
    EXPECT_EQ(telemetry_report_pb->reports(i).extension().name(),
              kExtensionName[i]);
    EXPECT_EQ(telemetry_report_pb->reports(i).extension().version(),
              kExtensionVersion);
    EXPECT_EQ(
        telemetry_report_pb->reports(i).extension().install_timestamp_msec(),
        extension_prefs_->GetLastUpdateTime(kExtensionId[i]).ToJavaTime());
  }

  // Verify that first extension's report has signal data.
  EXPECT_EQ(telemetry_report_pb->reports(0).signals().size(), 1);
  // Verify that second extension's report has no signal data.
  EXPECT_EQ(telemetry_report_pb->reports(1).signals().size(), 0);

  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());
}

TEST_F(ExtensionTelemetryServiceTest, TestExtensionInfoProtoConstruction) {
  // Clear out registered extensions first.
  UnregisterExtensionWithExtensionService(kExtensionId[0]);
  UnregisterExtensionWithExtensionService(kExtensionId[1]);

  auto add_extension = [this](const Extension* extension) {
    extension_prefs_->OnExtensionInstalled(
        extension, Extension::ENABLED, syncer::StringOrdinal(), std::string());
  };

  // Test basic prototype construction. All fields should be present, except
  // disable reasons (which should be empty).
  {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("test")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    std::unique_ptr<ExtensionInfo> extension_pb = GetExtensionInfo(*extension);
    EXPECT_TRUE(extension_pb->has_type());
    EXPECT_EQ(extension_pb->type(), ExtensionInfo::EXTENSION);

    EXPECT_TRUE(extension_pb->has_install_location());
    EXPECT_EQ(extension_pb->install_location(), ExtensionInfo::INTERNAL);

    EXPECT_TRUE(extension_pb->has_is_converted_from_user_script());
    EXPECT_FALSE(extension_pb->is_converted_from_user_script());

    EXPECT_TRUE(extension_pb->has_is_default_installed());
    EXPECT_FALSE(extension_pb->is_default_installed());

    EXPECT_TRUE(extension_pb->has_is_oem_installed());
    EXPECT_FALSE(extension_pb->is_oem_installed());

    EXPECT_TRUE(extension_pb->has_is_from_store());
    EXPECT_FALSE(extension_pb->is_from_store());

    EXPECT_TRUE(extension_pb->has_updates_from_store());
    EXPECT_FALSE(extension_pb->updates_from_store());

    EXPECT_TRUE(extension_pb->has_blocklist_state());
    EXPECT_EQ(extension_pb->blocklist_state(), ExtensionInfo::NOT_BLOCKLISTED);

    EXPECT_TRUE(extension_pb->has_disable_reasons());
    EXPECT_EQ(extension_pb->disable_reasons(), static_cast<uint32_t>(0));
  }

  // It's not helpful to exhaustively test each possible variation of each
  // field in the ExtensionInfo proto (since in many cases the test code would
  // then be re-writing the original code), but we test a few of the more
  // interesting cases.
  {
    // Test the type field; extensions of different types should be reported
    // as such.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("app", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    std::unique_ptr<ExtensionInfo> extension_pb = GetExtensionInfo(*extension);
    EXPECT_EQ(extension_pb->type(), ExtensionInfo::PLATFORM_APP);
  }

  {
    // Test the install location.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("unpacked")
            .SetLocation(ManifestLocation::kUnpacked)
            .Build();
    add_extension(extension.get());
    std::unique_ptr<ExtensionInfo> extension_pb = GetExtensionInfo(*extension);
    EXPECT_EQ(extension_pb->install_location(), ExtensionInfo::UNPACKED);
  }

  {
    // Test the disable reasons field.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("disable_reasons")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    extension_prefs_->SetExtensionDisabled(
        extension->id(), extensions::disable_reason::DISABLE_USER_ACTION);
    {
      std::unique_ptr<ExtensionInfo> extension_pb =
          GetExtensionInfo(*extension);
      EXPECT_TRUE(extension_pb->has_disable_reasons());
      EXPECT_EQ(extension_pb->disable_reasons(),
                static_cast<uint32_t>(
                    extensions::disable_reason::DISABLE_USER_ACTION));
    }
    // Adding additional disable reasons should result in all reasons being
    // reported.
    extension_prefs_->AddDisableReason(
        extension->id(), extensions::disable_reason::DISABLE_CORRUPTED);
    {
      std::unique_ptr<ExtensionInfo> extension_pb =
          GetExtensionInfo(*extension);
      EXPECT_TRUE(extension_pb->has_disable_reasons());
      EXPECT_EQ(extension_pb->disable_reasons(),
                static_cast<uint32_t>(
                    extensions::disable_reason::DISABLE_USER_ACTION |
                    extensions::disable_reason::DISABLE_CORRUPTED));
    }
  }

  {
    // Test changing the blocklist state.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("blocklist")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    extensions::blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
        extension->id(),
        extensions::BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY,
        extension_prefs_);
    std::unique_ptr<ExtensionInfo> extension_pb = GetExtensionInfo(*extension);
    EXPECT_EQ(extension_pb->blocklist_state(),
              ExtensionInfo::BLOCKLISTED_SECURITY_VULNERABILITY);
  }
}

TEST_F(ExtensionTelemetryServiceTest,
       PersistsReportsOnShutdownWithSignalDataPresent) {
  // Setting up the persister and signals.
  telemetry_service_->SetEnabled(false);
  scoped_feature_list.InitAndEnableFeature(kExtensionTelemetryPersistence);
  telemetry_service_->SetEnabled(true);
  PrimeTelemetryServiceWithSignal();
  task_environment_.RunUntilIdle();
  // After a shutdown, the persister should create a file of persisted data.
  telemetry_service_->Shutdown();
  base::FilePath persisted_dir = profile_.GetPath();
  persisted_dir = persisted_dir.AppendASCII("CRXTelemetry");
  base::FilePath persisted_file = persisted_dir.AppendASCII("CRXTelemetry_0");
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(base::PathExists(persisted_file));
  // After the telemetry service is disabled, the persisted data folder should
  // be deleted.
  telemetry_service_->SetEnabled(false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(persisted_dir));
}

TEST_F(ExtensionTelemetryServiceTest,
       DoesNotPersistsReportsOnShutdownWithNoSignalDataPresent) {
  // Setting up the persister and signals.
  telemetry_service_->SetEnabled(false);
  scoped_feature_list.InitAndEnableFeature(kExtensionTelemetryPersistence);
  telemetry_service_->SetEnabled(true);
  task_environment_.RunUntilIdle();
  // After a shutdown, the persister should not persist a file. There are
  // extensions installed but there is no signal data present.
  telemetry_service_->Shutdown();
  base::FilePath persisted_dir = profile_.GetPath();
  persisted_dir = persisted_dir.AppendASCII("CRXTelemetry");
  base::FilePath persisted_file = persisted_dir.AppendASCII("CRXTelemetry_0");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(persisted_file));
}

TEST_F(ExtensionTelemetryServiceTest, PersistsReportOnFailedUpload) {
  // Setting up the persister, signals, upload/write intervals, and the
  // uploader itself.
  telemetry_service_->SetEnabled(false);
  scoped_feature_list.InitAndEnableFeature(kExtensionTelemetryPersistence);
  telemetry_service_->SetEnabled(true);
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  profile_.GetPrefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
                               base::Time::NowFromSystemTime());
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), "Dummy",
      net::HTTP_BAD_REQUEST);
  // Fast forward a reporting interval, there should be one file after the
  // failed upload.
  task_environment_.FastForwardBy(interval);
  task_environment_.RunUntilIdle();
  base::FilePath persisted_dir = profile_.GetPath();
  persisted_dir = persisted_dir.AppendASCII("CRXTelemetry");
  EXPECT_TRUE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_0")));
}

TEST_F(ExtensionTelemetryServiceTest, NoReportPersistedIfUploadSucceeds) {
  // With the default NumberWritesInInterval=1, the persisting interval is the
  // same as the reporting interval. At each interval, the in-memory data
  // is used to create a report which is then uploaded. If the upload succeeds,
  // there is no need to persist anything.
  telemetry_service_->SetEnabled(false);
  scoped_feature_list.InitAndEnableFeature(kExtensionTelemetryPersistence);
  telemetry_service_->SetEnabled(true);
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  profile_.GetPrefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
                               base::Time::NowFromSystemTime());
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), "Dummy", net::HTTP_OK);
  // Fast forward a reporting interval, there should be no files persisted after
  // the upload.
  task_environment_.FastForwardBy(interval);
  task_environment_.RunUntilIdle();
  base::FilePath persisted_dir = profile_.GetPath();
  persisted_dir = persisted_dir.AppendASCII("CRXTelemetry");
  EXPECT_FALSE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_0")));
}

TEST_F(ExtensionTelemetryServiceTest, PersistsReportsOnInterval) {
  // Setting up the persister, signals, upload/write intervals, and the
  // uploader itself.
  telemetry_service_->SetEnabled(false);
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kExtensionTelemetry, {{"NumberOfWritesInInterval", "4"}}},
       {kExtensionTelemetryPersistence, {}}},
      {});
  telemetry_service_->SetEnabled(true);
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  profile_.GetPrefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
                               base::Time::NowFromSystemTime());
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), "Dummy", net::HTTP_OK);
  // Fast forward a (reporting interval - 1) seconds which is three write
  // intervals. There should be three files on disk now.
  task_environment_.FastForwardBy(interval - base::Seconds(1));
  task_environment_.RunUntilIdle();
  base::FilePath persisted_dir = profile_.GetPath();
  persisted_dir = persisted_dir.AppendASCII("CRXTelemetry");
  EXPECT_TRUE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_0")));
  EXPECT_TRUE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_1")));
  EXPECT_TRUE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_2")));
  // After a full reporting interval, check that four reports were uploaded,
  // the active report and the three persisted files. The cache should be empty.
  task_environment_.FastForwardBy(base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(test_url_loader_factory_.total_requests(),
            /*num_uploaded_reports=*/static_cast<size_t>(4));
  EXPECT_FALSE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_0")));
  EXPECT_FALSE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_1")));
  EXPECT_FALSE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_2")));
}

TEST_F(ExtensionTelemetryServiceTest, MalformedPersistedFile) {
  // Setting up the persister, signals, upload/write intervals, and the
  // uploader itself.
  telemetry_service_->SetEnabled(false);
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{kExtensionTelemetry, {{"NumberOfWritesInInterval", "4"}}},
       {kExtensionTelemetryPersistence, {}}},
      {});
  telemetry_service_->SetEnabled(true);
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  profile_.GetPrefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
                               base::Time::NowFromSystemTime());
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), "Dummy", net::HTTP_OK);
  // Fast forward a (reporting interval - 1) seconds which is three write
  // intervals. There should be three files on disk now.
  task_environment_.FastForwardBy(interval - base::Seconds(1));
  task_environment_.RunUntilIdle();
  base::FilePath persisted_dir = profile_.GetPath();
  persisted_dir = persisted_dir.AppendASCII("CRXTelemetry");
  EXPECT_TRUE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_0")));
  EXPECT_TRUE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_1")));
  EXPECT_TRUE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_2")));
  // Write to one of the files, malforming the protobuf.
  base::WriteFile(persisted_dir.AppendASCII("CRXTelemetry_1"), "BAD FILE");
  // After a full reporting interval 3 reports should be uploaded, the current
  // report in memory plus 2 properly formatted files on disk. The Bad File
  // should have been deleted and not uploaded.
  task_environment_.FastForwardBy(base::Seconds(1));
  task_environment_.RunUntilIdle();
  // Check to make sure the in-memory report and the 2 properly formatted files
  // were uploaded.
  EXPECT_EQ(test_url_loader_factory_.total_requests(),
            /*num_uploaded_reports=*/static_cast<size_t>(3));
  EXPECT_FALSE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_0")));
  EXPECT_FALSE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_1")));
  EXPECT_FALSE(base::PathExists(persisted_dir.AppendASCII("CRXTelemetry_2")));
}

TEST_F(ExtensionTelemetryServiceTest, StartupUploadCheck) {
  // Setting up the persister, signals, upload/write intervals, and the
  // uploader itself.
  telemetry_service_->SetEnabled(false);
  scoped_feature_list.InitAndEnableFeature(kExtensionTelemetryPersistence);
  telemetry_service_->SetEnabled(true);
  task_environment_.RunUntilIdle();
  profile_.GetPrefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
                               base::Time::NowFromSystemTime());
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), "Dummy", net::HTTP_OK);
  // Take the telemetry service offline and fast forward the environment
  // by a whole upload interval.
  telemetry_service_->SetEnabled(false);
  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  telemetry_service_->SetEnabled(true);
  task_environment_.RunUntilIdle();
  PrimeTelemetryServiceWithSignal();
  task_environment_.FastForwardBy(kStartupUploadCheckDelaySeconds);
  task_environment_.RunUntilIdle();
  // The startup check should empty the extension store data.
  {
    const ExtensionInfo* info =
        GetExtensionInfoFromExtensionStore(kExtensionId[0]);
    EXPECT_EQ(info, nullptr);
  }
}
TEST_F(ExtensionTelemetryServiceTest, PersisterThreadSafetyCheck) {
  scoped_feature_list.InitAndEnableFeature(kExtensionTelemetryPersistence);
  std::unique_ptr<ExtensionTelemetryService> telemetry_service_2 =
      std::make_unique<ExtensionTelemetryService>(
          &profile_, test_url_loader_factory_.GetSafeWeakWrapper(),
          extension_registry_, extension_prefs_);
  telemetry_service_2->SetEnabled(true);
  telemetry_service_2.reset();
}
}  // namespace safe_browsing
