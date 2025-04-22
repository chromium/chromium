// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using content::MockNavigationHandle;
using testing::NiceMock;

constexpr char kRequestCategoryPrefix[] = "test-prefix";
constexpr char kTestRequestUrl[] = "https://example.com";
constexpr char kTestNonGoogleSearchUrl[] = "https://example.org";

GURL GetCategoryUrl(std::string_view base_url, std::string_view category) {
  const std::string sep =
      (std::string(base_url).find("?") == std::string::npos) ? "?" : "&";
  return GURL(base::StrCat({base_url, sep, "category=", category}));
}

std::unique_ptr<KeyedService> BuildFromGWSNavigationAndKeepAliveRequestTracker(
    content::BrowserContext* context) {
  return std::make_unique<FromGWSNavigationAndKeepAliveRequestTracker>(context);
}

}  // namespace

class FromGWSNavigationAndKeepAliveRequestTrackerTest
    : public ChromeRenderViewHostTestHarness,
      public content::NavigationKeepAliveRequestUkmMatcher {
 protected:
  FromGWSNavigationAndKeepAliveRequestTrackerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{page_load_metrics::features::kBeaconLeakageLogging,
          {{"category_prefix", kRequestCategoryPrefix}}}},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    DeleteContents();
    SetContents(CreateTestWebContents());
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetInstance()
        ->SetTestingFactory(
            GetBrowserContext(),
            base::BindRepeating(
                &BuildFromGWSNavigationAndKeepAliveRequestTracker));
  }
  void TearDown() override {
    ukm_recorder_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  network::ResourceRequest CreateRequest(const GURL& url) {
    network::ResourceRequest request;

    request.url = url;
    request.attribution_reporting_eligibility =
        network::mojom::AttributionReportingEligibility::kEmpty;
    request.keepalive = true;
    request.keepalive_token = base::UnguessableToken::Create();

    return request;
  }

  std::unique_ptr<MockNavigationHandle> CreateMockNavigationHandle(
      const GURL& url) {
    return std::make_unique<NiceMock<MockNavigationHandle>>(url, main_rfh());
  }

  FromGWSNavigationAndKeepAliveRequestTracker* GetTracker() {
    return FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetForProfile(
        profile());
  }

  ukm::SourceId GetUkmSourceId() const {
    return ukm_recorder_->GetNewSourceID();
  }

  // KeepAliveRequestUkmMatcher overrides:
  ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest, TrackOnlyNavigation) {
  std::string category = "test-prefix10";
  int64_t category_id = 10;
  auto navigation_handle = CreateMockNavigationHandle(
      GetCategoryUrl(kTestNonGoogleSearchUrl, category));

  GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                GetUkmSourceId(),
                                navigation_handle->GetNavigationId());

  ExpectNoUkm();
}

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest,
       TrackOnlyKeepAliveRequest) {
  std::string category = "test-prefix10";
  int64_t category_id = 10;
  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));

  GetTracker()->TrackKeepAliveRequest(main_rfh()->GetGlobalId(), category_id,
                                      GetUkmSourceId(),
                                      *request.keepalive_token);

  ExpectNoUkm();
}

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest,
       TrackNavigationAndKeepAliveRequestWithDifferentFrameGlobalId) {
  std::string category = "test-prefix10";
  int64_t category_id = 10;
  ukm::SourceId ukm_source_id = GetUkmSourceId();

  auto navigation_handle = CreateMockNavigationHandle(
      GetCategoryUrl(kTestNonGoogleSearchUrl, category));
  GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                ukm_source_id,
                                navigation_handle->GetNavigationId());

  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));
  GetTracker()->TrackKeepAliveRequest(content::GlobalRenderFrameHostId(),
                                      category_id, ukm_source_id,
                                      *request.keepalive_token);

  ExpectNoUkm();
}

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest,
       TrackNavigationAndKeepAliveRequestWithDifferentUkmSourceId) {
  std::string category = "test-prefix10";
  int64_t category_id = 10;

  auto navigation_handle = CreateMockNavigationHandle(
      GetCategoryUrl(kTestNonGoogleSearchUrl, category));
  GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                GetUkmSourceId(),
                                navigation_handle->GetNavigationId());

  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));
  GetTracker()->TrackKeepAliveRequest(main_rfh()->GetGlobalId(), category_id,
                                      GetUkmSourceId(),
                                      *request.keepalive_token);

  ExpectNoUkm();
}

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest,
       TrackNavigationAndKeepAliveRequestWithDifferentCategoryId) {
  {
    std::string category = "test-prefix10";
    int64_t category_id = 10;
    auto navigation_handle = CreateMockNavigationHandle(
        GetCategoryUrl(kTestNonGoogleSearchUrl, category));
    GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                  GetUkmSourceId(),
                                  navigation_handle->GetNavigationId());
  }

  {
    std::string category = "test-prefix20";
    int64_t category_id = 20;
    auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));
    GetTracker()->TrackKeepAliveRequest(main_rfh()->GetGlobalId(), category_id,
                                        GetUkmSourceId(),
                                        *request.keepalive_token);
  }

  ExpectNoUkm();
}

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest,
       TrackNavigationAndKeepAliveRequest) {
  std::string category = "test-prefix10";
  int64_t category_id = 10;
  ukm::SourceId ukm_source_id = GetUkmSourceId();

  auto navigation_handle = CreateMockNavigationHandle(
      GetCategoryUrl(kTestNonGoogleSearchUrl, category));
  GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                ukm_source_id,
                                navigation_handle->GetNavigationId());

  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));
  GetTracker()->TrackKeepAliveRequest(main_rfh()->GetGlobalId(), category_id,
                                      ukm_source_id, *request.keepalive_token);

  ExpectNavigationUkm(category_id, navigation_handle->GetNavigationId(),
                      request.keepalive_token);
}

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest,
       TrackTwoSameNavigationsAndKeepAliveRequest) {
  std::string category = "test-prefix10";
  int64_t category_id = 10;
  ukm::SourceId ukm_source_id = GetUkmSourceId();

  auto navigation_handle = CreateMockNavigationHandle(
      GetCategoryUrl(kTestNonGoogleSearchUrl, category));
  GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                ukm_source_id,
                                navigation_handle->GetNavigationId());
  GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                ukm_source_id,
                                navigation_handle->GetNavigationId());

  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));
  GetTracker()->TrackKeepAliveRequest(main_rfh()->GetGlobalId(), category_id,
                                      ukm_source_id, *request.keepalive_token);

  // Only one pair of navigation and keepalive request is logged.
  ExpectNavigationUkms({
      {/*category_id=*/10, navigation_handle->GetNavigationId(),
       request.keepalive_token},
  });
}

TEST_F(FromGWSNavigationAndKeepAliveRequestTrackerTest,
       TrackNavigationAndTwoSameKeepAliveRequests) {
  std::string category = "test-prefix10";
  int64_t category_id = 10;
  ukm::SourceId ukm_source_id = GetUkmSourceId();

  auto navigation_handle = CreateMockNavigationHandle(
      GetCategoryUrl(kTestNonGoogleSearchUrl, category));
  GetTracker()->TrackNavigation(main_rfh()->GetGlobalId(), category_id,
                                ukm_source_id,
                                navigation_handle->GetNavigationId());

  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));
  GetTracker()->TrackKeepAliveRequest(main_rfh()->GetGlobalId(), category_id,
                                      ukm_source_id, *request.keepalive_token);
  GetTracker()->TrackKeepAliveRequest(main_rfh()->GetGlobalId(), category_id,
                                      ukm_source_id, *request.keepalive_token);

  // Only one pair of navigation and keepalive request is logged.
  ExpectNavigationUkms({
      {/*category_id=*/10, navigation_handle->GetNavigationId(),
       request.keepalive_token},
  });
}
