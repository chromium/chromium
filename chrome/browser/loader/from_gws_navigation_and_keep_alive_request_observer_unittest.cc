// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using content::MockNavigationHandle;
using testing::_;
using testing::Eq;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;

constexpr char kRequestCategoryPrefix[] = "test-prefix";
constexpr char kTestRequestUrl[] = "https://example.com";
constexpr char kTestGoogleSearchUrl[] = "https://www.google.com/search?q=test";
constexpr char kTestNonGoogleSearchUrl[] = "https://example.org";

GURL GetCategoryUrl(std::string_view base_url, std::string_view category) {
  const std::string sep =
      (std::string(base_url).find("?") == std::string::npos) ? "?" : "&";
  return GURL(base::StrCat({base_url, sep, "category=", category}));
}

class MockFromGWSNavigationAndKeepAliveRequestTracker
    : public FromGWSNavigationAndKeepAliveRequestTracker {
 public:
  explicit MockFromGWSNavigationAndKeepAliveRequestTracker(
      content::BrowserContext* context)
      : FromGWSNavigationAndKeepAliveRequestTracker(context) {}
  MockFromGWSNavigationAndKeepAliveRequestTracker(
      const MockFromGWSNavigationAndKeepAliveRequestTracker& other) = delete;
  MockFromGWSNavigationAndKeepAliveRequestTracker& operator=(
      const MockFromGWSNavigationAndKeepAliveRequestTracker&) = delete;

  static std::unique_ptr<
      NiceMock<MockFromGWSNavigationAndKeepAliveRequestTracker>>
  Create(content::BrowserContext* context) {
    return std::make_unique<
        NiceMock<MockFromGWSNavigationAndKeepAliveRequestTracker>>(context);
  }

  MOCK_METHOD(
      void,
      TrackNavigation,
      (content::GlobalRenderFrameHostId, int64_t, ukm::SourceId, int64_t),
      (override));
  MOCK_METHOD(void,
              TrackKeepAliveRequest,
              (content::GlobalRenderFrameHostId,
               int64_t,
               ukm::SourceId,
               base::UnguessableToken),
              (override));
};

}  // namespace

class FromGWSNavigationAndKeepAliveRequestObserverTestBase
    : public ChromeRenderViewHostTestHarness {
 public:
  using FeaturesType = std::vector<base::test::FeatureRefAndParams>;

  FromGWSNavigationAndKeepAliveRequestObserverTestBase()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    DeleteContents();
    SetContents(CreateTestWebContents());
  }

  void enable_feature() {
    static const FeaturesType enabled_features = {
        {page_load_metrics::features::kBeaconLeakageLogging,
         {{"category_prefix", kRequestCategoryPrefix}}}};
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  void disable_feature() {
    scoped_feature_list_.InitAndDisableFeature(
        page_load_metrics::features::kBeaconLeakageLogging);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using MaybeFromGWSNavigationAndKeepAliveRequestObserverForWebContentsTest =
    FromGWSNavigationAndKeepAliveRequestObserverTestBase;

TEST_F(MaybeFromGWSNavigationAndKeepAliveRequestObserverForWebContentsTest,
       FeatureDisabled) {
  disable_feature();
  EXPECT_THAT(
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreateForWebContents(
          web_contents()),
      IsNull());
}

TEST_F(MaybeFromGWSNavigationAndKeepAliveRequestObserverForWebContentsTest,
       FeatureEnabled) {
  enable_feature();

  EXPECT_THAT(
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreateForWebContents(
          web_contents()),
      NotNull());
}

TEST_F(MaybeFromGWSNavigationAndKeepAliveRequestObserverForWebContentsTest,
       NullWebContents) {
  enable_feature();

  EXPECT_THAT(
      FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreateForWebContents(
          /*web_contents=*/nullptr),
      IsNull());
}

class FromGWSNavigationAndKeepAliveRequestObserverTest
    : public FromGWSNavigationAndKeepAliveRequestObserverTestBase {
 protected:
  void SetUp() override {
    enable_feature();
    FromGWSNavigationAndKeepAliveRequestObserverTestBase::SetUp();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    tracker_ =
        FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetInstance()
            ->SetTestingSubclassFactoryAndUse(
                web_contents()->GetBrowserContext(),
                base::BindOnce(
                    &MockFromGWSNavigationAndKeepAliveRequestTracker::Create));
  }

  void TearDown() override {
    tracker_ = nullptr;
    ukm_recorder_.reset();
    FromGWSNavigationAndKeepAliveRequestObserverTestBase::TearDown();
  }

  std::unique_ptr<FromGWSNavigationAndKeepAliveRequestObserver> CreateObserver()
      const {
    return FromGWSNavigationAndKeepAliveRequestObserver::
        MaybeCreateForWebContents(web_contents());
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

  MockFromGWSNavigationAndKeepAliveRequestTracker* tracker() {
    return tracker_;
  }

  ukm::SourceId CreateUkmSourceId() const {
    return ukm_recorder_->GetNewSourceID();
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  // Owned by `FromGWSNavigationAndKeepAliveRequestTrackerFactory`.
  raw_ptr<MockFromGWSNavigationAndKeepAliveRequestTracker> tracker_ = nullptr;
};

// No initiator page.
TEST_F(FromGWSNavigationAndKeepAliveRequestObserverTest,
       DidStartNavigationWithoutInitiator) {
  std::string category = "test-prefix10";
  auto observer = CreateObserver();
  ASSERT_THAT(observer, NotNull());

  // Prepares a navigation with category ID.
  auto navigation_target = GetCategoryUrl(kTestNonGoogleSearchUrl, category);
  auto handle = CreateMockNavigationHandle(navigation_target);

  EXPECT_CALL(*tracker(), TrackNavigation).Times(0);
  EXPECT_CALL(*tracker(), TrackKeepAliveRequest).Times(0);
  observer->DidStartNavigation(handle.get());
}

// The navigation is not initiated from a Google SRP.
TEST_F(FromGWSNavigationAndKeepAliveRequestObserverTest,
       DidStartNavigationWithoutSRPIniator) {
  std::string category = "test-prefix10";
  // Navigates to a non-SRP.
  auto non_srp_page_url = GURL(kTestNonGoogleSearchUrl);
  NavigateAndCommit(non_srp_page_url);
  auto observer = CreateObserver();
  ASSERT_THAT(observer, NotNull());

  // Prepares a navigation with category ID.
  auto navigation_target = GetCategoryUrl(kTestNonGoogleSearchUrl, category);
  auto handle = CreateMockNavigationHandle(navigation_target);
  handle->set_initiator_frame_token(&main_rfh()->GetFrameToken());
  handle->set_initiator_process_id(main_rfh()->GetProcess()->GetDeprecatedID());

  EXPECT_CALL(*tracker(), TrackNavigation).Times(0);
  EXPECT_CALL(*tracker(), TrackKeepAliveRequest).Times(0);
  observer->DidStartNavigation(handle.get());
}

// The navigation URL does not have a desired category ID.
TEST_F(FromGWSNavigationAndKeepAliveRequestObserverTest,
       DidStartNavigationWithoutCategoryId) {
  std::string category = "invalid-prefix10";
  // Navigates to a SRP;
  auto page_url = GURL(kTestGoogleSearchUrl);
  NavigateAndCommit(page_url);
  auto observer = CreateObserver();
  ASSERT_THAT(observer, NotNull());

  // Prepares a navigation without category ID.
  auto navigation_target = GURL(kTestNonGoogleSearchUrl);
  auto handle = CreateMockNavigationHandle(navigation_target);
  handle->set_initiator_frame_token(&main_rfh()->GetFrameToken());
  handle->set_initiator_process_id(main_rfh()->GetProcess()->GetDeprecatedID());

  EXPECT_CALL(*tracker(), TrackNavigation).Times(0);
  EXPECT_CALL(*tracker(), TrackKeepAliveRequest).Times(0);
  observer->DidStartNavigation(handle.get());
}

TEST_F(FromGWSNavigationAndKeepAliveRequestObserverTest, DidStartNavigation) {
  std::string category = "test-prefix10";
  // Navigates to a SRP.
  auto page_url = GURL(kTestGoogleSearchUrl);
  NavigateAndCommit(page_url);
  auto observer = CreateObserver();
  ASSERT_THAT(observer, NotNull());

  // Prepares a navigation with category ID.
  auto navigation_target = GetCategoryUrl(kTestNonGoogleSearchUrl, category);
  auto handle = CreateMockNavigationHandle(navigation_target);
  handle->set_initiator_frame_token(&main_rfh()->GetFrameToken());
  handle->set_initiator_process_id(main_rfh()->GetProcess()->GetDeprecatedID());

  EXPECT_CALL(*tracker(), TrackNavigation(Eq(main_rfh()->GetGlobalId()), Eq(10),
                                          Eq(main_rfh()->GetPageUkmSourceId()),
                                          Eq(handle->GetNavigationId())))
      .Times(1);
  EXPECT_CALL(*tracker(), TrackKeepAliveRequest).Times(0);
  observer->DidStartNavigation(handle.get());
}

// The keepalive request is not initiated from a Google SRP.
TEST_F(FromGWSNavigationAndKeepAliveRequestObserverTest,
       OnKeepAliveRequestCreatedWithoutSRPIniator) {
  std::string category = "test-prefix10";
  // Navigates to a non-SRP.
  auto non_srp_page_url = GURL(kTestNonGoogleSearchUrl);
  NavigateAndCommit(non_srp_page_url);
  auto observer = CreateObserver();
  ASSERT_THAT(observer, NotNull());

  // Prepares a keepalive request with category ID.
  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));

  EXPECT_CALL(*tracker(), TrackNavigation).Times(0);
  EXPECT_CALL(*tracker(), TrackKeepAliveRequest).Times(0);
  observer->OnKeepAliveRequestCreated(request, main_rfh());
}

// The keepalive request do not have a desired category ID.
TEST_F(FromGWSNavigationAndKeepAliveRequestObserverTest,
       OnKeepAliveRequestCreatedWithoutRequestCategoryId) {
  std::string invalid_category = "invalid-prefix10";
  // Navigates to a SRP.
  auto page_url = GURL(kTestGoogleSearchUrl);
  NavigateAndCommit(page_url);
  auto observer = CreateObserver();
  ASSERT_THAT(observer, NotNull());

  // Prepares a keepalive request without category ID.
  auto request =
      CreateRequest(GetCategoryUrl(kTestRequestUrl, invalid_category));

  EXPECT_CALL(*tracker(), TrackNavigation).Times(0);
  EXPECT_CALL(*tracker(), TrackKeepAliveRequest).Times(0);
  observer->OnKeepAliveRequestCreated(request, main_rfh());
}

TEST_F(FromGWSNavigationAndKeepAliveRequestObserverTest,
       OnKeepAliveRequestCreated) {
  std::string category = "test-prefix10";
  // Navigates to a SRP.
  auto page_url = GURL(kTestGoogleSearchUrl);
  NavigateAndCommit(page_url);
  auto observer = CreateObserver();
  ASSERT_THAT(observer, NotNull());

  // Prepares a keepalive request with category ID.
  auto request = CreateRequest(GetCategoryUrl(kTestRequestUrl, category));

  EXPECT_CALL(*tracker(), TrackNavigation).Times(0);
  EXPECT_CALL(*tracker(),
              TrackKeepAliveRequest(Eq(main_rfh()->GetGlobalId()), Eq(10),
                                    Eq(main_rfh()->GetPageUkmSourceId()),
                                    Eq(request.keepalive_token.value())))
      .Times(1);
  observer->OnKeepAliveRequestCreated(request, main_rfh());
}
