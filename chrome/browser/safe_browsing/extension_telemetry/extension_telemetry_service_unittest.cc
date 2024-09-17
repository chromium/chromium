// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_uploader.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/switches.h"
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
using ExtensionTelemetryReportResponse =
    safe_browsing::ExtensionTelemetryReportResponse;
using OffstoreExtensionVerdict =
    ::safe_browsing::ExtensionTelemetryReportResponse_OffstoreExtensionVerdict;
using ::extensions::ExtensionService;

namespace safe_browsing {

namespace {

constexpr auto kExtensionId = std::to_array(
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
     "cccccccccccccccccccccccccccccccc", "dddddddddddddddddddddddddddddddd",
     "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"});
constexpr auto kExtensionName =
    std::to_array({"Test Extension 0", "Test Extension 1", "Test Extension 2",
                   "Test Extension 3", "Test Extension 4"});
constexpr const char kExtensionVersion[] = "1";
constexpr const char kTestUpdateUrl[] = "http://example.com/update_url";
constexpr const char kInstallationMode[] = "installation_mode";
constexpr const char kUpdateUrl[] = "update_url";
constexpr const char kScriptCode[] = "document.write('Hello World')";
constexpr const char kCookieName[] = "cookie-1";
constexpr const char kCookieStoreId[] = "store-1";
constexpr const char kCookieURL[] = "http://www.example1.com/";
// Size of extension ids are 33 bytes.
constexpr const int kMinReportSize = 32;

constexpr char kFileDataProcessTimestampPref[] = "last_processed_timestamp";
constexpr char kFileDataDictPref[] = "file_data";

constexpr char kManifestFile[] = "manifest.json";
constexpr char kJavaScriptFile[] = "js_file.js";

}  // namespace

class ExtensionTelemetryServiceTest : public ::testing::Test {
 protected:
  explicit ExtensionTelemetryServiceTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  void SetUp() override {
    ASSERT_NE(telemetry_service_, nullptr);
    ASSERT_TRUE(extensions_root_dir_.CreateUniqueTempDir());

    // Register test extensions with the extension service.
    RegisterExtensionWithExtensionService(kExtensionId[0], kExtensionName[0],
                                          ManifestLocation::kUnpacked,
                                          Extension::NO_FLAGS);
    RegisterExtensionWithExtensionService(kExtensionId[1], kExtensionName[1],
                                          ManifestLocation::kUnpacked,
                                          Extension::NO_FLAGS);
  }

  base::FilePath CreateExtensionForCommandLineLoad(
      const std::string& extension_name);
  void RegisterExtensionWithExtensionService(const ExtensionId& extension_id,
                                             const std::string& extension_name,
                                             const ManifestLocation& location,
                                             int flags);
  void UnregisterExtensionWithExtensionService(const ExtensionId& extension_id);
  void PrimeTelemetryServiceWithSignal();

  bool IsTelemetryServiceEnabled() {
    return IsTelemetryServiceEnabledForESB() ||
           IsTelemetryServiceEnabledForEnterprise();
  }

  bool IsTelemetryServiceEnabledForESB() {
    return telemetry_service_->esb_enabled_ &&
           !telemetry_service_->signal_processors_.empty() &&
           telemetry_service_->timer_.IsRunning();
  }

  bool IsTelemetryServiceEnabledForEnterprise() {
    return telemetry_service_->enterprise_enabled_ &&
           !telemetry_service_->enterprise_signal_processors_.empty() &&
           telemetry_service_->enterprise_timer_.IsRunning();
  }

  bool IsExtensionStoreEmpty() {
    return telemetry_service_->extension_store_.empty();
  }

  bool IsEnterpriseExtensionStoreEmpty() {
    return telemetry_service_->enterprise_extension_store_.empty();
  }

  const ExtensionInfo* GetExtensionInfoFromExtensionStore(
      const ExtensionId& extension_id) {
    auto iter = telemetry_service_->extension_store_.find(extension_id);
    if (iter == telemetry_service_->extension_store_.end()) {
      return nullptr;
    }
    return iter->second.get();
  }

  const ExtensionInfo* GetExtensionInfoFromEnterpriseExtensionStore(
      const ExtensionId& extension_id) {
    auto iter =
        telemetry_service_->enterprise_extension_store_.find(extension_id);
    if (iter == telemetry_service_->enterprise_extension_store_.end()) {
      return nullptr;
    }
    return iter->second.get();
  }

  std::unique_ptr<TelemetryReport> GetTelemetryReport() {
    return telemetry_service_->CreateReport();
  }

  std::unique_ptr<TelemetryReport> GetTelemetryReportForEnterprise() {
    return telemetry_service_->CreateReportForEnterprise();
  }

  std::unique_ptr<ExtensionInfo> GetExtensionInfo(const Extension& extension) {
    return telemetry_service_->GetExtensionInfoForReport(extension);
  }

  PrefService* prefs() { return profile_.GetPrefs(); }

  void TearDown() override {
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        &profile_)
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
  }

  // Test directory that serves as the root directory for all test extension
  // files.
  base::ScopedTempDir extensions_root_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ExtensionTelemetryService> telemetry_service_;
  raw_ptr<extensions::ExtensionService> extension_service_;
  raw_ptr<extensions::ExtensionPrefs> extension_prefs_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  std::unique_ptr<policy::MockCloudPolicyClient> cloud_policy_client_;
  base::TimeDelta kStartupUploadCheckDelaySeconds = base::Seconds(20);
};

ExtensionTelemetryServiceTest::ExtensionTelemetryServiceTest(
    base::test::TaskEnvironment::TimeSource time_source)
    : task_environment_{time_source} {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kExtensionTelemetryDisableOffstoreExtensions,
                            kExtensionTelemetryFileDataForCommandLineExtensions,
                            kExtensionTelemetryForEnterprise},
      /*disabled_features=*/{});

  // Create extension prefs and registry instances.
  extension_prefs_ = extensions::ExtensionPrefs::Get(&profile_);
  extension_registry_ = extensions::ExtensionRegistry::Get(&profile_);

  // Set up and enable ESB telemetry reporting by default.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);

  // Set up and disable enteprise telemetry reporting by default.
  policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm-token"));
  cloud_policy_client_ = std::make_unique<policy::MockCloudPolicyClient>();
  cloud_policy_client_->SetDMToken("dm-token");
  enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
      ->SetTestingFactory(&profile_,
                          base::BindRepeating(&BuildRealtimeReportingClient));
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
      &profile_)
      ->SetBrowserCloudPolicyClientForTesting(cloud_policy_client_.get());
  enterprise_connectors::test::SetOnSecurityEventReporting(/*prefs=*/prefs(),
                                                           /*enabled=*/false);

  // Create fake extension service instance.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  auto* test_extension_system = static_cast<extensions::TestExtensionSystem*>(
      extensions::ExtensionSystem::Get(&profile_));
  extension_service_ = test_extension_system->CreateExtensionService(
      &command_line, base::FilePath() /* install_directory */,
      false /* autoupdate_enabled */);

  // Create telemetry service instance.
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(
      &profile_, test_url_loader_factory_.GetSafeWeakWrapper());
}

base::FilePath ExtensionTelemetryServiceTest::CreateExtensionForCommandLineLoad(
    const std::string& extension_name) {
  // Create extension path.
  base::FilePath path =
      extensions_root_dir_.GetPath().AppendASCII(extension_name);
  CreateDirectory(path);

  scoped_refptr<const extensions::Extension> extension =
      ExtensionBuilder(extension_name)
          .SetLocation(ManifestLocation::kCommandLine)
          .SetPath(path)
          .SetID(crx_file::id_util::GenerateIdForPath(path))
          .Build();

  // Write extension files.
  EXPECT_TRUE(
      base::WriteFile(path.AppendASCII(kJavaScriptFile), kJavaScriptFile));
  base::FilePath manifest_path = path.AppendASCII(kManifestFile);
  JSONFileValueSerializer(manifest_path)
      .Serialize(*extension->manifest()->value());
  EXPECT_TRUE(base::PathExists(manifest_path));

  // Set a dummy install time in extension prefs - this mimics the install
  // timestamp stored from a previous install (eg. when ESB was disabled).
  // We use this value to check that the telemetry report ignores previous
  // install times for command-line extensions and instead explicitly sets
  // it to 0 (to reflect the fact the extension is not really installed).
  extension_prefs_->UpdateExtensionPref(
      extension->id(), "last_update_time",
      base::Value(base::TimeToValue(base::Time::Now())));
  return path;
}

void ExtensionTelemetryServiceTest::RegisterExtensionWithExtensionService(
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const ManifestLocation& location,
    int flags) {
  // Create extension path.
  base::FilePath path = extensions_root_dir_.GetPath()
                            .AppendASCII(extension_id)
                            .AppendASCII(kExtensionVersion);
  base::CreateDirectory(path);

  scoped_refptr<const extensions::Extension> extension =
      ExtensionBuilder()
          .SetID(extension_id)
          .SetPath(path)
          .SetLocation(location)
          .SetManifest(base::Value::Dict()
                           .Set("name", extension_name)
                           .Set("version", kExtensionVersion)
                           .Set("manifest_version", 2))
          .AddFlags(flags)
          .Build();

  // Write extension files.
  EXPECT_TRUE(
      base::WriteFile(path.AppendASCII(kJavaScriptFile), kJavaScriptFile));
  base::FilePath manifest_path = path.AppendASCII(kManifestFile);
  JSONFileValueSerializer(manifest_path)
      .Serialize(*extension->manifest()->value());
  EXPECT_TRUE(base::PathExists(manifest_path));

  // Register the extension with the extension service.
  extension_service_->AddExtension(extension.get());

  extension_prefs_->UpdateExtensionPref(
      extension_id, "last_update_time",
      base::Value(base::TimeToValue(base::Time::Now())));
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

  // Add a cookies.get API invocation signal to the telemetry service.
  auto signal = std::make_unique<CookiesGetSignal>(kExtensionId[0], kCookieName,
                                                   kCookieStoreId, kCookieURL);
  telemetry_service_->AddSignal(std::move(signal));
}

TEST_F(ExtensionTelemetryServiceTest, CheckEnableConditionsForESB) {
  // Test fixture enables ESB and creates telemetry service.
  // Verify that service is enabled for ESB users.
  EXPECT_TRUE(IsTelemetryServiceEnabledForESB());

  // Disable ESB. Verify that ESB reporting is disabled.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_FALSE(IsTelemetryServiceEnabledForESB());

  // Destruct and restart service and verify that it starts disabled for ESB
  // users.
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(
      &profile_, test_url_loader_factory_.GetSafeWeakWrapper());
  EXPECT_FALSE(IsTelemetryServiceEnabledForESB());

  // Re-enable ESB. Verify that ESB reporting is enabled.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_TRUE(IsTelemetryServiceEnabledForESB());
}

TEST_F(ExtensionTelemetryServiceTest, CheckEnableConditionsForEnterprise) {
  // Test fixture disables enterprise policy for telemetry reports and creates
  // telemetry service. Verify that enterprise reporting is disabled.
  EXPECT_FALSE(IsTelemetryServiceEnabledForEnterprise());

  // Enable enterprise policy. Verify that enterprise reporting is enabled.
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});
  EXPECT_TRUE(IsTelemetryServiceEnabledForEnterprise());

  // Destruct and restart service and verify that it starts enabled.
  telemetry_service_ = std::make_unique<ExtensionTelemetryService>(
      &profile_, test_url_loader_factory_.GetSafeWeakWrapper());
  EXPECT_TRUE(IsTelemetryServiceEnabledForEnterprise());

  // Disable enterprise policy. Verify that enterprise reporting is disabled.
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/false,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{});
  EXPECT_FALSE(IsTelemetryServiceEnabledForEnterprise());
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
            extension_prefs_->GetLastUpdateTime(kExtensionId[0])
                .InMillisecondsSinceUnixEpoch());
}

TEST_F(ExtensionTelemetryServiceTest, ProcessesSignalForEnterprise) {
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});
  PrimeTelemetryServiceWithSignal();
  // Verify that the registered extension information is saved in the
  // telemetry service's enterprise extension store.
  const ExtensionInfo* info =
      GetExtensionInfoFromEnterpriseExtensionStore(kExtensionId[0]);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->id(), kExtensionId[0]);
  EXPECT_EQ(info->name(), kExtensionName[0]);
  EXPECT_EQ(info->version(), kExtensionVersion);
  EXPECT_EQ(info->install_timestamp_msec(),
            extension_prefs_->GetLastUpdateTime(kExtensionId[0])
                .InMillisecondsSinceUnixEpoch());
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
    prefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
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

TEST_F(ExtensionTelemetryServiceTest,
       DoesNotGenerateEmptyTelemetryReportForEnterprise) {
  // Enable enterprise policy.
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/
      {enterprise_connectors::kExtensionTelemetryEvent},
      /*enabled_opt_in_events=*/{});

  // Check that telemetry service does not generate a telemetry report for
  // enterprise when there are no signals.
  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());

  // Verify that no telemetry report is generated.
  EXPECT_FALSE(GetTelemetryReportForEnterprise());
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
        extension_prefs_->GetLastUpdateTime(kExtensionId[i])
            .InMillisecondsSinceUnixEpoch());
    // Verify that there is no signal data associated with the extension.
    EXPECT_EQ(telemetry_report_pb->reports(i).signals().size(), 0);
  }

  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());
}

TEST_F(ExtensionTelemetryServiceTest,
       GeneratesTelemetryReportWithSignalForESBOnly) {
  // Enable ESB, disable enterprise.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  enterprise_connectors::test::SetOnSecurityEventReporting(/*prefs=*/prefs(),
                                                           /*enabled=*/false);
  PrimeTelemetryServiceWithSignal();

  // Since enterprise is disabled and no signals is added for enterprise, verify
  // that enterprise extension store is empty and no enterprise report is
  // generated.
  EXPECT_TRUE(IsEnterpriseExtensionStoreEmpty());
  EXPECT_FALSE(GetTelemetryReportForEnterprise());

  // Verify the contents of telemetry report generated for ESB.
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
        extension_prefs_->GetLastUpdateTime(kExtensionId[i])
            .InMillisecondsSinceUnixEpoch());
  }

  // Verify that first extension's report has signal data.
  EXPECT_EQ(telemetry_report_pb->reports(0).signals().size(), 1);
  // Verify that second extension's report has no signal data.
  EXPECT_EQ(telemetry_report_pb->reports(1).signals().size(), 0);

  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());
}

TEST_F(ExtensionTelemetryServiceTest,
       GeneratesTelemetryReportWithSignalForEnterpriseOnly) {
  // Disable ESB, enable enterprise.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});
  PrimeTelemetryServiceWithSignal();

  // Since ESB is disabled, verify that extension store is empty and no ESB
  // report is generated.
  EXPECT_TRUE(IsExtensionStoreEmpty());
  EXPECT_FALSE(GetTelemetryReport());

  // Verify the contents of telemetry report generated for enterprise.
  std::unique_ptr<TelemetryReport> telemetry_report_pb =
      GetTelemetryReportForEnterprise();

  ASSERT_NE(telemetry_report_pb, nullptr);
  // Telemetry report should contain reports for extension 0 only.
  ASSERT_EQ(telemetry_report_pb->reports_size(), 1);

  EXPECT_EQ(telemetry_report_pb->reports(0).extension().id(), kExtensionId[0]);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().name(),
            kExtensionName[0]);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().version(),
            kExtensionVersion);
  EXPECT_EQ(
      telemetry_report_pb->reports(0).extension().install_timestamp_msec(),
      extension_prefs_->GetLastUpdateTime(kExtensionId[0])
          .InMillisecondsSinceUnixEpoch());

  // Verify that first extension's report has signal data.
  EXPECT_EQ(telemetry_report_pb->reports(0).signals().size(), 1);

  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsEnterpriseExtensionStoreEmpty());
}

TEST_F(ExtensionTelemetryServiceTest,
       GeneratesTelemetryReportWithSignalForESBAndEnterprise) {
  // Enable ESB and enterprise.
  prefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});
  PrimeTelemetryServiceWithSignal();

  std::unique_ptr<TelemetryReport> esb_telemetry_report = GetTelemetryReport();
  std::unique_ptr<TelemetryReport> enterprise_telemetry_report =
      GetTelemetryReportForEnterprise();

  // Verify telemetry reports generated for both ESB and Enterprise.
  ASSERT_EQ(esb_telemetry_report->reports_size(), 2);
  ASSERT_EQ(enterprise_telemetry_report->reports_size(), 1);

  // Verify that both extension stores has been cleared after creating telemetry
  // reports.
  EXPECT_TRUE(IsExtensionStoreEmpty());
  EXPECT_TRUE(IsEnterpriseExtensionStoreEmpty());
}

TEST_F(ExtensionTelemetryServiceTest,
       GeneratesTelemetryReportWithDeveloperMode) {
  // Generate a telemetry report with developer mode disabled.
  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);

  // Verify developer mode is disabled.
  EXPECT_FALSE(telemetry_report_pb->developer_mode_enabled());

  // Set developer mode pref to true and generate another telemetry report.
  prefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());

  std::unique_ptr<TelemetryReport> telemetry_report_pb_2 = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb_2, nullptr);

  // Verify developer is enabled and collected.
  EXPECT_TRUE(telemetry_report_pb_2->developer_mode_enabled());
}

TEST_F(ExtensionTelemetryServiceTest,
       GeneratesTelemetryReportWithManagementAuthorityTrustworthiness) {
  {
    // Test NONE trustworthiness with setting NONE authority for both platform
    // and profile.
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(&profile_),
        policy::EnterpriseManagementAuthority::NONE);

    // Generate a telemetry report and verify.
    task_environment_.FastForwardBy(
        telemetry_service_->current_reporting_interval());

    EXPECT_EQ(GetTelemetryReport()->management_authority(),
              TelemetryReport::MANAGEMENT_AUTHORITY_NONE);
  }

  {
    // Test LOW trustworthiness with setting COMPUTER_LOCAL authority for
    // platform and NONE authority for profile.
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(&profile_),
        policy::EnterpriseManagementAuthority::NONE);

    // Generate a telemetry report and verify.
    task_environment_.FastForwardBy(
        telemetry_service_->current_reporting_interval());

    EXPECT_EQ(GetTelemetryReport()->management_authority(),
              TelemetryReport::MANAGEMENT_AUTHORITY_LOW);
  }

  {
    // Test TRUSTED trustworthiness with setting NONE authority for
    // platform and CLOUD authority for profile.
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::NONE);
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(&profile_),
        policy::EnterpriseManagementAuthority::CLOUD);

    // Generate a telemetry report and verify.
    task_environment_.FastForwardBy(
        telemetry_service_->current_reporting_interval());

    EXPECT_EQ(GetTelemetryReport()->management_authority(),
              TelemetryReport::MANAGEMENT_AUTHORITY_TRUSTED);
  }

  {
    // Test FULLY_TRUSTED trustworthiness with setting CLOUD_DOMAIN authority
    // for platform and DOMAIN_LOCAL authority for profile.
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);
    policy::ScopedManagementServiceOverrideForTesting profile_management(
        policy::ManagementServiceFactory::GetForProfile(&profile_),
        policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

    // Generate a telemetry report and verify.
    task_environment_.FastForwardBy(
        telemetry_service_->current_reporting_interval());

    EXPECT_EQ(GetTelemetryReport()->management_authority(),
              TelemetryReport::MANAGEMENT_AUTHORITY_FULLY_TRUSTED);
  }
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

    EXPECT_TRUE(extension_pb->has_telemetry_blocklist_state());
    EXPECT_EQ(extension_pb->telemetry_blocklist_state(),
              ExtensionInfo::NOT_BLOCKLISTED);

    EXPECT_TRUE(extension_pb->has_disable_reasons());
    EXPECT_EQ(extension_pb->disable_reasons(), static_cast<uint32_t>(0));

    EXPECT_TRUE(extension_pb->has_installation_policy());
    EXPECT_EQ(extension_pb->installation_policy(), ExtensionInfo::NO_POLICY);
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

  {
    // Test changing the telemetry blocklist state.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("blocklist")
            .SetLocation(ManifestLocation::kInternal)
            .Build();
    add_extension(extension.get());
    extensions::blocklist_prefs::SetExtensionTelemetryServiceBlocklistState(
        extension->id(), extensions::BitMapBlocklistState::BLOCKLISTED_MALWARE,
        extension_prefs_);
    std::unique_ptr<ExtensionInfo> extension_pb = GetExtensionInfo(*extension);
    EXPECT_EQ(extension_pb->telemetry_blocklist_state(),
              ExtensionInfo::BLOCKLISTED_MALWARE);
  }

  {
    // Test installation policy.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder("unpacked")
            .SetLocation(ManifestLocation::kUnpacked)
            .Build();
    add_extension(extension.get());
    {
      // Test NO_POLICY.
      std::unique_ptr<ExtensionInfo> unmanaged_allowed_extension_pb =
          GetExtensionInfo(*extension);
      EXPECT_EQ(unmanaged_allowed_extension_pb->installation_policy(),
                ExtensionInfo::NO_POLICY);
    }
    {
      // Test INSTALLATION_ALLOWED, INSTALLATION_BLOCKED, INSTALLATION_FORCED,
      // and INSTALLATION_RECOMMENDED.
      const std::vector<
          std::tuple<std::string, ExtensionInfo::InstallationPolicy>>
          installation_policies = {
              {"allowed", ExtensionInfo::INSTALLATION_ALLOWED},
              {"blocked", ExtensionInfo::INSTALLATION_BLOCKED},
              {"force_installed", ExtensionInfo::INSTALLATION_FORCED},
              {"normal_installed", ExtensionInfo::INSTALLATION_RECOMMENDED}};

      for (const auto& [mode, policy] : installation_policies) {
        base::Value::Dict entry = base::Value::Dict()
                                      .Set(kInstallationMode, mode)
                                      .Set(kUpdateUrl, kTestUpdateUrl);
        profile_.GetTestingPrefService()->SetManagedPref(
            extensions::pref_names::kExtensionManagement,
            base::Value::Dict().Set(extension->id(), std::move(entry)));

        std::unique_ptr<ExtensionInfo> extension_pb =
            GetExtensionInfo(*extension);
        EXPECT_EQ(extension_pb->installation_policy(), policy);
      }
    }
  }
}

TEST_F(ExtensionTelemetryServiceTest,
       PersistsReportsOnShutdownWithSignalDataPresent) {
  // Setting up the persister and signals.
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
  telemetry_service_->SetEnabledForESB(false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(base::PathExists(persisted_dir));
}

TEST_F(ExtensionTelemetryServiceTest,
       DoesNotPersistsReportsOnShutdownWithNoSignalDataPresent) {
  // Setting up the persister and signals.
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
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  prefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
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
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  prefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
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
  telemetry_service_->SetEnabledForESB(false);
  // NumChecksPerUploadInterval defaults to 1, setting to 4 to test
  // functionality of writing at intervals and uploading multiple files.
  telemetry_service_->num_checks_per_upload_interval_ = 4;
  telemetry_service_->SetEnabledForESB(true);
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  prefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
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
  // Setting up the persister, signals, upload/check intervals, and the
  // uploader itself.
  telemetry_service_->SetEnabledForESB(false);
  telemetry_service_->num_checks_per_upload_interval_ = 4;
  telemetry_service_->SetEnabledForESB(true);
  base::TimeDelta interval = telemetry_service_->current_reporting_interval();
  prefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
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
  telemetry_service_->SetEnabledForESB(true);
  task_environment_.RunUntilIdle();
  prefs()->SetTime(prefs::kExtensionTelemetryLastUploadTime,
                   base::Time::NowFromSystemTime());
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), "Dummy", net::HTTP_OK);
  // Take the telemetry service offline and fast forward the environment
  // by a whole upload interval.
  telemetry_service_->SetEnabledForESB(false);
  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  telemetry_service_->SetEnabledForESB(true);
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
  std::unique_ptr<ExtensionTelemetryService> telemetry_service_2 =
      std::make_unique<ExtensionTelemetryService>(
          &profile_, test_url_loader_factory_.GetSafeWeakWrapper());
  telemetry_service_2->SetEnabledForESB(true);
  telemetry_service_2.reset();
}

TEST_F(ExtensionTelemetryServiceTest, FileData_ProcessesOffstoreExtensions) {
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  auto& file_data_dict = prefs()->GetDict(prefs::kExtensionTelemetryFileData);

  // Test Extension 0.
  EXPECT_TRUE(file_data_dict.contains(kExtensionId[0]));

  const base::Value::Dict* actual_extension_0 =
      file_data_dict.FindDict(kExtensionId[0]);
  EXPECT_TRUE(actual_extension_0->FindString(kFileDataProcessTimestampPref));

  const base::Value::Dict* actual_extension_0_file_data =
      actual_extension_0->FindDict(kFileDataDictPref);
  EXPECT_TRUE(actual_extension_0_file_data->contains(kJavaScriptFile));
  EXPECT_TRUE(actual_extension_0_file_data->FindString(kJavaScriptFile));
  EXPECT_TRUE(actual_extension_0_file_data->contains(kManifestFile));
  EXPECT_TRUE(actual_extension_0_file_data->FindString(kManifestFile));

  // Test Extension 1.
  EXPECT_TRUE(file_data_dict.contains(kExtensionId[1]));

  const base::Value::Dict* actual_extension_1 =
      file_data_dict.FindDict(kExtensionId[1]);
  EXPECT_TRUE(actual_extension_1->FindString(kFileDataProcessTimestampPref));

  const base::Value::Dict* actual_extension_1_file_data =
      actual_extension_1->FindDict(kFileDataDictPref);
  EXPECT_TRUE(actual_extension_1_file_data->contains(kJavaScriptFile));
  EXPECT_TRUE(actual_extension_1_file_data->FindString(kJavaScriptFile));
  EXPECT_TRUE(actual_extension_1_file_data->contains(kManifestFile));
  EXPECT_TRUE(actual_extension_1_file_data->FindString(kManifestFile));
}

TEST_F(ExtensionTelemetryServiceTest, FileData_IgnoresNonOffstoreExtensions) {
  // Create/register webstore, component, and external component extensions.
  RegisterExtensionWithExtensionService(kExtensionId[2], kExtensionName[2],
                                        ManifestLocation::kInternal,
                                        Extension::FROM_WEBSTORE);
  RegisterExtensionWithExtensionService(kExtensionId[3], kExtensionName[3],
                                        ManifestLocation::kComponent,
                                        Extension::NO_FLAGS);
  RegisterExtensionWithExtensionService(kExtensionId[4], kExtensionName[4],
                                        ManifestLocation::kExternalComponent,
                                        Extension::NO_FLAGS);

  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  auto& file_data_dict = prefs()->GetDict(prefs::kExtensionTelemetryFileData);

  // Only test extension 0 and 1 are processed.
  EXPECT_TRUE(file_data_dict.contains(kExtensionId[0]));
  EXPECT_TRUE(file_data_dict.contains(kExtensionId[1]));
  EXPECT_FALSE(file_data_dict.contains(kExtensionId[2]));
  EXPECT_FALSE(file_data_dict.contains(kExtensionId[3]));
  EXPECT_FALSE(file_data_dict.contains(kExtensionId[4]));
}

TEST_F(ExtensionTelemetryServiceTest, FileData_RemovesStaleExtensionFromPref) {
  // Process extension 0 and 1 and save to prefs.
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  UnregisterExtensionWithExtensionService(kExtensionId[0]);

  telemetry_service_->SetEnabledForESB(false);
  telemetry_service_->SetEnabledForESB(true);
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  auto& file_data_dict = prefs()->GetDict(prefs::kExtensionTelemetryFileData);

  // Extension 0 is removed from prefs since unregistered.
  EXPECT_FALSE(file_data_dict.contains(kExtensionId[0]));
  EXPECT_TRUE(file_data_dict.contains(kExtensionId[1]));
}

TEST_F(ExtensionTelemetryServiceTest,
       FileData_ProcessesEachExtensionOncePerDay) {
  // Process extension 0 and 1 and save to prefs.
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  // Save first processed timestamp.
  auto& file_data_dict = prefs()->GetDict(prefs::kExtensionTelemetryFileData);
  const std::string first_processed_timestamp =
      *(file_data_dict.FindDict(kExtensionId[0])
            ->FindString(kFileDataProcessTimestampPref));

  // Register new extension and fast forward to next run.
  RegisterExtensionWithExtensionService(kExtensionId[2], kExtensionName[2],
                                        ManifestLocation::kUnpacked,
                                        Extension::NO_FLAGS);
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionIntervalSeconds());
  task_environment_.RunUntilIdle();

  // Extensions 0 and 1 match first processed timestamp.
  const std::string extension_0_timestamp =
      *(file_data_dict.FindDict(kExtensionId[0])
            ->FindString(kFileDataProcessTimestampPref));
  EXPECT_EQ(extension_0_timestamp, first_processed_timestamp);

  const std::string extension_1_timestamp =
      *(file_data_dict.FindDict(kExtensionId[1])
            ->FindString(kFileDataProcessTimestampPref));
  EXPECT_EQ(extension_1_timestamp, first_processed_timestamp);

  // Extension 2 is processed at the next interval.
  const std::string extension_2_timestamp =
      *(file_data_dict.FindDict(kExtensionId[2])
            ->FindString(kFileDataProcessTimestampPref));
  EXPECT_GT(extension_2_timestamp, first_processed_timestamp);
}

TEST_F(ExtensionTelemetryServiceTest, FileData_HandlesEmptyTimestampsInPrefs) {
  // Set up pref dict:
  // extension 0 - empty timestamp string
  // extension 1 - missing timestamp key
  auto extension_0_dict =
      base::Value::Dict().Set(kFileDataProcessTimestampPref, "");
  base::Value::Dict empty_timestamps_dict;
  empty_timestamps_dict.Set(kExtensionId[0], std::move(extension_0_dict));
  empty_timestamps_dict.Set(kExtensionId[1], base::Value::Dict());
  prefs()->SetDict(prefs::kExtensionTelemetryFileData,
                   std::move(empty_timestamps_dict));

  // Process extension 0 and 1 and save to prefs.
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  auto& file_data_dict = prefs()->GetDict(prefs::kExtensionTelemetryFileData);

  // Test Extension 0.
  EXPECT_TRUE(file_data_dict.contains(kExtensionId[0]));
  const base::Value::Dict* actual_extension_0 =
      file_data_dict.FindDict(kExtensionId[0]);
  EXPECT_TRUE(actual_extension_0->FindString(kFileDataProcessTimestampPref));
  EXPECT_TRUE(actual_extension_0->FindDict(kFileDataDictPref));

  // Test Extension 1.
  EXPECT_TRUE(file_data_dict.contains(kExtensionId[1]));
  const base::Value::Dict* actual_extension_1 =
      file_data_dict.FindDict(kExtensionId[1]);
  EXPECT_TRUE(actual_extension_1->FindString(kFileDataProcessTimestampPref));
  EXPECT_TRUE(actual_extension_1->FindDict(kFileDataDictPref));
}

TEST_F(ExtensionTelemetryServiceTest,
       FileData_AttachesOffstoreFileDataToReport) {
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  const auto& file_data_dict =
      prefs()->GetDict(prefs::kExtensionTelemetryFileData);

  const base::Value::Dict* extension_0_dict =
      file_data_dict.FindDict(kExtensionId[0])->FindDict(kFileDataDictPref);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().id(), kExtensionId[0]);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().manifest_json(),
            *(extension_0_dict->FindString(kManifestFile)));
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos_size(), 1);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos(0).name(),
            kJavaScriptFile);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos(0).hash(),
            *(extension_0_dict->FindString(kJavaScriptFile)));

  const base::Value::Dict* extension_1_dict =
      file_data_dict.FindDict(kExtensionId[1])->FindDict(kFileDataDictPref);
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().id(), kExtensionId[1]);
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().manifest_json(),
            *(extension_1_dict->FindString(kManifestFile)));
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().file_infos_size(), 1);
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().file_infos(0).name(),
            kJavaScriptFile);
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().file_infos(0).hash(),
            *(extension_1_dict->FindString(kJavaScriptFile)));
}

TEST_F(ExtensionTelemetryServiceTest,
       FileData_IncludesCommandlineExtensionsFileDataInReport) {
  // Remove previously installed extensions.
  UnregisterExtensionWithExtensionService(kExtensionId[0]);
  UnregisterExtensionWithExtensionService(kExtensionId[1]);
  telemetry_service_->SetEnabledForESB(false);
  // Create a commandline extension, set up the --load-extension commandline
  // switch, and re-enable the telemetry service.
  base::FilePath path = CreateExtensionForCommandLineLoad("commandline_crx");
  base::CommandLine::ForCurrentProcess()->AppendSwitchPath(
      extensions::switches::kLoadExtension, path);
  telemetry_service_->SetEnabledForESB(true);
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  // Generate and verify telemetry report contents.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_TRUE(telemetry_report_pb);
  ASSERT_EQ(telemetry_report_pb->reports_size(), 1);
  auto& cmdline_extension = telemetry_report_pb->reports(0).extension();
  // Verify extension name.
  EXPECT_EQ(cmdline_extension.name(), "commandline_crx");
  // Verify that the install timestamp is explicitly set to 0 and is not the
  // same as the timestamp set in extension prefs from a previous install.
  EXPECT_EQ(cmdline_extension.install_timestamp_msec(), 0);
  EXPECT_NE(cmdline_extension.install_timestamp_msec(),
            extension_prefs_->GetLastUpdateTime(cmdline_extension.id())
                .InMillisecondsSinceUnixEpoch());
  // Verify that cmdline extension file data stored in prefs matches that in the
  // telemetry report.
  const auto& file_data_dict =
      prefs()->GetDict(prefs::kExtensionTelemetryFileData);
  ASSERT_EQ(file_data_dict.size(), 1u);
  const base::Value::Dict* cmdline_extension_file_data_dict =
      file_data_dict.FindDict(cmdline_extension.id())
          ->FindDict(kFileDataDictPref);
  ASSERT_TRUE(cmdline_extension_file_data_dict);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().manifest_json(),
            *(cmdline_extension_file_data_dict->FindString(kManifestFile)));
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos_size(), 1);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos(0).name(),
            kJavaScriptFile);
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos(0).hash(),
            *(cmdline_extension_file_data_dict->FindString(kJavaScriptFile)));
}

TEST_F(ExtensionTelemetryServiceTest,
       FileData_DoesNotAttachFileDataForNonOffstoreExtensions) {
  // Register webstore extension 2.
  RegisterExtensionWithExtensionService(kExtensionId[2], kExtensionName[2],
                                        ManifestLocation::kInternal,
                                        Extension::FROM_WEBSTORE);
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();

  // Verify that Extension 0 has offstore file data.
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().id(), kExtensionId[0]);
  EXPECT_FALSE(
      telemetry_report_pb->reports(0).extension().manifest_json().empty());
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos_size(), 1);

  // Verify Extension 1 has offstore file data.
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().id(), kExtensionId[1]);
  EXPECT_FALSE(
      telemetry_report_pb->reports(1).extension().manifest_json().empty());
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().file_infos_size(), 1);

  // Verify Extension 2 does not have offstore file data.
  EXPECT_EQ(telemetry_report_pb->reports(2).extension().id(), kExtensionId[2]);
  EXPECT_FALSE(telemetry_report_pb->reports(2).extension().has_manifest_json());
  EXPECT_EQ(telemetry_report_pb->reports(2).extension().file_infos_size(), 0);
}

TEST_F(ExtensionTelemetryServiceTest, FileData_HandlesEmptyFileDataInPrefs) {
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  // Set up pref dict:
  // extension 0 - empty file data dict
  // extension 1 - missing file data dict key
  base::Value::Dict extension_0_dict;
  extension_0_dict.Set(kFileDataDictPref, base::Value::Dict());
  base::Value::Dict empty_file_data_dicts;
  empty_file_data_dicts.Set(kExtensionId[0], std::move(extension_0_dict));
  empty_file_data_dicts.Set(kExtensionId[1], base::Value::Dict());
  prefs()->SetDict(prefs::kExtensionTelemetryFileData,
                   std::move(empty_file_data_dicts));

  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();

  // Verify Extension 0 does not have offstore file data.
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().id(), kExtensionId[0]);
  EXPECT_FALSE(telemetry_report_pb->reports(0).extension().has_manifest_json());
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos_size(), 0);

  // Verify Extension 1 does not have offstore file data.
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().id(), kExtensionId[1]);
  EXPECT_FALSE(telemetry_report_pb->reports(1).extension().has_manifest_json());
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().file_infos_size(), 0);
}

TEST_F(ExtensionTelemetryServiceTest,
       FileData_EnforcesCollectionDurationLimit) {
  // Set collection duration limit to 0 milliseconds.
  telemetry_service_->offstore_file_data_collection_duration_limit_ =
      base::Milliseconds(0);
  task_environment_.FastForwardBy(
      telemetry_service_->GetOffstoreFileDataCollectionStartupDelaySeconds());
  task_environment_.RunUntilIdle();

  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();

  // Verify Extension 0 does not have offstore file data.
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().id(), kExtensionId[0]);
  EXPECT_FALSE(telemetry_report_pb->reports(0).extension().has_manifest_json());
  EXPECT_EQ(telemetry_report_pb->reports(0).extension().file_infos_size(), 0);

  // Verify Extension 1 does not have offstore file data.
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().id(), kExtensionId[1]);
  EXPECT_FALSE(telemetry_report_pb->reports(1).extension().has_manifest_json());
  EXPECT_EQ(telemetry_report_pb->reports(1).extension().file_infos_size(), 0);
}

TEST_F(ExtensionTelemetryServiceTest, DisableOffstoreExtensions) {
  // Extension 0 is enabled and not on blocklist.
  EXPECT_TRUE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[0]));
  EXPECT_FALSE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[0]));

  // Attach a MALWARE verdict for Extension 0 in telemetry report response.
  ExtensionTelemetryReportResponse response;
  auto* malware_verdict = response.add_offstore_extension_verdicts();
  malware_verdict->set_extension_id(kExtensionId[0]);
  malware_verdict->set_verdict_type(OffstoreExtensionVerdict::MALWARE);

  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(),
      response.SerializeAsString(), net::HTTP_OK);

  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  task_environment_.RunUntilIdle();

  // Verify Extension 0 is on blocklisted list.
  EXPECT_FALSE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[0]));
  EXPECT_TRUE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[0]));
}

TEST_F(ExtensionTelemetryServiceTest,
       DisableOffstoreExtensions_IgnoresNonOffstoreExtensions) {
  // Register webstore extension 2 and component extension 3.
  RegisterExtensionWithExtensionService(kExtensionId[2], kExtensionName[2],
                                        ManifestLocation::kInternal,
                                        Extension::FROM_WEBSTORE);
  RegisterExtensionWithExtensionService(kExtensionId[3], kExtensionName[3],
                                        ManifestLocation::kComponent,
                                        Extension::NO_FLAGS);

  // Extensions 2/3 is enabled and not on blocklist.
  EXPECT_TRUE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[2]));
  EXPECT_FALSE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[2]));
  EXPECT_TRUE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[3]));
  EXPECT_FALSE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[3]));

  // Attach a MALWARE verdict for Extensions 2/3 in telemetry report response.
  ExtensionTelemetryReportResponse response;
  auto* malware_verdict_2 = response.add_offstore_extension_verdicts();
  malware_verdict_2->set_extension_id(kExtensionId[2]);
  malware_verdict_2->set_verdict_type(OffstoreExtensionVerdict::MALWARE);
  auto* malware_verdict_3 = response.add_offstore_extension_verdicts();
  malware_verdict_3->set_extension_id(kExtensionId[3]);
  malware_verdict_3->set_verdict_type(OffstoreExtensionVerdict::MALWARE);

  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(),
      response.SerializeAsString(), net::HTTP_OK);

  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  task_environment_.RunUntilIdle();

  // Verify no action taken on Extensions 2/3.
  EXPECT_TRUE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[2]));
  EXPECT_FALSE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[2]));
  EXPECT_TRUE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[3]));
  EXPECT_FALSE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[3]));
}

TEST_F(ExtensionTelemetryServiceTest, DisableOffstoreExtensions_Reenable) {
  // Attach a MALWARE verdict for Extension 0 in telemetry report response.
  ExtensionTelemetryReportResponse malware_response;
  auto* malware_verdict = malware_response.add_offstore_extension_verdicts();
  malware_verdict->set_extension_id(kExtensionId[0]);
  malware_verdict->set_verdict_type(OffstoreExtensionVerdict::MALWARE);

  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(),
      malware_response.SerializeAsString(), net::HTTP_OK);

  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  task_environment_.RunUntilIdle();

  // Verify Extension 0 is on blocklisted list.
  EXPECT_FALSE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[0]));
  EXPECT_TRUE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[0]));

  // Attach a NONE verdict for Extension 0 in telemetry report response.
  ExtensionTelemetryReportResponse unblocklist_response;
  auto* none_verdict = unblocklist_response.add_offstore_extension_verdicts();
  none_verdict->set_extension_id(kExtensionId[0]);
  none_verdict->set_verdict_type(OffstoreExtensionVerdict::NONE);

  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(),
      unblocklist_response.SerializeAsString(), net::HTTP_OK);

  task_environment_.FastForwardBy(
      telemetry_service_->current_reporting_interval());
  task_environment_.RunUntilIdle();

  // Extension 0 is enabled and not on blocklist.
  EXPECT_TRUE(
      extension_registry_->enabled_extensions().Contains(kExtensionId[0]));
  EXPECT_FALSE(
      extension_registry_->blocklisted_extensions().Contains(kExtensionId[0]));
}

class ExtensionTelemetryServiceSystemTimeTest
    : public ExtensionTelemetryServiceTest {
 public:
  ExtensionTelemetryServiceSystemTimeTest()
      : ExtensionTelemetryServiceTest(
            base::test::TaskEnvironment::TimeSource::SYSTEM_TIME) {}
};

// Verify that a telemetry report persisted at service shutdown should have the
// `MANAGEMENT_AUTHORITY_UNSPECIFIED` management authority value.
// Regression test for https://crbug.com/362493322.
TEST_F(ExtensionTelemetryServiceSystemTimeTest,
       PersistsReportsWithUnspecifiedManagementAuthorityOnShutdown) {
  // Set up signals and persist a telemetry report after shutdown.
  PrimeTelemetryServiceWithSignal();
  telemetry_service_->Shutdown();

  // Retrieve persisted report.
  base::FilePath persisted_file_path = profile_.GetPath()
                                           .AppendASCII("CRXTelemetry")
                                           .AppendASCII("CRXTelemetry_0");
  EXPECT_TRUE(base::test::RunUntil([&] {
    int64_t file_size = 0;
    return base::PathExists(persisted_file_path) &&
           base::GetFileSize(persisted_file_path, &file_size) &&
           file_size > kMinReportSize;
  }));

  std::string persisted_report;
  EXPECT_TRUE(base::ReadFileToString(persisted_file_path, &persisted_report));
  ExtensionTelemetryReportRequest request;
  request.ParseFromString(persisted_report);

  // Verify management authority.
  EXPECT_EQ(request.management_authority(),
            TelemetryReport::MANAGEMENT_AUTHORITY_UNSPECIFIED);
}

}  // namespace safe_browsing
