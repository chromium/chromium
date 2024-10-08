// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_debug_report_fetcher.h"

#include "base/base64.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/rotation_debug_info.pb.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSessionId[] = "test_session_id";

class BoundSessionRefreshCookieDebugReportFetcherTest : public testing::Test {
 public:
  const GURL kRefreshUrl{"https://accounts.google.com/RotateCookies"};

  std::unique_ptr<BoundSessionRefreshCookieDebugReportFetcher> CreateFetcher(
      bound_session_credentials::RotationDebugInfo debug_info) {
    return std::make_unique<BoundSessionRefreshCookieDebugReportFetcher>(
        test_url_loader_factory_.GetSafeWeakWrapper(), kSessionId, kRefreshUrl,
        /*is_off_the_record_profile=*/false, std::move(debug_info));
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(BoundSessionRefreshCookieDebugReportFetcherTest, Fetch) {
  bound_session_credentials::RotationDebugInfo debug_info;
  debug_info.set_termination_reason(
      bound_session_credentials::RotationDebugInfo::
          TERMINATION_HEADER_RECEIVED);
  std::unique_ptr<BoundSessionRefreshCookieDebugReportFetcher> fetcher =
      CreateFetcher(debug_info);
  base::test::TestFuture<BoundSessionRefreshCookieFetcher::Result> future;
  constexpr char kDebugChallengeResponse[] = "debug_response";
  fetcher->Start(future.GetCallback(), kDebugChallengeResponse);

  EXPECT_EQ(test_url_loader_factory().total_requests(), 1u);
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      test_url_loader_factory().GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url, kRefreshUrl);
  EXPECT_EQ(pending_request->request.method, "GET");
  EXPECT_EQ(pending_request->request.credentials_mode,
            network::mojom::CredentialsMode::kInclude);
  auto headers = pending_request->request.headers;
  EXPECT_EQ(headers.GetHeader("Sec-Session-Google-Response"),
            kDebugChallengeResponse);
  // Verify the debug header.
  std::string sent_debug_info_str;
  ASSERT_TRUE(base::Base64Decode(
      headers.GetHeader("Sec-Session-Google-Rotation-Debug-Info")
          .value_or(std::string()),
      &sent_debug_info_str));
  bound_session_credentials::RotationDebugInfo sent_debug_info;
  ASSERT_TRUE(sent_debug_info.ParseFromString(sent_debug_info_str));
  // Fetcher should set the request time when starting the request, so update
  // the expectation too.
  *debug_info.mutable_request_time() =
      bound_session_credentials::TimeToTimestamp(base::Time::Now());
  EXPECT_THAT(sent_debug_info, base::test::EqualsProto(debug_info));

  test_url_loader_factory().SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "");
  EXPECT_TRUE(future.IsReady());
}

}  // namespace
