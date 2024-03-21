// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_navigation_observer.h"

#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

namespace enterprise_data_protection {

namespace {

safe_browsing::RTLookupResponse::ThreatInfo GetTestThreatInfo(
    std::string watermark_text,
    int64_t timestamp_seconds) {
  safe_browsing::MatchedUrlNavigationRule::WatermarkMessage wm;
  wm.set_watermark_message(watermark_text);
  wm.mutable_timestamp()->set_seconds(timestamp_seconds);

  safe_browsing::RTLookupResponse::ThreatInfo threat_info;
  safe_browsing::MatchedUrlNavigationRule* matched_url_navigation_rule =
      threat_info.mutable_matched_url_navigation_rule();
  *matched_url_navigation_rule->mutable_watermark_message() = wm;
  return threat_info;
}

class FakeRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  FakeRealTimeUrlLookupService() = default;

  // RealTimeUrlLookupServiceBase:
  void StartLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id) override {
    // Create custom threat info instance. The DataProtectionNavigationObserver
    // does not care whether the verdict came from the verdict cache or from an
    // actual lookup request, as long as it gets a verdict back.
    auto response = std::make_unique<safe_browsing::RTLookupResponse>();
    safe_browsing::RTLookupResponse::ThreatInfo* new_threat_info =
        response->add_threat_info();
    *new_threat_info = GetTestThreatInfo("custom_message", 1709181364);

    callback_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(response_callback),
                       /*is_rt_lookup_successful=*/true,
                       /*is_cached_response=*/true, std::move(response)));
  }
};

class DataProtectionNavigationObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    web_contents_ = CreateTestWebContents();
  }

  void TearDown() override {
    web_contents_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 protected:
  FakeRealTimeUrlLookupService lookup_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  const GURL kTestURL = GURL("https://test");
};

}  // namespace

TEST_F(DataProtectionNavigationObserverTest, TestWatermarkTextUpdated) {
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      kTestURL, web_contents()->GetPrimaryMainFrame());

  // DataProtectionNavigationObserver does not implement DidStartNavigation(),
  // this is called by BrowserView. So we simply call Start() and manually
  // construct the class using the navigation handle that is provided once
  // Start() is called.
  simulator->Start();
  content::NavigationHandle* navigation_handle =
      simulator->GetNavigationHandle();
  base::test::TestFuture<const std::string&> future;

  // The DataProtectionNavigationObserver needs to be constructed using
  // CreateForNavigationHandle to allow for proper lifetime management of the
  // object, since we call DeleteForNavigationHandle() in our
  // DidFinishNavigation() override.
  enterprise_data_protection::DataProtectionNavigationObserver::
      CreateForNavigationHandle(*navigation_handle, &lookup_service_,
                                navigation_handle->GetWebContents(),
                                future.GetCallback());

  // Call DidFinishNavigation() navigation, which should invoke our callback.
  simulator->Commit();

  std::string watermark_text = future.Get();
  ASSERT_EQ(watermark_text, "custom_message\n\n2024-02-29T04:36:04.000Z");
}

namespace {

struct WatermarkStringParams {
  WatermarkStringParams(std::string identifier,
                        std::string custom_message,
                        int64_t timestamp_seconds,
                        std::string expected)
      : identifier(identifier),
        custom_message(custom_message),
        timestamp_seconds(timestamp_seconds),
        expected(expected) {}

  std::string identifier;
  std::string custom_message;
  int64_t timestamp_seconds;
  std::string expected;
};

class WatermarkStringTest
    : public testing::TestWithParam<WatermarkStringParams> {};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    WatermarkStringTest,
    WatermarkStringTest,
    testing::Values(
        WatermarkStringParams(
            "example@email.com",
            "custom_message",
            1709181364,
            "custom_message\nexample@email.com\n2024-02-29T04:36:04.000Z"),
        WatermarkStringParams(
            "<device-id>",
            "custom_message",
            1709181364,
            "custom_message\n<device-id>\n2024-02-29T04:36:04.000Z"),
        WatermarkStringParams("example@email.com",
                              "",
                              1709181364,
                              "example@email.com\n2024-02-29T04:36:04.000Z")));

TEST_P(WatermarkStringTest, TestGetWatermarkStringFromThreatInfo) {
  safe_browsing::RTLookupResponse::ThreatInfo threat_info = GetTestThreatInfo(
      GetParam().custom_message, GetParam().timestamp_seconds);
  EXPECT_EQ(enterprise_data_protection::GetWatermarkString(
                GetParam().identifier, threat_info),
            GetParam().expected);
}

}  // namespace enterprise_data_protection
