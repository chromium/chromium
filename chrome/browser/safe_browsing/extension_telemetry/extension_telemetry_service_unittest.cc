// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  void RegisterExtensionWithExtensionService(
      const extensions::ExtensionId& extension_id,
      const std::string& extension_name,
      const base::Time& install_time,
      const std::string& version);
  void UnregisterExtensionWithExtensionService(
      const extensions::ExtensionId& extension_id);
  void PrimeTelemetryServiceWithSignal();

  bool IsTelemetryServiceEnabled() {
    return telemetry_service_->enabled() &&
           !telemetry_service_->signal_processors_.empty() &&
           telemetry_service_->timer_.IsRunning();
  }

  bool IsExtensionStoreEmpty() {
    return telemetry_service_->extension_store_.empty();
  }

  using ExtensionInfo = ExtensionTelemetryReportRequest_ExtensionInfo;
  const ExtensionInfo* GetExtensionInfoFromExtensionStore(
      const extensions::ExtensionId& extension_id) {
    auto iter = telemetry_service_->extension_store_.find(extension_id);
    if (iter == telemetry_service_->extension_store_.end())
      return nullptr;
    return iter->second.get();
  }

  using TelemetryReport = ExtensionTelemetryReportRequest;
  std::unique_ptr<TelemetryReport> GetTelemetryReport() {
    return telemetry_service_->CreateReport();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<ExtensionTelemetryService> telemetry_service_;
  raw_ptr<extensions::ExtensionService> extension_service_;
  raw_ptr<extensions::ExtensionPrefs> extension_prefs_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
};

ExtensionTelemetryServiceTest::ExtensionTelemetryServiceTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  // Create extension prefs and registry instances.
  extension_prefs_ = extensions::ExtensionPrefs::Get(&profile_);
  extension_registry_ = extensions::ExtensionRegistry::Get(&profile_);

  // Create telemetry service instance.
  profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(
      &profile_, extension_registry_, extension_prefs_);

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
    const extensions::ExtensionId& extension_id,
    const std::string& extension_name,
    const base::Time& install_time,
    const std::string& version) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder()
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
      extension_id, "install_time",
      std::make_unique<base::Value>(
          base::NumberToString(install_time.ToJavaTime())));
}

void ExtensionTelemetryServiceTest::UnregisterExtensionWithExtensionService(
    const extensions::ExtensionId& extension_id) {
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
      &profile_, extension_registry_, extension_prefs_);
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
            extension_prefs_->GetInstallTime(kExtensionId[0]).ToJavaTime());
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
        extension_prefs_->GetInstallTime(kExtensionId[i]).ToJavaTime());
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
        extension_prefs_->GetInstallTime(kExtensionId[i]).ToJavaTime());
  }

  // Verify that first extension's report has signal data.
  EXPECT_EQ(telemetry_report_pb->reports(0).signals().size(), 1);
  // Verify that second extension's report has no signal data.
  EXPECT_EQ(telemetry_report_pb->reports(1).signals().size(), 0);

  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());
}

}  // namespace safe_browsing
