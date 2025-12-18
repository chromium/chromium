// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/url_session_helper.h"

#include <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "net/base/apple/url_conversions.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/data_element.h"

namespace url_session_helper {

namespace {

constexpr NSString* kNsOrigin = @"Origin";

// Will ignore headers where conversion between std::string and NSString failed.
// Never returns nil, if all headers were skipped an empty dictionary will be
// returned.
NSMutableDictionary* ConvertHttpRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  NSMutableDictionary* headers_dict = [NSMutableDictionary dictionary];

  for (const auto& header : headers.GetHeaderVector()) {
    NSString* key = base::SysUTF8ToNSString(header.key);
    NSString* value = base::SysUTF8ToNSString(header.value);
    if (key && value) {
      [headers_dict setObject:value forKey:key];
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

}  // namespace

NSURLRequest* ConvertResourceRequest(const network::ResourceRequest& request,
                                     int timeout_in_seconds) {
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
  ns_request.timeoutInterval = timeout_in_seconds;
  ns_request.HTTPBody = body;
  return ns_request;
}

}  // namespace url_session_helper
