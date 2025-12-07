// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_url_lookup_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kUrl[] = "someurl.com";
constexpr char kIdentifier[] = "someuser@example.com";

safe_browsing::RTLookupResponse::ThreatInfo GetTestThreatInfo(
    int cache_duration_sec) {
  safe_browsing::RTLookupResponse::ThreatInfo threat_info;
  threat_info.set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
  threat_info.set_cache_duration_sec(cache_duration_sec);
  return threat_info;
}

std::unique_ptr<safe_browsing::RTLookupResponse> CreateRTLookupResponse(
    int cache_duration_sec) {
  auto response = std::make_unique<safe_browsing::RTLookupResponse>();
  safe_browsing::RTLookupResponse::ThreatInfo* new_threat_info =
      response->add_threat_info();
  *new_threat_info = GetTestThreatInfo(cache_duration_sec);
  return response;
}

class MockRealTimeUrlLookupService
    : public safe_browsing::testing::FakeRealTimeUrlLookupService {
 public:
  MOCK_METHOD(void, LookupCalled, ());

  void StartMaybeCachedLookup(
      const GURL& url,
      safe_browsing::RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
      SessionID session_id,
      std::optional<safe_browsing::internal::ReferringAppInfo>
          referring_app_info,
      bool use_cache) override {
    LookupCalled();
    std::move(response_callback)
        .Run(true, true, CreateRTLookupResponse(cache_duration_sec_));
  }

  void set_cache_duration_sec(int cache_duration_sec) {
    cache_duration_sec_ = cache_duration_sec;
  }

 private:
  int cache_duration_sec_ = 0;
};

}  // namespace

namespace enterprise_data_protection {

struct UrlLookupTestCase {
  bool verdict_cache_enabled;
  int cache_duration_sec;
  int second_do_lookup_delay_sec;
  int do_lookup_call_count;
};

UrlLookupTestCase kUrlLookupTestCases[] = {{.verdict_cache_enabled = true,
                                            .cache_duration_sec = 90,
                                            .second_do_lookup_delay_sec = 0,
                                            .do_lookup_call_count = 1},
                                           {.verdict_cache_enabled = false,
                                            .cache_duration_sec = 90,
                                            .second_do_lookup_delay_sec = 0,
                                            .do_lookup_call_count = 2},
                                           {.verdict_cache_enabled = true,
                                            .cache_duration_sec = 90,
                                            .second_do_lookup_delay_sec = 100,
                                            .do_lookup_call_count = 2}};

// The RenderViewHostTestHarness is used to obtain a test WebContents instance
class DataProtectionUrlLookupServiceTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<UrlLookupTestCase> {
 public:
  DataProtectionUrlLookupServiceTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

  void CreateLookupService() {
    service_ = std::make_unique<DataProtectionUrlLookupService>();
  }

  DataProtectionUrlLookupService* GetLookupService() {
    if (!service_) {
      CreateLookupService();
    }
    return service_.get();
  }

  // content::RenderViewHostTestHarness:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 private:
  std::unique_ptr<DataProtectionUrlLookupService> service_;
};

TEST_P(DataProtectionUrlLookupServiceTest, VerdictCachePopulated) {
  base::test::ScopedFeatureList scoped_features;
  UrlLookupTestCase test_case = GetParam();

  if (test_case.verdict_cache_enabled) {
    scoped_features.InitAndEnableFeature(
        enterprise_data_protection::kEnableVerdictCache);
  } else {
    scoped_features.InitAndDisableFeature(
        enterprise_data_protection::kEnableVerdictCache);
  }

  SetContents(CreateTestWebContents());

  // create the mock lookup
  MockRealTimeUrlLookupService lookup_service;
  lookup_service.set_cache_duration_sec(test_case.cache_duration_sec);
  EXPECT_CALL(lookup_service, LookupCalled)
      .Times(test_case.do_lookup_call_count);

  // create the service
  auto* dp_lookup_service = GetLookupService();

  // call DoLookup, passing the fake
  GURL url(kUrl);
  dp_lookup_service->DoLookup(&lookup_service, url, kIdentifier,
                              base::DoNothing(), web_contents());

  task_environment()->FastForwardBy(
      base::Seconds(test_case.second_do_lookup_delay_sec));

  // second call to the same url ensure value is fetched from cache
  dp_lookup_service->DoLookup(&lookup_service, url, kIdentifier,
                              base::DoNothing(), web_contents());
}

INSTANTIATE_TEST_SUITE_P(,
                         DataProtectionUrlLookupServiceTest,
                         testing::ValuesIn(kUrlLookupTestCases));

}  // namespace enterprise_data_protection
