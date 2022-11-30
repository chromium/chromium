// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/survey/http_client_type.h"

#include <string>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace survey {

class HttpClientTypeUnitTest : public testing::Test {
 public:
  HttpClientTypeUnitTest(const HttpClientTypeUnitTest&) = delete;
  HttpClientTypeUnitTest& operator=(const HttpClientTypeUnitTest&) = delete;

 protected:
  HttpClientTypeUnitTest() {}
  ~HttpClientTypeUnitTest() override {}
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }
  const std::vector<HttpClientType>& client_types() { return client_types_; }

 private:
  base::HistogramTester histogram_tester_;
  const std::vector<HttpClientType> client_types_;
};

TEST_F(HttpClientTypeUnitTest, TestRecordHistogram) {
  std::vector<std::string> client_histogram_suffixes(
      {"Survey", "Notification"});

  std::vector<int> common_response_codes(
      {net::HTTP_ACCEPTED, net::HTTP_CONTINUE, net::HTTP_OK, net::HTTP_CREATED,
       net::HTTP_MULTIPLE_CHOICES, net::HTTP_BAD_REQUEST,
       net::HTTP_UNAUTHORIZED, net::HTTP_NOT_FOUND,
       net::HTTP_INTERNAL_SERVER_ERROR, net::HTTP_TOO_MANY_REQUESTS});

  for (size_t i = 0; i < client_types().size(); i++) {
    std::string histogram_name =
        "Net.HttpResponseCode.CustomHttpClient." + client_histogram_suffixes[i];

    for (const auto& code : common_response_codes) {
      RecordHttpResponseCodeHistogram(client_types()[i], code);
      histogram_tester()->ExpectBucketCount(histogram_name, code, 1);
    }
  }
}

TEST_F(HttpClientTypeUnitTest, TestTrafficAnnotations) {
  std::vector<std::string> annotation_unique_id_pre_hash(
      {"chrome_android_hats", "chime_sdk"});

  for (size_t i = 0; i < client_types().size(); i++) {
    net::NetworkTrafficAnnotationTag annotation_tag =
        GetTrafficAnnotation(client_types()[i]);

    int32_t unique_id_hash_code = COMPUTE_NETWORK_TRAFFIC_ANNOTATION_ID_HASH(
        annotation_unique_id_pre_hash[i].c_str());

    EXPECT_EQ(annotation_tag.unique_id_hash_code, unique_id_hash_code);
  }
}

}  // namespace survey
