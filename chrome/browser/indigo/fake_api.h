// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_FAKE_API_H_
#define CHROME_BROWSER_INDIGO_FAKE_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace indigo {

class FakeApi {
 public:
  FakeApi();
  FakeApi(const FakeApi&) = delete;
  FakeApi& operator=(const FakeApi&) = delete;
  ~FakeApi();

  // Initializes the embedded test server and starts listening.
  bool InitializeAndListen();

  // Starts accepting connections on the embedded test server.
  void StartAcceptingConnections(int num_requests = 1);

  // Returns the URL to be used for the generate endpoint.
  GURL GetGenerateUrl() const;

  // Waits for a request to arrive at the generate endpoint.
  void WaitForGenerateRequest(size_t index = 0);

  // Sends a successful response with the given image URL.
  void SendSuccessResponse(const GURL& image_url, size_t index = 0);

  // Sends an error response.
  void SendErrorResponse(size_t index = 0);

  // Checks that the request has a valid product image.
  testing::AssertionResult RequestHasValidProductImage(
      base::span<const uint8_t> expected_image_bytes,
      size_t index = 0);

 private:
  net::EmbeddedTestServer test_server_;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      controllable_responses_;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_FAKE_API_H_
