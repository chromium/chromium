// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_service.h"

#include <memory>

#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_prefs.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "media/base/media_switches.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace kaleidoscope {

namespace {

const char kTestUrl[] =
    "https://chromemediarecommendations-pa.googleapis.com/v1/collections";

const char kTestData[] = "zzzz";

const char kTestAPIKey[] = "apikey";

const char kTestAccessToken[] = "accesstoken";

}  // namespace

class KaleidoscopeServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  KaleidoscopeServiceTest() = default;
  ~KaleidoscopeServiceTest() override = default;
  KaleidoscopeServiceTest(const KaleidoscopeServiceTest& t) = delete;
  KaleidoscopeServiceTest& operator=(const KaleidoscopeServiceTest&) = delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    GetService()->test_url_loader_factory_for_fetcher_ =
        base::MakeRefCounted<::network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    GetService()->clock_ = &clock_;

    feature_list_.InitWithFeatures({}, {media::kKaleidoscopeModuleCacheOnly});
  }

  ::network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  bool RespondToFetch(
      const std::string& response_body,
      net::HttpStatusCode response_code = net::HttpStatusCode::HTTP_OK,
      int net_error = net::OK) {
    auto response_head = ::network::CreateURLResponseHead(response_code);

    bool rv = url_loader_factory()->SimulateResponseForPendingRequest(
        GURL(kTestUrl), ::network::URLLoaderCompletionStatus(net_error),
        std::move(response_head), response_body);
    task_environment()->RunUntilIdle();
    return rv;
  }

  void WaitForRequest() {
    task_environment()->RunUntilIdle();

    ASSERT_TRUE(GetCurrentRequest().url.is_valid());
    EXPECT_EQ(net::HttpRequestHeaders::kPostMethod, GetCurrentRequest().method);
    EXPECT_EQ(GetCurrentlyQueriedHeaderValue("X-Goog-Api-Key"), kTestAPIKey);
    EXPECT_EQ(
        GetCurrentlyQueriedHeaderValue(net::HttpRequestHeaders::kAuthorization),
        base::StrCat({"Bearer ", kTestAccessToken}));
  }

  media::mojom::CredentialsPtr CreateCredentials() {
    auto creds = media::mojom::Credentials::New();
    creds->api_key = kTestAPIKey;
    creds->access_token = kTestAccessToken;
    return creds;
  }

  KaleidoscopeService* GetService() {
    return KaleidoscopeService::Get(profile());
  }

  void MarkFirstRunAsComplete() {
    profile()->GetPrefs()->SetInteger(
        kaleidoscope::prefs::kKaleidoscopeFirstRunCompleted,
        kKaleidoscopeFirstRunLatestVersion);
  }

  base::SimpleTestClock& clock() { return clock_; }

 private:
  std::string GetCurrentlyQueriedHeaderValue(const base::StringPiece& key) {
    std::string out;
    GetCurrentRequest().headers.GetHeader(key, &out);
    return out;
  }

  const ::network::ResourceRequest& GetCurrentRequest() {
    return url_loader_factory()->pending_requests()->front().request;
  }

  ::network::TestURLLoaderFactory url_loader_factory_;

  base::SimpleTestClock clock_;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(KaleidoscopeServiceTest, Success) {
  MarkFirstRunAsComplete();

  base::HistogramTester histogram_tester;

  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_EQ(kTestData, result->response);
            EXPECT_EQ(media::mojom::GetCollectionsResult::kSuccess,
                      result->result);
          }));

  WaitForRequest();
  clock().Advance(base::TimeDelta::FromSeconds(5));
  ASSERT_TRUE(RespondToFetch(kTestData));

  // Wait for the callback to be called.
  histogram_tester.ExpectUniqueTimeSample(
      KaleidoscopeService::kNTPModuleServerFetchTimeHistogramName,
      base::TimeDelta::FromSeconds(5), 1);

  // If we call again then we should hit the cache.
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_EQ(kTestData, result->response);
            EXPECT_EQ(media::mojom::GetCollectionsResult::kSuccess,
                      result->result);
          }));

  task_environment()->RunUntilIdle();
  EXPECT_TRUE(url_loader_factory()->pending_requests()->empty());

  // If we change the GAIA id then we should trigger a refetch.
  GetService()->GetCollections(
      CreateCredentials(), "1234", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_EQ(kTestData, result->response);
            EXPECT_EQ(media::mojom::GetCollectionsResult::kSuccess,
                      result->result);
          }));

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(url_loader_factory()->pending_requests()->empty());
}

TEST_F(KaleidoscopeServiceTest, ServerFail_Forbidden) {
  MarkFirstRunAsComplete();

  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_TRUE(result->response.empty());
            EXPECT_EQ(media::mojom::GetCollectionsResult::kNotAvailable,
                      result->result);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_FORBIDDEN));

  // If we call again then we should hit the cache. HTTP Forbidden is special
  // cased because this indicates the user cannot access Kaleidoscope.
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_TRUE(result->response.empty());
            EXPECT_EQ(media::mojom::GetCollectionsResult::kNotAvailable,
                      result->result);
          }));

  task_environment()->RunUntilIdle();
  EXPECT_TRUE(url_loader_factory()->pending_requests()->empty());
}

TEST_F(KaleidoscopeServiceTest, ServerFail) {
  MarkFirstRunAsComplete();

  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_TRUE(result->response.empty());
            EXPECT_EQ(media::mojom::GetCollectionsResult::kFailed,
                      result->result);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_BAD_REQUEST));

  // If we call again then we should not hit the cache.
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_TRUE(result->response.empty());
            EXPECT_EQ(media::mojom::GetCollectionsResult::kFailed,
                      result->result);
          }));

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(url_loader_factory()->pending_requests()->empty());
}

TEST_F(KaleidoscopeServiceTest, NetworkFail) {
  MarkFirstRunAsComplete();

  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_TRUE(result->response.empty());
            EXPECT_EQ(media::mojom::GetCollectionsResult::kFailed,
                      result->result);
          }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_OK, net::ERR_UNEXPECTED));

  // If we call again then we should not hit the cache.
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindLambdaForTesting(
          [&](media::mojom::GetCollectionsResponsePtr result) {
            EXPECT_TRUE(result->response.empty());
            EXPECT_EQ(media::mojom::GetCollectionsResult::kFailed,
                      result->result);
          }));

  task_environment()->RunUntilIdle();
  EXPECT_FALSE(url_loader_factory()->pending_requests()->empty());
}

TEST_F(KaleidoscopeServiceTest, ForceCache) {
  MarkFirstRunAsComplete();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kKaleidoscopeModuleCacheOnly);

  {
    base::HistogramTester histogram_tester;

    bool resolved = false;
    GetService()->GetCollections(
        CreateCredentials(), "123", "abcd",
        base::BindLambdaForTesting(
            [&](media::mojom::GetCollectionsResponsePtr result) {
              EXPECT_TRUE(result->response.empty());
              EXPECT_EQ(media::mojom::GetCollectionsResult::kFailed,
                        result->result);
              resolved = true;
            }));

    WaitForRequest();

    // Check the callback is resolved before the fetch.
    EXPECT_TRUE(resolved);

    histogram_tester.ExpectUniqueSample(
        KaleidoscopeService::kNTPModuleCacheHitHistogramName,
        KaleidoscopeService::CacheHitResult::kCacheMiss, 1);
  }

  // Resolve the fetch to store the data.
  ASSERT_TRUE(RespondToFetch(kTestData));

  {
    base::HistogramTester histogram_tester;

    // If we call again then we should hit the cache.
    GetService()->GetCollections(
        CreateCredentials(), "123", "abcd",
        base::BindLambdaForTesting(
            [&](media::mojom::GetCollectionsResponsePtr result) {
              EXPECT_EQ(kTestData, result->response);
              EXPECT_EQ(media::mojom::GetCollectionsResult::kSuccess,
                        result->result);
            }));

    task_environment()->RunUntilIdle();
    EXPECT_TRUE(url_loader_factory()->pending_requests()->empty());

    histogram_tester.ExpectUniqueSample(
        KaleidoscopeService::kNTPModuleCacheHitHistogramName,
        KaleidoscopeService::CacheHitResult::kCacheHit, 1);
  }
}

TEST_F(KaleidoscopeServiceTest, FirstRun) {
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindOnce([](media::mojom::GetCollectionsResponsePtr result) {
        EXPECT_TRUE(result->response.empty());
        EXPECT_EQ(media::mojom::GetCollectionsResult::kFirstRun,
                  result->result);
      }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch(kTestData));

  // If we call again then we should hit the cache.
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindOnce([](media::mojom::GetCollectionsResponsePtr result) {
        EXPECT_TRUE(result->response.empty());
        EXPECT_EQ(media::mojom::GetCollectionsResult::kFirstRun,
                  result->result);
      }));

  // A request should not be created.
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(url_loader_factory()->pending_requests()->empty());
}

TEST_F(KaleidoscopeServiceTest, FirstRunNotAvailable) {
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindOnce([](media::mojom::GetCollectionsResponsePtr result) {
        EXPECT_TRUE(result->response.empty());
        EXPECT_EQ(media::mojom::GetCollectionsResult::kNotAvailable,
                  result->result);
      }));

  WaitForRequest();
  ASSERT_TRUE(RespondToFetch("", net::HTTP_FORBIDDEN));

  // If we call again then we should hit the cache. HTTP Forbidden is special
  // cased because this indicates the user cannot access Kaleidoscope.
  GetService()->GetCollections(
      CreateCredentials(), "123", "abcd",
      base::BindOnce([](media::mojom::GetCollectionsResponsePtr result) {
        EXPECT_TRUE(result->response.empty());
        EXPECT_EQ(media::mojom::GetCollectionsResult::kNotAvailable,
                  result->result);
      }));

  // A request should not be created.
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(url_loader_factory()->pending_requests()->empty());
}

}  // namespace kaleidoscope
