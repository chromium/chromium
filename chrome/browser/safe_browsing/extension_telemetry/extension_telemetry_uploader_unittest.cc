// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_uploader.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/browser/test_safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ExtensionTelemetryUploaderTest : public testing::Test {
 public:
  void OnUploadTestCallback(bool success, const std::string& response_data) {
    upload_success_ = success;
    response_data_ = response_data;
  }

 protected:
  ExtensionTelemetryUploaderTest()
      : upload_data_("dummy report"),
        task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    uploader_ = std::make_unique<ExtensionTelemetryUploader>(
        base::BindOnce(&ExtensionTelemetryUploaderTest::OnUploadTestCallback,
                       base::Unretained(this)),
        test_url_loader_factory_.GetSafeWeakWrapper(),
        std::make_unique<std::string>(upload_data_), nullptr);
  }

  std::string upload_data_;
  bool upload_success_ = false;
  std::string response_data_;
  base::HistogramTester histograms_;
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ExtensionTelemetryUploader> uploader_;
};

TEST_F(ExtensionTelemetryUploaderTest, FetchAccessTokenForReport) {
  auto token_fetcher = std::make_unique<TestSafeBrowsingTokenFetcher>();
  auto* raw_token_fetcher = token_fetcher.get();
  std::string access_token = "testing_access_token";
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_THAT(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            testing::Optional("Bearer " + access_token));
      }));

  test_url_loader_factory.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), upload_data_,
      net::HTTP_OK);
  std::unique_ptr<ExtensionTelemetryUploader> sign_in_uploader =
      std::make_unique<ExtensionTelemetryUploader>(
          base::BindOnce(&ExtensionTelemetryUploaderTest::OnUploadTestCallback,
                         base::Unretained(this)),
          test_url_loader_factory.GetSafeWeakWrapper(),
          std::make_unique<std::string>(upload_data_),
          std::move(token_fetcher));
  sign_in_uploader->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  // Expects token fetcher to be called.
  EXPECT_EQ(raw_token_fetcher->WasStartCalled(), true);
  raw_token_fetcher->RunAccessTokenCallback(access_token);
}

TEST_F(ExtensionTelemetryUploaderTest, AttachZwiebackCookieForReport) {
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // When access token is not fetched, header does not include access
        // token.
        EXPECT_EQ(
            request.headers.GetHeader(net::HttpRequestHeaders::kAuthorization),
            std::nullopt);
        // Set the credential mode to kInclude by default.
        EXPECT_EQ(request.credentials_mode,
                  network::mojom::CredentialsMode::kInclude);
      }));
  test_url_loader_factory.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), upload_data_,
      net::HTTP_OK);
  std::unique_ptr<ExtensionTelemetryUploader> sign_out_uploader =
      std::make_unique<ExtensionTelemetryUploader>(
          base::BindOnce(&ExtensionTelemetryUploaderTest::OnUploadTestCallback,
                         base::Unretained(this)),
          test_url_loader_factory.GetSafeWeakWrapper(),
          std::make_unique<std::string>(upload_data_), nullptr);
  sign_out_uploader->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(ExtensionTelemetryUploaderTest, AbortsWithoutRetries) {
  // Aborts upload without retries if response code < 500
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), upload_data_,
      net::HTTP_BAD_REQUEST);
  uploader_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(upload_success_);
  // Sample count of 1 indicates only 1 request was sent, i.e., no retries.
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.NetworkRequestResponseCodeOrError",
      net::HTTP_BAD_REQUEST, 1);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.UploadSuccess", false, 1);
}

TEST_F(ExtensionTelemetryUploaderTest, AbortsAfterRetries) {
  // Aborts after multiple retries if response code >= 500
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), upload_data_,
      net::HTTP_SERVICE_UNAVAILABLE);
  uploader_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(upload_success_);
  // Sample count of 3 indicates 3 requests were sent, i.e., 2 retries.
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.NetworkRequestResponseCodeOrError",
      net::HTTP_SERVICE_UNAVAILABLE, 3);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.UploadSuccess", false, 1);
}

TEST_F(ExtensionTelemetryUploaderTest, SucceedsWithoutRetries) {
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), upload_data_,
      net::HTTP_OK);
  uploader_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(upload_success_);
  histograms_.ExpectUniqueSample("SafeBrowsing.ExtensionTelemetry.UploadSize",
                                 upload_data_.size(), 1);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.NetworkRequestResponseCodeOrError",
      net::HTTP_OK, 1);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.UploadSuccess", true, 1);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.RetriesTillUploadSuccess", 0, 1);
}

TEST_F(ExtensionTelemetryUploaderTest, SucceedsAfterRetries) {
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), upload_data_,
      net::HTTP_SERVICE_UNAVAILABLE);
  uploader_->Start();
  task_environment_.FastForwardBy(base::Seconds(1));

  // Verify that there were 2 failed requests (1 original + 1 retry).
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.NetworkRequestResponseCodeOrError",
      net::HTTP_SERVICE_UNAVAILABLE, 2);

  // Change server response to success and advance time to allow next retry.
  test_url_loader_factory_.AddResponse(
      ExtensionTelemetryUploader::GetUploadURLForTest(), upload_data_,
      net::HTTP_OK);
  task_environment_.FastForwardUntilNoTasksRemain();

  // Verify that the upload succeeds.
  EXPECT_TRUE(upload_success_);
  histograms_.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.NetworkRequestResponseCodeOrError",
      net::HTTP_OK, 1);
  histograms_.ExpectTotalCount(
      "SafeBrowsing.ExtensionTelemetry.NetworkRequestResponseCodeOrError", 3);
  histograms_.ExpectUniqueSample(
      "SafeBrowsing.ExtensionTelemetry.RetriesTillUploadSuccess", 2, 1);
}

}  // namespace safe_browsing
