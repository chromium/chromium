// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_TEST_REALTIME_EVENT_UPLOADER_TEST_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_TEST_REALTIME_EVENT_UPLOADER_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif

namespace enterprise_reporting {

class MockRealtimeReportingClient
    : public enterprise_connectors::RealtimeReportingClient {
 public:
  explicit MockRealtimeReportingClient(content::BrowserContext* context);
  ~MockRealtimeReportingClient() override;

  MOCK_METHOD(void,
              ReportSaasUsageEvent,
              (::chrome::cros::reporting::proto::Event event,
               bool is_per_profile,
               const std::string& dm_token,
               base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                   upload_callback),
              (override));

  MOCK_METHOD(void,
              ReportBrowserLaunchEvent,
              (::chrome::cros::reporting::proto::Event event,
               bool is_per_profile,
               const std::string& dm_token,
               base::OnceCallback<void(policy::CloudPolicyClient::Result)>
                   upload_callback),
              (override));
};

// Base class for unit tests of real-time event uploaders.
// Provides helpers for creating managed profiles and mocking reporting clients.
class RealtimeEventUploaderTestBase : public testing::Test {
 public:
  RealtimeEventUploaderTestBase();
  ~RealtimeEventUploaderTestBase() override;

 protected:
  void SetUp() override;
  void TearDown() override;

  // Configures the machine to be managed with the given DM token.
  void SetBrowserManaged(bool is_managed,
                         const std::string& dm_token = "browser_dm_token");

  // Creates a testing profile with optional management and affiliation.
  TestingProfile* CreateProfile(const std::string& name,
                                bool is_managed,
                                bool is_affiliated,
                                bool create_reporting_client);

  // Returns the mock reporting client for the given profile.
  MockRealtimeReportingClient* GetMockClient(Profile* profile);

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
#if !BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<policy::FakeBrowserDMTokenStorage>
      fake_browser_dm_token_storage_;
#endif
  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_TEST_REALTIME_EVENT_UPLOADER_TEST_BASE_H_
