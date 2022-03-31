// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test support library for response payloads.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_RESPONSE_PAYLOAD_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_RESPONSE_PAYLOAD_H_

#include "base/values.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

// Class to build response based on an input request.
// Example:
//
//   auto simple_successful_response = ResponseBuilder(std::move(request))
//                                       .Build();
//   auto failed_response = ResponseBuilder(std::move(request))
//                                       .SetSuccess(false)
//                                       .SetForceConfirm(true)
//                                       .Build();
class ResponseBuilder {
 public:
  explicit ResponseBuilder(const base::Value::Dict& request);
  explicit ResponseBuilder(base::Value::Dict&& request);
  ResponseBuilder& SetForceConfirm(bool force_confirm);
  ResponseBuilder& SetSuccess(bool success);

  // Because we are focusing on testing here, the build here won't change
  // members of the |ResponseBuilder| instance so that it can be called repeated
  // for convenience. Additionally, this allows us to add more const qualifiers
  // throughout the tests so that we reduce chances of incorrect test code
  // (which itself isn't tested!).
  base::Value::Dict Build() const;

 private:
  // The request that was sent to the server.
  base::Value::Dict request_;
  bool success_ = true;
  bool force_confirm_ = false;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_TEST_RESPONSE_PAYLOAD_H_
