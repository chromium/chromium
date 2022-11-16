// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_event_details.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(WebRequestEventDetailsTest, SetResponseHeaders) {
  const int kFilter =
      extension_web_request_api_helpers::ExtraInfoSpec::RESPONSE_HEADERS;

  char headers_string[] =
      "HTTP/1.0 200 OK\r\n"
      "Key1: Value1\r\n"
      "X-Chrome-ID-Consistency-Response: Value2\r\n"
      "\r\n";
  auto headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers_string));

  {
    // Non-Gaia URL.
    WebRequestInfoInitParams params;
    params.url = GURL("http://www.example.com");
    WebRequestInfo request_info(std::move(params));
    WebRequestEventDetails details(request_info, kFilter);
    details.SetResponseHeaders(request_info, headers.get());
    base::Value::Dict dict =
        details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    base::Value* filtered_headers = dict.Find("responseHeaders");
    ASSERT_TRUE(filtered_headers);
    ASSERT_EQ(2u, filtered_headers->GetList().size());
    EXPECT_EQ("Key1",
              filtered_headers->GetList()[0].FindKey("name")->GetString());
    EXPECT_EQ("Value1",
              filtered_headers->GetList()[0].FindKey("value")->GetString());
    EXPECT_EQ("X-Chrome-ID-Consistency-Response",
              filtered_headers->GetList()[1].FindKey("name")->GetString());
    EXPECT_EQ("Value2",
              filtered_headers->GetList()[1].FindKey("value")->GetString());
  }

  {
    // Gaia URL.
    WebRequestInfoInitParams params;
    params.url = GaiaUrls::GetInstance()->gaia_url();
    WebRequestInfo gaia_request_info(std::move(params));
    WebRequestEventDetails gaia_details(gaia_request_info, kFilter);
    gaia_details.SetResponseHeaders(gaia_request_info, headers.get());
    base::Value::Dict dict =
        gaia_details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    base::Value* filtered_headers = dict.Find("responseHeaders");
    ASSERT_TRUE(filtered_headers);
    ASSERT_EQ(1u, filtered_headers->GetList().size());
    EXPECT_EQ("Key1",
              filtered_headers->GetList()[0].FindKey("name")->GetString());
    EXPECT_EQ("Value1",
              filtered_headers->GetList()[0].FindKey("value")->GetString());
  }
}

}  // namespace extensions
