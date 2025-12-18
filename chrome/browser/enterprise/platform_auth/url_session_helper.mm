// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/url_session_helper.h"

#include <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "net/base/apple/url_conversions.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_version.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace url_session_helper {

namespace {

constexpr NSString* kNsOrigin = @"Origin";

// These lists contain the minimum required for Okta SSO request to work.
constexpr auto kRequestHeadersAllowlist =
    base::MakeFixedFlatSet<std::string_view>({
        "accept",
        "accept-language",
        "content-type",
        "user-agent",
        "x-okta-user-agent-extended",
    });

constexpr auto kFixedRequestHeaders =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        {"Ceche-Control", "no-cache"},
        {"Pragma", "no-cache"},
        {"Priority", "u=1, i"},
        {"Sec-Fetch-Dest", "empty"},
        {"Sec-Fetch-Mode", "cors"},
        {"Sec-Fetch-Site", "same-origin"},
    });

constexpr auto kResponseHeadersAllowlist =
    base::MakeFixedFlatSet<std::string_view>({
        "accept-ch",
        "access-control-allow-credentials",
        "access-control-allow-headers",
        "access-control-allow-origin",
        "cache-control",
        "content-security-policy",
        "content-security-policy-report-only",
        "content-type",
        "date",
        "expires",
        "referrer-policy",
        "server",
        "strict-transport-security",
        "vary",
        "x-content-type-options",
        "x-okta-request-id",
        "x-rate-limit-limit",
        "x-rate-limit-remaining",
        "x-robots-tag",
    });

// Will ignore headers where conversion between std::string and NSString failed.
// Never returns nil, if all headers were skipped an empty dictionary will be
// returned.
// Headers are allowlisted, moreover a certain fixed set of headers is added.
NSMutableDictionary* ConvertHttpRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  NSMutableDictionary* headers_dict = [NSMutableDictionary dictionary];

  for (const auto& header : headers.GetHeaderVector()) {
    if (kRequestHeadersAllowlist.contains(base::ToLowerASCII(header.key))) {
      NSString* ns_key = base::SysUTF8ToNSString(header.key);
      NSString* ns_value = base::SysUTF8ToNSString(header.value);
      if (ns_key && ns_value) {
        [headers_dict setObject:ns_value forKey:ns_key];
      }
    }
  }

  for (const auto& [key, value] : kFixedRequestHeaders) {
    NSString* ns_key = base::SysUTF8ToNSString(key);
    NSString* ns_value = base::SysUTF8ToNSString(value);
    if (ns_key && ns_value) {
      [headers_dict setObject:ns_value forKey:ns_key];
    }
  }

  return headers_dict;
}

// This function only works with network::DataElementBytes. Returns nil if any
// of the elements are of a different type than bytes.
NSData* ConvertRequestBody(
    const scoped_refptr<network::ResourceRequestBody>& request_body) {
  if (!request_body) {
    return nil;
  }

  const std::vector<network::DataElement>* elements = request_body->elements();
  if (!elements) {
    return nil;
  }

  NSMutableData* body_data = [NSMutableData data];

  for (const auto& data_element : *elements) {
    if (data_element.type() != network::DataElement::Tag::kBytes) {
      return nil;
    }
    base::span<const uint8_t> data_span =
        data_element.As<network::DataElementBytes>().bytes();
    [body_data appendBytes:data_span.data() length:data_span.size()];
  }

  return body_data;
}

// Returns nullptr if status code is invalid.
// Ignores headers with invalid names or values.
scoped_refptr<net::HttpResponseHeaders> ConvertNSHTTPURLResponseToHeaders(
    NSHTTPURLResponse* http_response) {
  std::optional<net::HttpStatusCode> status_code =
      net::TryToGetHttpStatusCode(http_response.statusCode);
  // If the status code returned by URLSession is invalid we return a response
  // without the headers.
  if (!status_code.has_value()) {
    return nullptr;
  }

  const std::string status_line = base::JoinString(
      {base::ToString(status_code.value()),
       net::GetHttpReasonPhrase(std::move(status_code.value()))},
      " ");

  // There is not public accessor for the HTTP version from NSHTTPURLResponse so
  // we use 1.1 by default, it is enough for our needs.
  net::HttpResponseHeaders::Builder builder(net::HttpVersion(1, 1),
                                            status_line);

  NSDictionary<NSString*, NSString*>* headers = http_response.allHeaderFields;

  // Builder.AddHeader() doesn't copy the string, the copy is only made once
  // Build() is called. Because of this we need to make sure the strings are
  // valid until then.
  std::vector<std::pair<std::string, std::string>> headers_to_add;
  headers_to_add.reserve(headers.count);

  for (NSString* key in headers) {
    NSString* value = headers[key];
    std::string header_name = base::SysNSStringToUTF8(key);
    if (!kResponseHeadersAllowlist.contains(base::ToLowerASCII(header_name))) {
      continue;
    }

    std::string header_value = base::SysNSStringToUTF8(value);
    // SysNSStringToUTF8 returns an empty string if argument was nil or
    // invalid.
    if (!header_name.empty() && !header_value.empty() &&
        net::HttpUtil::IsValidHeaderName(header_name) &&
        net::HttpUtil::IsValidHeaderValue(header_value)) {
      headers_to_add.emplace_back(std::move(header_name),
                                  std::move(header_value));
    }
  }

  for (const auto& [key, value] : headers_to_add) {
    builder.AddHeader(key, value);
  }

  return builder.Build();
}

}  // namespace

NSURLRequest* ConvertResourceRequest(const network::ResourceRequest& request,
                                     base::TimeDelta timeout) {
  NSURL* native_url = net::NSURLWithGURL(request.url);
  if (!native_url) {
    return nil;
  }

  NSString* method = base::SysUTF8ToNSString(request.method);
  if (!method) {
    return nil;
  }

  NSData* body = ConvertRequestBody(request.request_body);

  NSMutableDictionary* headers = ConvertHttpRequestHeaders(request.headers);
  // Set the Origin header if missing.
  if (!request.headers.HasHeader(net::HttpRequestHeaders::kOrigin) &&
      request.request_initiator.has_value()) {
    NSString* ns_origin =
        base::SysUTF8ToNSString(request.request_initiator.value().Serialize());
    if (ns_origin) {
      [headers setObject:ns_origin forKey:kNsOrigin];
    }
  }

  NSMutableURLRequest* ns_request =
      [NSMutableURLRequest requestWithURL:native_url];
  ns_request.HTTPMethod = method;
  ns_request.allHTTPHeaderFields = headers;
  ns_request.cachePolicy = NSURLRequestUseProtocolCachePolicy;
  ns_request.timeoutInterval = timeout.InSeconds();
  ns_request.HTTPBody = body;
  return ns_request;
}

network::mojom::URLResponseHeadPtr ConvertNSURLResponse(
    NSURLResponse* ns_response) {
  CHECK(ns_response);
  network::mojom::URLResponseHeadPtr response =
      network::mojom::URLResponseHead::New();

  if (ns_response.MIMEType) {
    response->mime_type = base::SysNSStringToUTF8(ns_response.MIMEType);
  }
  response->content_length = ns_response.expectedContentLength;
  response->network_accessed = true;

  if ([ns_response isKindOfClass:[NSHTTPURLResponse class]]) {
    NSHTTPURLResponse* http_response = (NSHTTPURLResponse*)ns_response;
    response->headers = ConvertNSHTTPURLResponseToHeaders(http_response);
  }

  return response;
}

}  // namespace url_session_helper
