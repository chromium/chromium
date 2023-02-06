// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/check_deref.h"
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
    const base::Value::List* filtered_headers =
        dict.FindList("responseHeaders");
    ASSERT_TRUE(filtered_headers);
    ASSERT_EQ(2u, filtered_headers->size());
    const base::Value::Dict& first_header =
        CHECK_DEREF(filtered_headers)[0].GetDict();
    const base::Value::Dict& second_header =
        CHECK_DEREF(filtered_headers)[1].GetDict();
    EXPECT_EQ("Key1", CHECK_DEREF(first_header.FindString("name")));
    EXPECT_EQ("Value1", CHECK_DEREF(first_header.FindString("value")));
    EXPECT_EQ("X-Chrome-ID-Consistency-Response",
              CHECK_DEREF(second_header.FindString("name")));
    EXPECT_EQ("Value2", CHECK_DEREF(second_header.FindString("value")));
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
    base::Value::List* filtered_headers = dict.FindList("responseHeaders");
    ASSERT_TRUE(filtered_headers);
    ASSERT_EQ(1u, filtered_headers->size());
    const base::Value::Dict& header =
        CHECK_DEREF(filtered_headers)[0].GetDict();
    EXPECT_EQ("Key1", CHECK_DEREF(header.FindString("name")));
    EXPECT_EQ("Value1", CHECK_DEREF(header.FindString("value")));
  }
}

}  // namespace extensions
