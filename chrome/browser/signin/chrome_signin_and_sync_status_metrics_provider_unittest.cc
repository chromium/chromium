// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_and_sync_status_metrics_provider.h"

#include <optional>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace {

class ChromeSigninAndSyncStatusMetricsProviderTest : public testing::Test {
 public:
  ChromeSigninAndSyncStatusMetricsProviderTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

// Regression test for crbug.com/506208912.
TEST_F(ChromeSigninAndSyncStatusMetricsProviderTest,
       ProvideCurrentSessionDataWithProfileWithoutBrowserWindows) {
  TestingProfile::Builder profile_builder;
  profile_builder.DisallowBrowserWindows();
  Profile* profile = profile_manager_.CreateGuestProfile(
      std::make_optional<TestingProfile::Builder>(std::move(profile_builder)));
  ASSERT_NE(profile, nullptr);
  ASSERT_FALSE(profile->AllowsBrowserWindows());
  ASSERT_EQ(nullptr, ProfileBrowserCollection::GetForProfile(profile));

  ChromeSigninAndSyncStatusMetricsProvider provider;
  metrics::ChromeUserMetricsExtension uma_proto;
  provider.ProvideCurrentSessionData(&uma_proto);
}

}  // namespace
