// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_event_uploader_desktop.h"

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/enterprise/reporting/test/realtime_event_uploader_test_base.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

using ::testing::_;

}  // namespace

class BrowserLaunchEventUploaderDesktopTest
    : public RealtimeEventUploaderTestBase {};

TEST_F(BrowserLaunchEventUploaderDesktopTest, UploadBrowserEventSuccess) {
  SetBrowserManaged(true, "browser_dm_token");

  // Create a profile with a client so the discovery logic finds it. This
  // profile is not managed.
  auto* profile = CreateProfile("default_profile", /*is_managed=*/false,
                                /*is_affiliated=*/false,
                                /*create_reporting_client=*/true);

  BrowserLaunchEventUploaderDesktop uploader;

  EXPECT_CALL(*GetMockClient(profile),
              ReportBrowserLaunchEvent(_, false, "browser_dm_token", _))
      .WillOnce([](::chrome::cros::reporting::proto::Event wrapper, bool,
                   const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        EXPECT_TRUE(wrapper.has_browser_launch_event());
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  base::MockCallback<
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>>
      upload_callback;
  EXPECT_CALL(upload_callback,
              Run(testing::Property(
                  &policy::CloudPolicyClient::Result::IsSuccess, true)));

  uploader.UploadEvent(::chrome::cros::reporting::proto::BrowserLaunchEvent(),
                       upload_callback.Get());
}

TEST_F(BrowserLaunchEventUploaderDesktopTest, UploadProfileEventUnaffiliated) {
  SetBrowserManaged(true);
  auto* profile = CreateProfile("unaffiliated_profile", /*is_managed=*/true,
                                /*is_affiliated=*/false,
                                /*create_reporting_client=*/true);

  BrowserLaunchEventUploaderDesktop uploader(profile);

  EXPECT_CALL(*GetMockClient(profile),
              ReportBrowserLaunchEvent(_, true,
                                       "user_dm_token_unaffiliated_profile", _))
      .WillOnce([](::chrome::cros::reporting::proto::Event wrapper, bool,
                   const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        EXPECT_TRUE(wrapper.has_browser_launch_event());
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  base::MockCallback<
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>>
      upload_callback;
  EXPECT_CALL(upload_callback,
              Run(testing::Property(
                  &policy::CloudPolicyClient::Result::IsSuccess, true)));

  uploader.UploadEvent(::chrome::cros::reporting::proto::BrowserLaunchEvent(),
                       upload_callback.Get());
}

TEST_F(BrowserLaunchEventUploaderDesktopTest,
       UploadEventMultipleAffiliatedProfiles) {
  SetBrowserManaged(true, "browser_dm_token");

  TestingProfile* profile1 = CreateProfile("profile1", /*is_managed=*/true,
                                           /*is_affiliated=*/true,
                                           /*create_reporting_client=*/true);
  TestingProfile* profile2 = CreateProfile("profile2", /*is_managed=*/true,
                                           /*is_affiliated=*/false,
                                           /*create_reporting_client=*/true);

  std::vector<Profile*> loaded_profiles =
      profile_manager_->profile_manager()->GetLoadedProfiles();
  ASSERT_EQ(loaded_profiles.size(), 2u);

  ::chrome::cros::reporting::proto::BrowserLaunchEvent event;

  auto* active_mock_client = GetMockClient(loaded_profiles[0]);
  auto* ignored_mock_client = GetMockClient(loaded_profiles[1]);

  // The browser-level report is machine-wide and can be sent by the client
  // from any loaded profile. We expect exactly one machine report with the
  // browser DM token.
  EXPECT_CALL(*active_mock_client,
              ReportBrowserLaunchEvent(_, false, "browser_dm_token", _))
      .WillOnce([](auto, bool, const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  EXPECT_CALL(*ignored_mock_client,
              ReportBrowserLaunchEvent(_, false, "browser_dm_token", _))
      .Times(0);

  // Expectation for profile-level reports.
  EXPECT_CALL(*GetMockClient(profile1),
              ReportBrowserLaunchEvent(_, true, "user_dm_token_profile1", _))
      .WillOnce([](auto, bool, const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  EXPECT_CALL(*GetMockClient(profile2),
              ReportBrowserLaunchEvent(_, true, "user_dm_token_profile2", _))
      .WillOnce([](auto, bool, const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  base::MockCallback<
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>>
      upload_callback;
  EXPECT_CALL(upload_callback,
              Run(testing::Property(
                  &policy::CloudPolicyClient::Result::IsSuccess, true)))
      .Times(3);

  // Trigger all uploaders.
  BrowserLaunchEventUploaderDesktop browser_uploader;
  browser_uploader.UploadEvent(event, upload_callback.Get());

  BrowserLaunchEventUploaderDesktop p1_uploader(profile1);
  p1_uploader.UploadEvent(event, upload_callback.Get());

  BrowserLaunchEventUploaderDesktop p2_uploader(profile2);
  p2_uploader.UploadEvent(event, upload_callback.Get());
}

TEST_F(BrowserLaunchEventUploaderDesktopTest,
       UploadBrowserEventMultiProfileStartup) {
  SetBrowserManaged(true, "browser_dm_token");

  // At startup with multiple profiles, only the system profile is loaded.
  // We simulate this by creating a profile without a reporting client.
  CreateProfile("system_profile", /*is_managed=*/false,
                /*is_affiliated=*/false,
                /*create_reporting_client=*/false);

  BrowserLaunchEventUploaderDesktop uploader;

  base::MockCallback<
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>>
      upload_callback;

  // The upload should be deferred until a regular profile with a reporting
  // client is loaded, so upload_callback should not be called yet.
  EXPECT_CALL(upload_callback, Run(_)).Times(0);
  uploader.UploadEvent(::chrome::cros::reporting::proto::BrowserLaunchEvent(),
                       upload_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&upload_callback);

  // Now simulate adding a regular profile with a RealtimeReportingClient
  // (e.g., when the user selects a profile tile in the Profile Picker).
  auto* user_profile =
      CreateProfile("user_profile", /*is_managed=*/false,
                    /*is_affiliated=*/false,
                    /*create_reporting_client=*/true);

  EXPECT_CALL(*GetMockClient(user_profile),
              ReportBrowserLaunchEvent(_, false, "browser_dm_token", _))
      .WillOnce([](::chrome::cros::reporting::proto::Event wrapper, bool,
                   const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        EXPECT_TRUE(wrapper.has_browser_launch_event());
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  EXPECT_CALL(upload_callback,
              Run(testing::Property(
                  &policy::CloudPolicyClient::Result::IsSuccess, true)));

  uploader.OnProfileAdded(user_profile);
}

TEST_F(BrowserLaunchEventUploaderDesktopTest,
       UploadBrowserEventGuestProfileStartup) {
  SetBrowserManaged(true, "browser_dm_token");

  TestingProfile::Builder builder;
  builder.SetGuestSession();
  builder.AddTestingFactory(
      enterprise_connectors::RealtimeReportingClientFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<MockRealtimeReportingClient>(context);
      }));
  auto* guest_profile =
      profile_manager_->CreateGuestProfile(std::move(builder));

  BrowserLaunchEventUploaderDesktop uploader;

  EXPECT_CALL(*GetMockClient(guest_profile),
              ReportBrowserLaunchEvent(_, false, "browser_dm_token", _))
      .WillOnce([](::chrome::cros::reporting::proto::Event wrapper, bool,
                   const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        EXPECT_TRUE(wrapper.has_browser_launch_event());
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  base::MockCallback<
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>>
      upload_callback;
  EXPECT_CALL(upload_callback,
              Run(testing::Property(
                  &policy::CloudPolicyClient::Result::IsSuccess, true)));

  uploader.UploadEvent(::chrome::cros::reporting::proto::BrowserLaunchEvent(),
                       upload_callback.Get());
}

TEST_F(BrowserLaunchEventUploaderDesktopTest,
       UploadBrowserEventIncognitoProfileStartup) {
  SetBrowserManaged(true, "browser_dm_token");

  // Create a profile without a reporting client, and get its OTR (incognito)
  // profile.
  auto* regular_profile =
      CreateProfile("regular_profile", /*is_managed=*/false,
                    /*is_affiliated=*/false,
                    /*create_reporting_client=*/false);
  regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  BrowserLaunchEventUploaderDesktop uploader;

  base::MockCallback<
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>>
      upload_callback;

  // Since incognito profiles do not have a RealtimeReportingClient, upload
  // should be deferred.
  EXPECT_CALL(upload_callback, Run(_)).Times(0);
  uploader.UploadEvent(::chrome::cros::reporting::proto::BrowserLaunchEvent(),
                       upload_callback.Get());
  testing::Mock::VerifyAndClearExpectations(&upload_callback);

  // When a regular profile with a reporting client is added later, it uploads.
  auto* user_profile =
      CreateProfile("user_profile", /*is_managed=*/false,
                    /*is_affiliated=*/false,
                    /*create_reporting_client=*/true);
  EXPECT_CALL(*GetMockClient(user_profile),
              ReportBrowserLaunchEvent(_, false, "browser_dm_token", _))
      .WillOnce([](::chrome::cros::reporting::proto::Event wrapper, bool,
                   const std::string&,
                   base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                       callback) {
        EXPECT_TRUE(wrapper.has_browser_launch_event());
        std::move(callback).Run(
            policy::CloudPolicyClient::Result(policy::DM_STATUS_SUCCESS));
      });

  EXPECT_CALL(upload_callback,
              Run(testing::Property(
                  &policy::CloudPolicyClient::Result::IsSuccess, true)));

  uploader.OnProfileAdded(user_profile);
}

}  // namespace enterprise_reporting
