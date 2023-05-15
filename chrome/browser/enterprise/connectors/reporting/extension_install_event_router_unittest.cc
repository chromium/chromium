// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
namespace enterprise_connectors {

namespace {
const char* kFakeExtensionId = "fake-extension-id";
const char* kFakeExtensionName = "Foo extension";
const char* kFakeExtensionDescription = "Does Foo";
const char* kFakeProfileUsername = "Tiamat";

}  // namespace

// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
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

std::unique_ptr<KeyedService> CreateMockRealtimeReportingClient(
    content::BrowserContext* profile_) {
  return std::make_unique<MockRealtimeReportingClient>(profile_);
}

class ExtensionInstallEventRouterTest : public testing::Test {
 public:
  ExtensionInstallEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeProfileUsername);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&CreateMockRealtimeReportingClient));
    extensionInstallEventRouter_ =
        std::make_unique<ExtensionInstallEventRouter>(profile_);

    mockRealtimeReportingClient_ = static_cast<MockRealtimeReportingClient*>(
        RealtimeReportingClientFactory::GetForProfile(profile_));

    safe_browsing::SetOnSecurityEventReporting(
        profile_->GetPrefs(), /*enabled=*/true,
        /*enabled_event_names=*/std::set<std::string>(),
        /*enabled_opt_in_events=*/
        std::map<std::string, std::vector<std::string>>());
    // Set a mock cloud policy client in the router.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");
    mockRealtimeReportingClient_->SetBrowserCloudPolicyClientForTesting(
        client_.get());

    settings.enabled_event_names.insert(
        ReportingServiceSettings::kExtensionInstallEvent);

    base::Value::Dict manifest;
    manifest.Set(extensions::manifest_keys::kName, kFakeExtensionName);
    manifest.Set(extensions::manifest_keys::kVersion, "1");
    manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
    manifest.Set(extensions::manifest_keys::kDescription,
                 kFakeExtensionDescription);

    std::string error;
    extension_chrome_ = extensions::Extension::Create(
        base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
        manifest, extensions::Extension::NO_FLAGS, kFakeExtensionId, &error);
  }

  void TearDown() override {
    mockRealtimeReportingClient_->SetBrowserCloudPolicyClientForTesting(
        nullptr);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;

  scoped_refptr<extensions::Extension> extension_chrome_;
  ReportingSettings settings;
  raw_ptr<MockRealtimeReportingClient> mockRealtimeReportingClient_;
  std::unique_ptr<ExtensionInstallEventRouter> extensionInstallEventRouter_;
};

TEST_F(ExtensionInstallEventRouterTest, CheckEventReported) {
  base::Value::Dict expectedEvent;

  expectedEvent.Set("id", kFakeExtensionId);
  expectedEvent.Set("name", kFakeExtensionName);
  expectedEvent.Set("description", kFakeExtensionDescription);
  expectedEvent.Set("profileUserName", kFakeProfileUsername);

  EXPECT_CALL(*mockRealtimeReportingClient_, GetProfileUserName())
      .WillOnce(Return(kFakeProfileUsername));
  EXPECT_CALL(
      *mockRealtimeReportingClient_,
      ReportRealtimeEvent(ReportingServiceSettings::kExtensionInstallEvent, _,
                          Eq(ByRef(expectedEvent))))
      .Times(1);
  extensionInstallEventRouter_->OnExtensionInstalled(
      nullptr, extension_chrome_.get(), false);
}

}  // namespace enterprise_connectors
