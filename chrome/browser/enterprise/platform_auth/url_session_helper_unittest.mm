// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/url_session_helper.h"

#import <Foundation/Foundation.h>

#include <cstring>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace url_session_helper {

namespace {

NSData* CreateNSDataFromString(const std::string& s) {
  return [NSData dataWithBytes:s.data() length:s.size()];
}

scoped_refptr<network::ResourceRequestBody> CreateBodyFromString(
    absl::string_view content) {
  auto span = base::as_byte_span(content);
  return network::ResourceRequestBody::CreateFromBytes(
      std::vector<uint8_t>(span.begin(), span.end()));
}

constexpr char kUrl[] = "https://example.com/";
constexpr char kMethod[] = "POST";
constexpr char kHeaderKey[] = "X-Custom-Header";
constexpr char kHeaderVal[] = "custom-val";
constexpr char kAnotherHeaderKey[] = "User-Agent";
constexpr char kAnotherHeaderVal[] = "Chrome";
constexpr char kInvalidHeaderName[] = "Invalid@Name";
constexpr char kInitiator[] = "https://initiator.com";
constexpr char kContent[] = "payload";
constexpr char kOrigin[] = "Origin";
constexpr base::TimeDelta kTimeout = base::Seconds(2);
constexpr char kInvalidString[] = "\x80";
constexpr char kContentLength[] = "Content-Length";
constexpr char kContentType[] = "Content-Type";

}  // namespace

TEST(UrlSessionHelperTest, ConvertResourceRequest_NullRequestBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(nil, result.HTTPBody);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_SimpleBytesBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_body = CreateBodyFromString(kContent);
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  ASSERT_NE(nil, result.HTTPBody);
  EXPECT_TRUE([result.HTTPBody
      isEqualToData:CreateNSDataFromString(std::string(kContent))]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_EmptyBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.request_body = base::MakeRefCounted<network::ResourceRequestBody>();
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(0u, result.HTTPBody.length);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_UnsupportedBodyType) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  body->AppendFileRange(base::FilePath(FILE_PATH_LITERAL("filename.txt")), 0,
                        10, base::Time());
  request.request_body = std::move(body);
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(nil, result.HTTPBody);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_BasicHeaders) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  net::HttpRequestHeaders headers;
  headers.SetHeader(kHeaderKey, kHeaderVal);
  headers.SetHeader(kAnotherHeaderKey, kAnotherHeaderVal);
  request.headers = std::move(headers);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(2u, result.allHTTPHeaderFields.count);
  EXPECT_NSEQ(@(kHeaderVal), result.allHTTPHeaderFields[@(kHeaderKey)]);
  EXPECT_NSEQ(@(kAnotherHeaderVal),
              result.allHTTPHeaderFields[@(kAnotherHeaderKey)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_SkipsInvalidHeaders) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  net::HttpRequestHeaders headers;
  headers.SetHeader(kHeaderKey, kHeaderVal);
  headers.SetHeader(kAnotherHeaderKey, kInvalidString);
  request.headers = std::move(headers);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(1u, result.allHTTPHeaderFields.count);
  EXPECT_NSEQ(@(kHeaderVal), result.allHTTPHeaderFields[@(kHeaderKey)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_EmptyResult) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  net::HttpRequestHeaders headers;
  headers.SetHeader(kAnotherHeaderKey, kInvalidString);
  request.headers = std::move(headers);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_EQ(0u, result.allHTTPHeaderFields.count);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_BasicFields) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.headers.SetHeader(kHeaderKey, kHeaderVal);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  ASSERT_NE(nil, result);

  EXPECT_NSEQ([NSURL URLWithString:@(kUrl)], result.URL);
  EXPECT_NSEQ(@(kMethod), result.HTTPMethod);
  EXPECT_EQ(kTimeout.InSeconds(), result.timeoutInterval);

  NSDictionary* headers = result.allHTTPHeaderFields;
  EXPECT_NSEQ(@(kHeaderVal), headers[@(kHeaderKey)]);
  EXPECT_NSEQ(nil, headers[@(kOrigin)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_WithOriginInitiator) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);

  url::Origin origin = url::Origin::Create(GURL(kInitiator));
  request.request_initiator = origin;

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  NSDictionary* headers = result.allHTTPHeaderFields;

  EXPECT_NSEQ(@(kInitiator), headers[@(kOrigin)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_DoesNotOverwriteOrigin) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.headers.SetHeader(kOrigin, kInitiator);

  url::Origin origin = url::Origin::Create(GURL("otherorigin.com"));
  request.request_initiator = origin;

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  NSDictionary* headers = result.allHTTPHeaderFields;
  EXPECT_NSEQ(@(kInitiator), headers[@(kOrigin)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_WithFieldsHeadersAndBody) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kMethod;
  request.headers.SetHeader(kHeaderKey, kHeaderVal);
  request.request_initiator = url::Origin::Create(GURL(kInitiator));
  request.request_body = CreateBodyFromString(kContent);

  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);

  EXPECT_NSEQ([NSURL URLWithString:@(kUrl)], result.URL);
  EXPECT_NSEQ(@(kMethod), result.HTTPMethod);
  EXPECT_EQ(kTimeout.InSeconds(), result.timeoutInterval);

  NSDictionary* headers = result.allHTTPHeaderFields;
  EXPECT_NSEQ(@(kHeaderVal), headers[@(kHeaderKey)]);
  EXPECT_NSEQ(@(kInitiator), headers[@(kOrigin)]);

  ASSERT_NE(nil, result.HTTPBody);
  EXPECT_TRUE([result.HTTPBody isEqualToData:CreateNSDataFromString(kContent)]);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_WithInvalidURL) {
  network::ResourceRequest request;
  request.url = GURL();
  request.method = kMethod;
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  EXPECT_EQ(nil, result);
}

TEST(UrlSessionHelperTest, ConvertResourceRequest_WithInvalidMethod) {
  network::ResourceRequest request;
  request.url = GURL(kUrl);
  request.method = kInvalidString;
  NSURLRequest* result = ConvertResourceRequest(request, kTimeout);
  EXPECT_EQ(nil, result);
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_BasicHttp200) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSDictionary* headers = @{
    @(kContentType) : @"text/html",
    @(kContentLength) : @"42",
    @(kHeaderKey) : @(kHeaderVal)
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];
  network::mojom::URLResponseHeadPtr head = ConvertNSURLResponse(response);

  ASSERT_TRUE(head);
  EXPECT_EQ(head->mime_type, "text/html");
  EXPECT_EQ(head->content_length, 42);
  EXPECT_TRUE(head->network_accessed);

  ASSERT_TRUE(head->headers);
  EXPECT_EQ(head->headers->response_code(), 200);
  EXPECT_EQ(head->headers->GetStatusLine(), "HTTP/1.1 200 OK");
  ASSERT_TRUE(head->headers->HasHeader(kHeaderKey));
  EXPECT_EQ(head->headers->GetNormalizedHeader(kHeaderKey).value(), kHeaderVal);
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_Http404) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:404
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:@{}];

  network::mojom::URLResponseHeadPtr head = ConvertNSURLResponse(response);

  ASSERT_TRUE(head);
  ASSERT_TRUE(head->headers);
  EXPECT_EQ(head->headers->response_code(), 404);
}

TEST(UrlSessionHelperTest, ConvertNSURLResponse_FiltersInvalidHeaders) {
  NSURL* url = [NSURL URLWithString:@(kUrl)];
  NSDictionary* headers = @{
    @(kHeaderKey) : @(kHeaderVal),
    @(kInvalidHeaderName) : @(kHeaderVal),
  };
  NSHTTPURLResponse* response =
      [[NSHTTPURLResponse alloc] initWithURL:url
                                  statusCode:200
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:headers];

  network::mojom::URLResponseHeadPtr head = ConvertNSURLResponse(response);

  ASSERT_TRUE(head->headers);
  EXPECT_TRUE(head->headers->HasHeader(kHeaderKey));
  EXPECT_FALSE(head->headers->HasHeader(kInvalidHeaderName));
  EXPECT_FALSE(head->headers->HasHeader(kAnotherHeaderKey));
}

}  // namespace url_session_helper
