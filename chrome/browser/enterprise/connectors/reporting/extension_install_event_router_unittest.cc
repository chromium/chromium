// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/reporting/extension_install_event_router.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using base::test::EqualsProto;
using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;

constexpr char kFakeExtensionId[] = "fake-extension-id";
constexpr char kFakeExtensionName[] = "Foo extension";
constexpr char kFakeExtensionDescription[] = "Does Foo";
constexpr char kFakeProfileUsername[] = "Tiamat";
constexpr char kFakeInstallAction[] = "INSTALL";
constexpr char kFakeUpdateAction[] = "UPDATE";
constexpr char kFakeUninstallAction[] = "UNINSTALL";
constexpr char kFakeExtensionVersion[] = "1";

}  // namespace

class ExtensionInstallEventRouterTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  ExtensionInstallEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    if (use_proto_format()) {
      feature_list_.InitAndEnableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    } else {
      feature_list_.InitAndDisableFeature(
          policy::kUploadRealtimeReportingEventsUsingProto);
    }

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeProfileUsername);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&test::MockRealtimeReportingClient::
                                          CreateMockRealtimeReportingClient));
    extension_install_event_router_ =
        std::make_unique<ExtensionInstallEventRouter>(profile_);

    mock_realtime_reporting_client_ =
        static_cast<test::MockRealtimeReportingClient*>(
            RealtimeReportingClientFactory::GetForProfile(profile_));
    mock_realtime_reporting_client_->SetProfileUserNameForTesting(
        kFakeProfileUsername);

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

    base::Value::Dict manifest;
    manifest.Set(extensions::manifest_keys::kName, kFakeExtensionName);
    manifest.Set(extensions::manifest_keys::kVersion, "1");
    manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
    manifest.Set(extensions::manifest_keys::kDescription,
                 kFakeExtensionDescription);
    auto location = is_component()
                        ? extensions::mojom::ManifestLocation::kComponent
                        : extensions::mojom::ManifestLocation::kUnpacked;

    int flags = is_from_webstore() ? extensions::Extension::FROM_WEBSTORE
                                   : extensions::Extension::NO_FLAGS;

    std::u16string error;
    extension_chrome_ = extensions::Extension::Create(
        base::FilePath(), location, manifest, flags, kFakeExtensionId, &error);
  }

  void TearDown() override {
    mock_realtime_reporting_client_->SetBrowserCloudPolicyClientForTesting(
        nullptr);
  }

  bool use_proto_format() { return std::get<0>(GetParam()); }
  bool is_from_webstore() { return std::get<1>(GetParam()); }
  bool is_component() { return std::get<2>(GetParam()); }

  std::string expected_extension_source_legacy_format() {
    if (is_component()) {
      return "COMPONENT";
    } else if (is_from_webstore()) {
      return "CHROME_WEBSTORE";
    } else {
      return "EXTERNAL";
    }
  }

  ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
      ExtensionSource
      expected_extension_source_proto_format() {
    if (is_component()) {
      return ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
          ExtensionSource::
              BrowserExtensionInstallEvent_ExtensionSource_COMPONENT;
    } else if (is_from_webstore()) {
      return ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
          ExtensionSource::
              BrowserExtensionInstallEvent_ExtensionSource_CHROME_WEBSTORE;
    } else {
      return ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
          ExtensionSource::
              BrowserExtensionInstallEvent_ExtensionSource_EXTERNAL;
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;

  scoped_refptr<extensions::Extension> extension_chrome_;
  raw_ptr<test::MockRealtimeReportingClient> mock_realtime_reporting_client_;
  std::unique_ptr<ExtensionInstallEventRouter> extension_install_event_router_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ExtensionInstallEventRouterTest, CheckInstallEventReported) {
  ::chrome::cros::reporting::proto::Event expected_event_proto;
  base::Value::Dict expected_event;

  if (use_proto_format()) {
    auto* extension_event =
        expected_event_proto.mutable_browser_extension_install_event();

    extension_event->set_id(kFakeExtensionId);
    extension_event->set_name(kFakeExtensionName);
    extension_event->set_description(kFakeExtensionDescription);
    extension_event->set_extension_action_type(
        ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
            ExtensionAction::
                BrowserExtensionInstallEvent_ExtensionAction_INSTALL);
    extension_event->set_extension_version(kFakeExtensionVersion);
    extension_event->set_extension_source(
        expected_extension_source_proto_format());

    EXPECT_CALL(*mock_realtime_reporting_client_,
                ReportEvent(EqualsProto(expected_event_proto), _))
        .Times(1);
  } else {
    expected_event.Set("id", kFakeExtensionId);
    expected_event.Set("name", kFakeExtensionName);
    expected_event.Set("description", kFakeExtensionDescription);
    expected_event.Set("extension_action_type", kFakeInstallAction);
    expected_event.Set("extension_version", kFakeExtensionVersion);
    expected_event.Set("extension_source",
                       expected_extension_source_legacy_format());

    EXPECT_CALL(*mock_realtime_reporting_client_,
                ReportRealtimeEvent(kExtensionInstallEvent, _,
                                    Eq(ByRef(expected_event))))
        .Times(1);
  }
  extension_install_event_router_->OnExtensionInstalled(
      nullptr, extension_chrome_.get(), false);
}

TEST_P(ExtensionInstallEventRouterTest, CheckUpdateEventReported) {
  ::chrome::cros::reporting::proto::Event expected_event_proto;
  base::Value::Dict expected_event;

  if (use_proto_format()) {
    auto* extension_event =
        expected_event_proto.mutable_browser_extension_install_event();

    extension_event->set_id(kFakeExtensionId);
    extension_event->set_name(kFakeExtensionName);
    extension_event->set_description(kFakeExtensionDescription);
    extension_event->set_extension_action_type(
        ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
            ExtensionAction::
                BrowserExtensionInstallEvent_ExtensionAction_UPDATE);
    extension_event->set_extension_version(kFakeExtensionVersion);
    extension_event->set_extension_source(
        expected_extension_source_proto_format());

    EXPECT_CALL(*mock_realtime_reporting_client_,
                ReportEvent(EqualsProto(expected_event_proto), _))
        .Times(1);
  } else {
    expected_event.Set("id", kFakeExtensionId);
    expected_event.Set("name", kFakeExtensionName);
    expected_event.Set("description", kFakeExtensionDescription);
    expected_event.Set("extension_action_type", kFakeUpdateAction);
    expected_event.Set("extension_version", kFakeExtensionVersion);
    expected_event.Set("extension_source",
                       expected_extension_source_legacy_format());

    EXPECT_CALL(*mock_realtime_reporting_client_,
                ReportRealtimeEvent(kExtensionInstallEvent, _,
                                    Eq(ByRef(expected_event))))
        .Times(1);
  }

  extension_install_event_router_->OnExtensionInstalled(
      nullptr, extension_chrome_.get(), true);
}

TEST_P(ExtensionInstallEventRouterTest, CheckUninstallEventReported) {
  ::chrome::cros::reporting::proto::Event expected_event_proto;
  base::Value::Dict expected_event;

  if (use_proto_format()) {
    auto* extension_event =
        expected_event_proto.mutable_browser_extension_install_event();

    extension_event->set_id(kFakeExtensionId);
    extension_event->set_name(kFakeExtensionName);
    extension_event->set_description(kFakeExtensionDescription);
    extension_event->set_extension_action_type(
        ::chrome::cros::reporting::proto::BrowserExtensionInstallEvent::
            ExtensionAction::
                BrowserExtensionInstallEvent_ExtensionAction_UNINSTALL);
    extension_event->set_extension_version(kFakeExtensionVersion);
    extension_event->set_extension_source(
        expected_extension_source_proto_format());

    EXPECT_CALL(*mock_realtime_reporting_client_,
                ReportEvent(EqualsProto(expected_event_proto), _))
        .Times(1);
  } else {
    expected_event.Set("id", kFakeExtensionId);
    expected_event.Set("name", kFakeExtensionName);
    expected_event.Set("description", kFakeExtensionDescription);
    expected_event.Set("extension_action_type", kFakeUninstallAction);
    expected_event.Set("extension_version", kFakeExtensionVersion);
    expected_event.Set("extension_source",
                       expected_extension_source_legacy_format());

    EXPECT_CALL(*mock_realtime_reporting_client_,
                ReportRealtimeEvent(kExtensionInstallEvent, _,
                                    Eq(ByRef(expected_event))))
        .Times(1);
  }
  extension_install_event_router_->OnExtensionUninstalled(
      nullptr, extension_chrome_.get(),
      extensions::UNINSTALL_REASON_FOR_TESTING);
}

INSTANTIATE_TEST_SUITE_P(,
                         ExtensionInstallEventRouterTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace enterprise_connectors

