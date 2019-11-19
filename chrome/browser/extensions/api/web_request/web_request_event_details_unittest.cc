// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_details.h"

#include "base/stl_util.h"
#include "base/values.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(WebRequestEventDetailsTest, WhitelistedCopyForPublicSession) {
  // Create original, and populate it with some values.
  std::unique_ptr<WebRequestEventDetails> orig(new WebRequestEventDetails);

  const char* const safe_attributes[] = {
    "method", "requestId", "timeStamp", "type", "tabId", "frameId",
    "parentFrameId", "fromCache", "error", "ip", "statusLine", "statusCode"
  };

  orig->render_process_id_ = 1;
  orig->render_frame_id_ = 2;
  orig->extra_info_spec_ = 3;

  orig->request_body_.reset(new base::DictionaryValue);
  orig->request_headers_.reset(new base::ListValue);
  orig->response_headers_.reset(new base::ListValue);

  for (const char* safe_attr : safe_attributes) {
    orig->dict_.SetString(safe_attr, safe_attr);
  }

  orig->dict_.SetString("url", "http://www.foo.bar/baz");

  // Add some extra dict_ values that should be filtered out.
  orig->dict_.SetString("requestBody", "request body value");
  orig->dict_.SetString("requestHeaders", "request headers value");

  // Get a filtered copy then check that filtering really works.
  std::unique_ptr<WebRequestEventDetails> copy =
      orig->CreatePublicSessionCopy();

  EXPECT_EQ(orig->render_process_id_, copy->render_process_id_);
  EXPECT_EQ(orig->render_frame_id_, copy->render_frame_id_);
  EXPECT_EQ(0, copy->extra_info_spec_);

  EXPECT_EQ(nullptr, copy->request_body_);
  EXPECT_EQ(nullptr, copy->request_headers_);
  EXPECT_EQ(nullptr, copy->response_headers_);

  for (const char* safe_attr : safe_attributes) {
    std::string copy_str;
    copy->dict_.GetString(safe_attr, &copy_str);
    EXPECT_EQ(safe_attr, copy_str);
  }

  // URL is stripped down to origin.
  std::string url;
  copy->dict_.GetString("url", &url);
  EXPECT_EQ("http://www.foo.bar/", url);

  // Extras are filtered out (+1 for url).
  EXPECT_EQ(base::size(safe_attributes) + 1, copy->dict_.size());
}

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
    std::unique_ptr<base::DictionaryValue> dict =
        details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    base::Value* filtered_headers = dict->FindKey("responseHeaders");
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
    std::unique_ptr<base::DictionaryValue> dict =
        gaia_details.GetFilteredDict(kFilter, nullptr, std::string(), false);
    base::Value* filtered_headers = dict->FindKey("responseHeaders");
    ASSERT_TRUE(filtered_headers);
    ASSERT_EQ(1u, filtered_headers->GetList().size());
    EXPECT_EQ("Key1",
              filtered_headers->GetList()[0].FindKey("name")->GetString());
    EXPECT_EQ("Value1",
              filtered_headers->GetList()[0].FindKey("value")->GetString());
  }
}

}  // namespace extensions
