// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "base/command_line.h"
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

constexpr const char kExtensionId[] = "aaaabbbbccccddddeeeeffffgggghhhh";
constexpr const char kExtensionName[] = "Test Extension";
constexpr const char kExtensionVersion[] = "1";
constexpr const char kScriptCode[] = "document.write('Hello World')";

}  // namespace

class ExtensionTelemetryServiceTest : public ::testing::Test {
 protected:
  ExtensionTelemetryServiceTest();
  void RegisterExtensionWithExtensionService(
      const extensions::ExtensionId& extension_id = kExtensionId,
      const std::string& extension_name = kExtensionName,
      const base::Time& install_time = base::Time::Now(),
      const std::string& version = kExtensionVersion);
  void PrimeTelemetryServiceWithSignal();

  void SetUp() override { ASSERT_NE(telemetry_service_, nullptr); }

  bool IsTelemetryServiceEnabled() {
    return telemetry_service_->enabled() &&
           !telemetry_service_->signal_processors_.empty() &&
           telemetry_service_->timer_.IsRunning();
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
  extensions::ExtensionService* extension_service_;
  extensions::ExtensionPrefs* extension_prefs_;
};

ExtensionTelemetryServiceTest::ExtensionTelemetryServiceTest()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  // Create telemetry service instance.
  profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(&profile_);

  // Create fake extension service instance.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  auto* test_extension_system = static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(&profile_));
  extension_service_ = test_extension_system->CreateExtensionService(
      &command_line, base::FilePath() /* install_directory */,
      false /* autoupdate_enabled */);

  // Create extension prefs instance.
  extension_prefs_ = extensions::ExtensionPrefs::Get(&profile_);

  // Register a test extension with the fake extension service.
  RegisterExtensionWithExtensionService();
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

void ExtensionTelemetryServiceTest::PrimeTelemetryServiceWithSignal() {
  // Verify that service is enabled.
  EXPECT_TRUE(IsTelemetryServiceEnabled());

  // Add a tabs.executeScript API invocation signal to the telemetry service.
  auto signal =
      std::make_unique<TabsExecuteScriptSignal>(kExtensionId, kScriptCode);
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
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(&profile_);
  EXPECT_FALSE(IsTelemetryServiceEnabled());

  // Re-enable ESB, service should become enabled.
  profile_.GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_TRUE(IsTelemetryServiceEnabled());
}

TEST_F(ExtensionTelemetryServiceTest, ProcessesSignal) {
  PrimeTelemetryServiceWithSignal();
  // Verify that the registered extension information is saved in the
  // telemetry service's extension store.
  const ExtensionInfo* info = GetExtensionInfoFromExtensionStore(kExtensionId);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->id(), kExtensionId);
  EXPECT_EQ(info->name(), kExtensionName);
  EXPECT_EQ(info->version(), kExtensionVersion);
  EXPECT_EQ(info->install_timestamp_msec(),
            extension_prefs_->GetInstallTime(kExtensionId).ToJavaTime());
}

TEST_F(ExtensionTelemetryServiceTest, GeneratesTelemetryReport) {
  PrimeTelemetryServiceWithSignal();
  // Verify the contents of telemetry report generated.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  ASSERT_EQ(telemetry_report_pb->reports_size(), 1);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().id(), kExtensionId);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().name(), kExtensionName);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().version(),
            kExtensionVersion);
  EXPECT_EQ(
      telemetry_report_pb->reports(0).extension().install_timestamp_msec(),
      extension_prefs_->GetInstallTime(kExtensionId).ToJavaTime());
  // Verify that extension store contents have been cleared after creating a
  // telemetry report.
  const ExtensionInfo* info = GetExtensionInfoFromExtensionStore(kExtensionId);
  EXPECT_EQ(info, nullptr);
}

TEST_F(ExtensionTelemetryServiceTest, GeneratesReportAtProperIntervals) {
  for (int i = 0; i < 2; i++) {
    PrimeTelemetryServiceWithSignal();
    {
      const ExtensionInfo* info =
          GetExtensionInfoFromExtensionStore(kExtensionId);
      EXPECT_NE(info, nullptr);
    }
    // Check that extension store still has extension info stored before
    // reporting interval elapses.
    base::TimeDelta interval = telemetry_service_->current_reporting_interval();
    task_environment_.FastForwardBy(interval - base::Seconds(1));
    {
      const ExtensionInfo* info =
          GetExtensionInfoFromExtensionStore(kExtensionId);
      EXPECT_NE(info, nullptr);
    }
    // Check that extension store is cleared after reporting interval elapses.
    task_environment_.FastForwardBy(base::Seconds(1));
    {
      const ExtensionInfo* info =
          GetExtensionInfoFromExtensionStore(kExtensionId);
      EXPECT_EQ(info, nullptr);
    }
  }
}

}  // namespace safe_browsing
