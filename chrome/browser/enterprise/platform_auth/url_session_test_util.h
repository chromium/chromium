// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_

#include <Foundation/Foundation.h>

#include "base/functional/callback.h"
#include "url/gurl.h"

// Provides a test server for URL request performed with Apple's URLSession API.

// Example usage:

// NSURLRequest* ns_request = ...
// NSURLSession* session = url_session_test_util::CreateTestURLSession();
// ResponseConfig config;
// config.body = "result";
// url_session_test_util::AddRequestHandler(ns_request.url,
// std::move(config));
// Now the request will return 200 OK "result" without reaeching the actual
// network.
namespace url_session_test_util {

class ResponseConfig {
 public:
  ResponseConfig();
  ~ResponseConfig();
  ResponseConfig(const ResponseConfig&) = delete;
  ResponseConfig(ResponseConfig&&);
  ResponseConfig& operator=(ResponseConfig&&);

  std::optional<std::string> body;
  bool os_error;
  bool hang;
  base::OnceClosure on_started;
  base::OnceClosure on_stopped;
};

NSURLSession* GetTestURLSessionForConfig(ResponseConfig&& config);

}  // namespace url_session_test_util

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_
