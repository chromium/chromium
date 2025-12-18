// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_HELPER_H_

#include <Foundation/Foundation.h>

#include "base/memory/scoped_refptr.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace base {
class TimeDelta;
}  // namespace base

// URLSession is Apple's API for performing URL requests. This namespace
// provides a few helper functions to convert between Chrome's network objects
// and Apple's native objects to ease the use of the aforementioned API.
namespace url_session_helper {

// This function only takes care of: URL, headers, method, body and timeout.
// Additionally, if the request has request_initiator, then the Origin header
// will be set to its value.
// Returns nil if conversion of URL or method fails.
// Will ignore headers where conversion between std::string and NSString failed.
// Headers are allowlisted, see kRequestHeadersAllowlist in the .mm file for
// details.
// Only supports request body of type network::DataElementBytes, for
// other types body will be set to nil.
NSURLRequest* ConvertResourceRequest(const network::ResourceRequest& request,
                                     base::TimeDelta timeout);

// Only converts: mime_type, content_length, network_accessed
// and http headers.
// Headers are allowlisted, see kResponseHeadersAllowlist in the .mm file for
// details. When converting HTTP headers will use hard-coded HTTP 1.1 for
// simplicity. Assumes response is not nil.
network::mojom::URLResponseHeadPtr ConvertNSURLResponse(
    NSURLResponse* response);

}  // namespace url_session_helper

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_HELPER_H_
