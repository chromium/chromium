// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/realtime_event_upload_helper.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/reporting/test/realtime_event_uploader_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

// Test wrapper that uses composition with RealtimeEventUploadHelper.
class TestRealtimeProfileUploader {
 public:
  explicit TestRealtimeProfileUploader(Profile* profile)
      : helper_("profile", "test_event", profile) {}

  std::optional<RealtimeEventUploadHelper::ReportingContext> Prepare() {
    return helper_.PrepareUpload(/*per_profile=*/true);
  }

 private:
  RealtimeEventUploadHelper helper_;
};

class TestRealtimeBrowserUploader {
 public:
  TestRealtimeBrowserUploader() : helper_("browser", "test_event") {}

  std::optional<RealtimeEventUploadHelper::ReportingContext> Prepare() {
    return helper_.PrepareUpload(/*per_profile=*/false);
  }

 private:
  RealtimeEventUploadHelper helper_;
};

}  // namespace

class RealtimeEventUploadHelperTest : public RealtimeEventUploaderTestBase {};

class RealtimeEventUploadHelperParamTest
    : public RealtimeEventUploaderTestBase,
      public testing::WithParamInterface<bool> {};

TEST_P(RealtimeEventUploadHelperParamTest, ProfileUploaderSuccess) {
  SetBrowserManaged(true);
  bool is_affiliated = GetParam();
  std::string name = is_affiliated ? "affiliated" : "unaffiliated";
  TestingProfile* profile =
      CreateProfile(name, /*is_managed=*/true, is_affiliated,
                    /*create_reporting_client=*/true);

  TestRealtimeProfileUploader uploader(profile);
  auto context = uploader.Prepare();

  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->dm_token, "user_dm_token_" + name);
  EXPECT_TRUE(context->per_profile);
  EXPECT_EQ(
      &context->client.get(),
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          profile));
}

INSTANTIATE_TEST_SUITE_P(All,
                         RealtimeEventUploadHelperParamTest,
                         testing::Bool());

#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(RealtimeEventUploadHelperTest, BrowserUploaderSuccess) {
  SetBrowserManaged(true);

  // Create a profile without a reporting client.
  CreateProfile("no_client", /*is_managed=*/false, /*is_affiliated=*/false,
                /*create_reporting_client=*/false);

  // Create another profile with a reporting client.
  auto* profile_with_client =
      CreateProfile("with_client", /*is_managed=*/false,
                    /*is_affiliated=*/false, /*create_reporting_client=*/true);

  // Browser-level reporting should discover and use the available client.
  TestRealtimeBrowserUploader uploader;
  auto context = uploader.Prepare();

  ASSERT_TRUE(context.has_value());
  EXPECT_EQ(context->dm_token, "browser_dm_token");
  EXPECT_FALSE(context->per_profile);
  EXPECT_EQ(
      &context->client.get(),
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          profile_with_client));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(RealtimeEventUploadHelperTest, NoClientFailure) {
  SetBrowserManaged(true);
  auto* profile = CreateProfile("user", /*is_managed=*/true,
                                /*is_affiliated=*/false,
                                /*create_reporting_client=*/false);

  // Both profile and browser uploaders should fail if no client is found.
  TestRealtimeProfileUploader profile_uploader(profile);
  EXPECT_FALSE(profile_uploader.Prepare().has_value());

  TestRealtimeBrowserUploader browser_uploader;
  EXPECT_FALSE(browser_uploader.Prepare().has_value());
}

TEST_F(RealtimeEventUploadHelperTest, NoTokenFailure) {
  // Unmanaged browser/profile will result in no DM tokens.
  SetBrowserManaged(false);
  auto* profile = CreateProfile("user", /*is_managed=*/false,
                                /*is_affiliated=*/false,
                                /*create_reporting_client=*/true);

  // Both profile and browser uploaders should fail if no DM token is found.
  TestRealtimeProfileUploader profile_uploader(profile);
  EXPECT_FALSE(profile_uploader.Prepare().has_value());

  TestRealtimeBrowserUploader browser_uploader;
  EXPECT_FALSE(browser_uploader.Prepare().has_value());
}

}  // namespace enterprise_reporting
