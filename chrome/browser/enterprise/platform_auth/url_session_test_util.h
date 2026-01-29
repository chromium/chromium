// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_

#include <Foundation/Foundation.h>

#include <cstddef>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace url_session_test_util {

// Configuration defining how the mock NSURLSession should behave when a
// task is started.
// Out of |body|, |os_error| and |hang| only a single field should have a
// non-default value. If all the fields keep the default values the response
// will be 200 OK without a body.
class ResponseConfig {
 public:
  ResponseConfig();
  ~ResponseConfig();
  ResponseConfig(const ResponseConfig&) = delete;
  ResponseConfig(ResponseConfig&&);
  ResponseConfig& operator=(ResponseConfig&&);

  // If set, the mock session will return a 200 OK HTTP response with this
  // string as the body.
  std::optional<std::string> body;

  // If true, simulates a low-level OS networking error (e.g., offline)
  // instead of returning an HTTP response.
  bool os_error = false;

  // If true, the request will never complete, simulating a timeout or
  // server hang.
  bool hang = false;

  // Callback invoked immediately when the network task starts.
  base::OnceClosure on_started;

  // Callback invoked when the network task completes (or fails).
  base::OnceClosure on_stopped;
};

// Returns a mock `NSURLSession` configured via `ResponseConfig`.
//
// Example usage:
//   NSURLRequest* ns_request = ...;
//   ResponseConfig config;
//   config.body = "success_result";
//
//   // Create the mock session
//   NSURLSession* session =
//       url_session_test_util::GetTestURLSessionForConfig(std::move(config));
//
//   // This task will now return 200 OK with "success_result" without
//   // reaching the actual network.
//   NSURLSessionDataTask* task = [session dataTaskWithRequest:ns_request];
//   [task resume];
NSURLSession* GetTestURLSessionForConfig(ResponseConfig&& config);

// A RAII helper that sets the global URLSession override upon construction
// and clears it upon destruction. There should always be at most one instance
// of this class. This ensures that the overrides don't leak between the unit
// tests.
class ScopedURLSessionOverrideForTesting {
 public:
  explicit ScopedURLSessionOverrideForTesting(NSURLSession* session_override);
  ~ScopedURLSessionOverrideForTesting();

 private:
  static bool instance_exists_;
};

}  // namespace url_session_test_util

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_URL_SESSION_TEST_UTIL_H_
