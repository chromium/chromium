// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace request_header_integrity {

class RequestHeaderIntegrityURLLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  RequestHeaderIntegrityURLLoaderThrottle();
  RequestHeaderIntegrityURLLoaderThrottle(
      const RequestHeaderIntegrityURLLoaderThrottle&) = delete;
  RequestHeaderIntegrityURLLoaderThrottle& operator=(
      const RequestHeaderIntegrityURLLoaderThrottle&) = delete;

  ~RequestHeaderIntegrityURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

  static bool IsFeatureEnabled();
};

}  // namespace request_header_integrity

#endif  // CHROME_BROWSER_REQUEST_HEADER_INTEGRITY_REQUEST_HEADER_INTEGRITY_URL_LOADER_THROTTLE_H_
