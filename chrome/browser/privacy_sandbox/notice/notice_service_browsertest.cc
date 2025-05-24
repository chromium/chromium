// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_factory.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

namespace {

using Notice = privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using NoticeEvent = privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;

class PrivacySandboxNoticeServiceBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDownInProcessBrowserTestFixture() override {
    histogram_tester_.reset();
  }

 protected:
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// TODO(crbug.com/40200835) Enable Restart tests on Android when they're
// supported.

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeServiceBrowserTest,
                       PRE_StartupHistogramsEmitOnlyAfterNoticeInteraction) {
  Profile* profile = GetProfile();
  ASSERT_TRUE(profile && !profile->IsOffTheRecord());

  EXPECT_TRUE(histogram_tester()
                  ->GetTotalCountsForPrefix(
                      "PrivacySandbox.Notice.Startup.LastRecordedEvent.")
                  .empty());

  PrivacySandboxNoticeServiceInterface* notice_service =
      PrivacySandboxNoticeServiceFactory::GetForProfile(profile);
  ASSERT_NE(nullptr, notice_service);
  notice_service->EventOccurred(
      {Notice::kTopicsConsentNotice, SurfaceType::kDesktopNewTab},
      NoticeEvent::kShown);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeServiceBrowserTest,
                       StartupHistogramsEmitOnlyAfterNoticeInteraction) {
  Profile* profile = GetProfile();
  ASSERT_TRUE(profile && !profile->IsOffTheRecord());

  ASSERT_NE(nullptr,
            PrivacySandboxNoticeServiceFactory::GetForProfile(profile));

  EXPECT_FALSE(histogram_tester()
                   ->GetTotalCountsForPrefix(
                       "PrivacySandbox.Notice.Startup.LastRecordedEvent.")
                   .empty());
}

#endif  // BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(PrivacySandboxNoticeServiceBrowserTest,
                       ServiceNotCreatedForOTRProfile) {
  Profile* regular_profile = GetProfile();
  ASSERT_TRUE(regular_profile);
  ASSERT_NE(nullptr,
            PrivacySandboxNoticeServiceFactory::GetForProfile(regular_profile));

  Profile* otr_profile = regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile && otr_profile->IsOffTheRecord());
  EXPECT_EQ(nullptr,
            PrivacySandboxNoticeServiceFactory::GetForProfile(otr_profile));
}

}  // namespace

}  // namespace privacy_sandbox
