// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/almanac_api_util.h"
#include <string>

#include "base/memory/scoped_refptr.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace apps {
namespace {

using AlmanacApiUtilTest = testing::Test;

TEST_F(AlmanacApiUtilTest, GetEndpointUrl) {
  EXPECT_EQ(GetAlmanacEndpointUrl("").spec(),
            "https://chromeosalmanac-pa.googleapis.com/");
  EXPECT_EQ(GetAlmanacEndpointUrl("endpoint").spec(),
            "https://chromeosalmanac-pa.googleapis.com/endpoint");
  EXPECT_EQ(GetAlmanacEndpointUrl("v1/app-preload").spec(),
            "https://chromeosalmanac-pa.googleapis.com/v1/app-preload");
}

TEST_F(AlmanacApiUtilTest, NoDownloadError) {
  auto response_info = network::mojom::URLResponseHead::New();
  response_info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  std::string response_body;
  EXPECT_TRUE(GetDownloadError(net::OK, response_info.get(), &response_body,
                               "histogram")
                  .ok());
  EXPECT_TRUE(
      GetDownloadError(net::OK, nullptr, &response_body, absl::nullopt).ok());
}

TEST_F(AlmanacApiUtilTest, NetDownloadError) {
  auto response_info = network::mojom::URLResponseHead::New();
  response_info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  std::string response_body;
  EXPECT_FALSE(GetDownloadError(net::ERR_INSUFFICIENT_RESOURCES,
                                response_info.get(), &response_body,
                                "histogram")
                   .ok());
  EXPECT_FALSE(GetDownloadError(net::ERR_CONNECTION_FAILED, nullptr,
                                &response_body, absl::nullopt)
                   .ok());
}

TEST_F(AlmanacApiUtilTest, ServerDownloadError) {
  auto response_info = network::mojom::URLResponseHead::New();
  response_info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 404");
  std::string response_body;
  EXPECT_FALSE(GetDownloadError(net::OK, response_info.get(), &response_body,
                                "histogram")
                   .ok());
  response_info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 502");
  EXPECT_FALSE(GetDownloadError(net::OK, response_info.get(), &response_body,
                                absl::nullopt)
                   .ok());
}

TEST_F(AlmanacApiUtilTest, NoRequestBodyDownloadError) {
  auto response_info = network::mojom::URLResponseHead::New();
  response_info->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  std::string* response_body = nullptr;
  EXPECT_FALSE(
      GetDownloadError(net::OK, response_info.get(), response_body, "histogram")
          .ok());
  EXPECT_FALSE(
      GetDownloadError(net::OK, nullptr, response_body, absl::nullopt).ok());
}
}  // namespace
}  // namespace apps
