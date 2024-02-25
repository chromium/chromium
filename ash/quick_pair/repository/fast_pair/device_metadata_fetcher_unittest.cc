// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"

#include <memory>
#include <optional>

#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/mock_http_fetcher.h"
#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kValidResponseEncoded[] =
    "Cr0HCKy4kAMYASJ4aHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnRlbnQuY29tL3dLM3YzcVI1d3"
    "pHMk5JbXBuS1UyYlpfblFpdjh4elJoVDFadWRhT0NhSzlOVzRVS3lZNWtvYlNrSHlxeUJZTzVO"
    "M1h3Um84XzRERkdGcHEtUjNWbW5nKgpQaXhlbCBCdWRzMrQCaW50ZW50OiNJbnRlbnQ7YWN0aW"
    "9uPWNvbS5nb29nbGUuYW5kcm9pZC5nbXMubmVhcmJ5LmRpc2NvdmVyeSUzQUFDVElPTl9NQUdJ"
    "Q19QQUlSO3BhY2thZ2U9Y29tLmdvb2dsZS5hbmRyb2lkLmdtcztjb21wb25lbnQ9Y29tLmdvb2"
    "dsZS5hbmRyb2lkLmdtcy8ubmVhcmJ5LmRpc2NvdmVyeS5zZXJ2aWNlLkRpc2NvdmVyeVNlcnZp"
    "Y2U7Uy5jb20uZ29vZ2xlLmFuZHJvaWQuZ21zLm5lYXJieS5kaXNjb3ZlcnklM0FFWFRSQV9DT0"
    "1QQU5JT05fQVBQPWNvbS5nb29nbGUuYW5kcm9pZC5hcHBzLndlYXJhYmxlcy5tYWVzdHJvLmNv"
    "bXBhbmlvbjtlbmRFmpkZP0pCEkAqaamccP9akybjAVzzvMeRhVAJjHOrKnyKet1/"
    "L9H3JlQQdpFD2w1lclPx5B9I2+mjvU9IHsW2Xlsn6z0+HgdTUgIIA1oMCNye8e4FEIDe/"
    "OIBaAd67QIKeGh0dHBzOi8vbGgzLmdvb2dsZXVzZXJjb250ZW50LmNvbS9POUo4V0dSQzg5Q08"
    "1OEFTamFGbWxjbVM1NWYtMWJJZjJvUWFXVzFyS2REMDZtRlpTS1E3RWlNT3JKSWpjRUtRR0RTL"
    "UlwUmN2TndhUGhvWXA1YVFuZxJ2aHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnRlbnQuY29tL1l"
    "YeWk4Vy1VQkRKYklySU40cnhUQXljR2dSb2lXTWNuUFpsVnJRMU1MTHl1WjJEbVBIdGJzRDIzY"
    "m1leTdJbXlHZWpSbWR2YWZIS0tzbXV6Zm5mVBp5aHR0cHM6Ly9saDMuZ29vZ2xldXNlcmNvbnR"
    "lbnQuY29tL2hXeWdJM2liTjRIV0QtRmp3dlNuV1BIbkdzMUFNMTVLclh2VWx6Tmo2SlJDMmpNM"
    "jZuTFZjY0tYNDFRZEtmOHE3aFN2eVJMYjZMcVJCRDJWVTlQWkNQY4ABAZoBBkdvb2dsZaoBF1B"
    "yZXN0byBFVlQgQWxtb3N0IEJsYWNrsAECugEAGpEBiVBORw0KGgoAAAANSUhEUgAAAAQAAAAEC"
    "AYAAACp8Z5+"
    "AAAABHNCSVQICAgIfAhkiAAAAEhJREFUCJkFwTENgDAQQNGfYIAEXdjARTVUARI6UBMsnU9ADd"
    "zM0Hze2wBKKfda65lzvqhna83eu+qOemWmESEA6jHG+NQK8AOtZCpIT/"
    "9elAAAAABJRU5ErkJggiKjBRInVGFwIHRvIHBhaXIuIEVhcmJ1ZHMgd2lsbCBiZSB0aWVkIHRv"
    "ICVzGhxUYXAgdG8gcGFpciB3aXRoIHRoaXMgZGV2aWNlIhNUYXAgdG8gZmluaXNoIHNldHVwKi"
    "5UYXAgdG8gdXBkYXRlIGRldmljZSBzZXR0aW5ncyBhbmQgZmluaXNoIHNldHVwMj5UYXAgdG8g"
    "ZG93bmxvYWQgZGV2aWNlIGFwcCBvbiBHb29nbGUgUGxheSBhbmQgc2VlIGFsbCBmZWF0dXJlcz"
    "oRVW5hYmxlIHRvIGNvbm5lY3RCIlRyeSBtYW51YWxseSBwYWlyaW5nIHRvIHRoZSBkZXZpY2VK"
    "KCVzIHdpbGwgYXBwZWFyIG9uIGRldmljZXMgbGlua2VkIHdpdGggJXNSIVlvdXIgZGV2aWNlIG"
    "lzIHJlYWR5IHRvIGJlIHNldCB1cFpERG93bmxvYWQgdGhlIGRldmljZSBhcHAgb24gR29vZ2xl"
    "IFBsYXkgdG8gc2VlIGFsbCBhdmFpbGFibGUgZmVhdHVyZXNiGENvbm5lY3QgJXMgdG8gdGhpcy"
    "BwaG9uZWo6U2F2ZSBkZXZpY2UgdG8gJXMgZm9yIGZhc3RlciBwYWlyaW5nIHRvIHlvdXIgb3Ro"
    "ZXIgZGV2aWNlc3IcVGhpcyB3aWxsIHRha2UgYSBmZXcgbW9tZW50c3o3VHJ5IG1hbnVhbGx5IH"
    "BhaXJpbmcgdG8gdGhlIGRldmljZSBieSBnb2luZyB0byBTZXR0aW5nc7IBN0dldCB0aGUgaGFu"
    "ZHMtZnJlZSBoZWxwIG9uIHRoZSBnbyBmcm9tIEdvb2dsZSBBc3Npc3RhbnS6ASNUYXAgdG8gc2"
    "V0IHVwIHlvdXIgR29vZ2xlIEFzc2lzdGFudA==";
constexpr char kEmptyResponse[] = "{}";
constexpr char kInvalidResponse[] = "<html>404 error</html>";
constexpr char kHexDeviceId[] = "07B";
const int kDeviceId = 123;
const char kDeviceMetadataFetchResult[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Result";
const char kDeviceMetadataFetchNetError[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Get.NetError";
const char kDeviceMetadataFetchHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Get.HttpResponseError";

}  // namespace

namespace ash {
namespace quick_pair {

class DeviceMetadataFetcherTest : public testing::Test {
 public:
  void SetUp() override {
    auto mock_http_fetcher = std::make_unique<MockHttpFetcher>();
    mock_http_fetcher_ = mock_http_fetcher.get();
    device_metadata_fetcher_ =
        std::make_unique<DeviceMetadataFetcher>(std::move(mock_http_fetcher));
  }

 protected:
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<DeviceMetadataFetcher> device_metadata_fetcher_;
  raw_ptr<MockHttpFetcher> mock_http_fetcher_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(DeviceMetadataFetcherTest, ValidResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::string decoded;
        base::Base64Decode(kValidResponseEncoded, &decoded);
        std::move(callback).Run(
            std::make_unique<std::string>(decoded),
            std::make_unique<FastPairHttpResult>(net::Error::OK, nullptr));
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce([](std::optional<nearby::fastpair::GetObservedDeviceResponse>
                       response,
                   bool has_retryable_error) {
        ASSERT_EQ("Pixel Buds", response->device().name());
        ASSERT_EQ(
            "https://lh3.googleusercontent.com/"
            "wK3v3qR5wzG2NImpnKU2bZ_"
            "nQiv8xzRhT1ZudaOCaK9NW4UKyY5kobSkHyqyBYO5N3XwRo8_4DFGFpq-R3Vmng",
            response->device().image_url());
        ASSERT_EQ(0.6f, response->device().trigger_distance());
        ASSERT_FALSE(
            response->device().anti_spoofing_key_pair().public_key().empty());
        ASSERT_FALSE(
            response->device().true_wireless_images().left_bud_url().empty());
        ASSERT_FALSE(
            response->device().true_wireless_images().right_bud_url().empty());
        ASSERT_FALSE(
            response->device().true_wireless_images().case_url().empty());
      });

  device_metadata_fetcher_->LookupHexDeviceId(kHexDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(DeviceMetadataFetcherTest, InvalidResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::move(callback).Run(std::make_unique<std::string>(kInvalidResponse),
                                nullptr);
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce(
          [](std::optional<nearby::fastpair::GetObservedDeviceResponse>
                 response,
             bool has_retryable_error) { ASSERT_EQ(std::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(DeviceMetadataFetcherTest, EmptyResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::move(callback).Run(std::make_unique<std::string>(kEmptyResponse),
                                nullptr);
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce(
          [](std::optional<nearby::fastpair::GetObservedDeviceResponse>
                 response,
             bool has_retryable_error) { ASSERT_EQ(std::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(DeviceMetadataFetcherTest, NoResponse) {
  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::move(callback).Run(nullptr, nullptr);
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce(
          [](std::optional<nearby::fastpair::GetObservedDeviceResponse>
                 response,
             bool has_retryable_error) { ASSERT_EQ(std::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();
}

TEST_F(DeviceMetadataFetcherTest, RecordNetError) {
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchResult, 0);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchNetError, 0);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchHttpResponseError, 0);

  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::unique_ptr<FastPairHttpResult> http_result =
            std::make_unique<FastPairHttpResult>(
                /*net_error=*/net::ERR_CERT_COMMON_NAME_INVALID,
                /*head=*/network::CreateURLResponseHead(
                    net::HttpStatusCode::HTTP_NOT_FOUND)
                    .get());
        std::move(callback).Run(std::make_unique<std::string>(kInvalidResponse),
                                std::move(http_result));
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce(
          [](std::optional<nearby::fastpair::GetObservedDeviceResponse>
                 response,
             bool has_retryable_error) { ASSERT_EQ(std::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchResult, 1);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchNetError, 1);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchHttpResponseError, 0);
}

TEST_F(DeviceMetadataFetcherTest, RecordHttpError) {
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchResult, 0);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchNetError, 0);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchHttpResponseError, 0);

  EXPECT_CALL(*mock_http_fetcher_, ExecuteGetRequest)
      .WillOnce([](const GURL& url, FetchCompleteCallback callback) {
        ASSERT_EQ(0u,
                  url.spec().find(
                      "https://nearbydevices-pa.googleapis.com/v1/device/123"));
        std::unique_ptr<FastPairHttpResult> http_result =
            std::make_unique<FastPairHttpResult>(
                /*net_error=*/net::OK,
                /*head=*/network::CreateURLResponseHead(
                    net::HttpStatusCode::HTTP_NOT_FOUND)
                    .get());
        std::move(callback).Run(std::make_unique<std::string>(kInvalidResponse),
                                std::move(http_result));
      });

  base::MockCallback<GetObservedDeviceCallback> callback;
  EXPECT_CALL(callback, Run)
      .WillOnce(
          [](std::optional<nearby::fastpair::GetObservedDeviceResponse>
                 response,
             bool has_retryable_error) { ASSERT_EQ(std::nullopt, response); });

  device_metadata_fetcher_->LookupDeviceId(kDeviceId, callback.Get());
  task_environment.RunUntilIdle();

  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchResult, 1);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchNetError, 0);
  histogram_tester_.ExpectTotalCount(kDeviceMetadataFetchHttpResponseError, 1);
}

}  // namespace quick_pair
}  // namespace ash
