// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;

namespace enterprise_connectors {

namespace {

constexpr char kFakeProfileUsername[] = "fake-profile";
constexpr char kFakeExtensionId[] = "fake-extension-id";
constexpr char kFakeExtensionVersion[] = "1";
constexpr char kFakeExtensionName[] = "Foo extension";
constexpr char kUnknownInstallLocation[] = "UNKNOWN_LOCATION";
constexpr char kInternalInstallLocation[] = "INTERNAL";
constexpr char kExternalPrefInstallLocation[] = "EXTERNAL_PREF";
constexpr char kExternalRegistryInstallLocation[] = "EXTERNAL_REGISTRY";
constexpr char kUnpackedInstallLocation[] = "UNPACKED";
constexpr char kComponentInstallLocation[] = "COMPONENT";
constexpr char kExternalPrefDownloadInstallLocation[] =
    "EXTERNAL_PREF_DOWNLOAD";
constexpr char kExternalPolicyDownloadInstallLocation[] =
    "EXTERNAL_POLICY_DOWNLOAD";
constexpr char kCommandLineInstallLocation[] = "COMMAND_LINE";
constexpr char kExternalPolicyInstallLocation[] = "EXTERNAL_POLICY";
constexpr char kExternalComponentInstallLocation[] = "EXTERNAL_COMPONENT";
const char* kAllExtensionSources[] = {kUnknownInstallLocation,
                                      kInternalInstallLocation,
                                      kExternalPrefInstallLocation,
                                      kExternalRegistryInstallLocation,
                                      kUnpackedInstallLocation,
                                      kComponentInstallLocation,
                                      kExternalPrefDownloadInstallLocation,
                                      kExternalPolicyDownloadInstallLocation,
                                      kCommandLineInstallLocation,
                                      kExternalPolicyInstallLocation,
                                      kExternalComponentInstallLocation};

}  // namespace

// SafeBrowsingDatabaseManager implementation that returns fixed result for
// given URL
class MockRealtimeReportingClient : public RealtimeReportingClient {
 public:
  explicit MockRealtimeReportingClient(content::BrowserContext* context)
      : RealtimeReportingClient(context) {}
  MockRealtimeReportingClient(const MockRealtimeReportingClient&) = delete;
  MockRealtimeReportingClient& operator=(const MockRealtimeReportingClient&) =
      delete;

  MOCK_METHOD3(ReportRealtimeEvent,
               void(const std::string&,
                    const ReportingSettings& settings,
                    base::Value::Dict event));

  MOCK_METHOD(std::string, GetProfileUserName, (), (const, override));
};

std::unique_ptr<KeyedService> MakeMockRealtimeReportingClient(
    content::BrowserContext* profile_) {
  return std::make_unique<MockRealtimeReportingClient>(profile_);
}

class ExtensionTelemetryEventRouterTest : public testing::Test {
 public:
  ExtensionTelemetryEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeProfileUsername);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&MakeMockRealtimeReportingClient));
    extension_telemetry_event_router_ =
        std::make_unique<ExtensionTelemetryEventRouter>(profile_);

    mock_realtime_reporting_client_ = static_cast<MockRealtimeReportingClient*>(
        RealtimeReportingClientFactory::GetForProfile(profile_));

    test::SetOnSecurityEventReporting(
        profile_->GetPrefs(), /*enabled=*/true,
        /*enabled_event_names=*/std::set<std::string>(),
        /*enabled_opt_in_events=*/
        std::map<std::string, std::vector<std::string>>());
    // Set a mock cloud policy client in the router.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");
    mock_realtime_reporting_client_->SetBrowserCloudPolicyClientForTesting(
        client_.get());

    // Set up extension info
    base::Value::Dict manifest;
    manifest.Set(extensions::manifest_keys::kName, kFakeExtensionName);
    manifest.Set(extensions::manifest_keys::kVersion, kFakeExtensionVersion);
    manifest.Set(extensions::manifest_keys::kManifestVersion, 2);

    std::string error;
    extension_chrome_ = extensions::Extension::Create(
        base::FilePath(), extensions::mojom::ManifestLocation::kExternalPref,
        manifest, extensions::Extension::NO_FLAGS, kFakeExtensionId, &error);
  }
  void TearDown() override {
    mock_realtime_reporting_client_->SetBrowserCloudPolicyClientForTesting(
        nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  scoped_refptr<extensions::Extension> extension_chrome_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<MockRealtimeReportingClient> mock_realtime_reporting_client_;
  std::unique_ptr<ExtensionTelemetryEventRouter>
      extension_telemetry_event_router_;
};

class ExtensionTelemetryEventExtensionSourceTest
    : public ExtensionTelemetryEventRouterTest,
      public testing::WithParamInterface<const char*> {
 public:
  ExtensionTelemetryEventExtensionSourceTest() {
    scoped_feature_list_.InitAndEnableFeature(kExtensionTelemetryEventsEnabled);
  }

 protected:
  const char* extension_source_ = GetParam();
  std::map<const char*, extensions::mojom::ManifestLocation> location_map_ = {
      {kUnknownInstallLocation,
       extensions::mojom::ManifestLocation::kInvalidLocation},
      {kInternalInstallLocation,
       extensions::mojom::ManifestLocation::kInternal},
      {kExternalPrefInstallLocation,
       extensions::mojom::ManifestLocation::kExternalPref},
      {kExternalRegistryInstallLocation,
       extensions::mojom::ManifestLocation::kExternalRegistry},
      {kUnpackedInstallLocation,
       extensions::mojom::ManifestLocation::kUnpacked},
      {kComponentInstallLocation,
       extensions::mojom::ManifestLocation::kComponent},
      {kExternalPrefDownloadInstallLocation,
       extensions::mojom::ManifestLocation::kExternalPrefDownload},
      {kExternalPolicyDownloadInstallLocation,
       extensions::mojom::ManifestLocation::kExternalPolicyDownload},
      {kCommandLineInstallLocation,
       extensions::mojom::ManifestLocation::kCommandLine},
      {kExternalPolicyInstallLocation,
       extensions::mojom::ManifestLocation::kExternalPolicy},
      {kExternalComponentInstallLocation,
       extensions::mojom::ManifestLocation::kExternalComponent}};
};

TEST_P(ExtensionTelemetryEventExtensionSourceTest,
       CheckTelemetryEventReported) {
  // Set up extension info
  base::Value::Dict manifest;
  manifest.Set(extensions::manifest_keys::kName, kFakeExtensionName);
  manifest.Set(extensions::manifest_keys::kVersion, kFakeExtensionVersion);
  manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
  std::string error;
  extension_chrome_ = extensions::Extension::Create(
      base::FilePath(), location_map_[extension_source_], manifest,
      extensions::Extension::NO_FLAGS, kFakeExtensionId, &error);

  base::Value::Dict expectedEvent;
  expectedEvent.Set("id", kFakeExtensionId);
  expectedEvent.Set("name", kFakeExtensionName);
  expectedEvent.Set("extension_version", kFakeExtensionVersion);
  expectedEvent.Set("extension_source", extension_source_);
  expectedEvent.Set("profileUserName", kFakeProfileUsername);

  EXPECT_CALL(*mock_realtime_reporting_client_, GetProfileUserName())
      .WillOnce(Return(kFakeProfileUsername));
  EXPECT_CALL(*mock_realtime_reporting_client_,
              ReportRealtimeEvent("extensionTelemetryEvent", _,
                                  Eq(ByRef(expectedEvent))))
      .Times(1);
  extension_telemetry_event_router_->UploadTelemetryReport(
      profile_, extension_chrome_.get());
}

INSTANTIATE_TEST_SUITE_P(ExtensionTelemetryEventExtensionSourceTest,
                         ExtensionTelemetryEventExtensionSourceTest,
                         testing::ValuesIn(kAllExtensionSources));

TEST_F(ExtensionTelemetryEventRouterTest, CheckTelemetryEventNotReported) {
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{"browserExtensionInstallEvent"},
      /*enabled_opt_in_events=*/
      std::map<std::string, std::vector<std::string>>());
  EXPECT_CALL(*mock_realtime_reporting_client_, GetProfileUserName()).Times(0);
  EXPECT_CALL(*mock_realtime_reporting_client_,
              ReportRealtimeEvent("extensionTelemetryEvent", _, _))
      .Times(0);
  extension_telemetry_event_router_->UploadTelemetryReport(
      profile_, extension_chrome_.get());
}

}  // namespace enterprise_connectors
