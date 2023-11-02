// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_PROTOBUF_MATCHERS_H_
#define BASE_TEST_PROTOBUF_MATCHERS_H_

#include <string>

#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace base::test {

// Matcher that verifies two protobufs contain the same data.
MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized;
  if (!message.SerializeToString(&expected_serialized)) {
    *result_listener << "Expected proto fails to serialize";
    return false;
  }
  std::string actual_serialized;
  if (!arg.SerializeToString(&actual_serialized)) {
    *result_listener << "Actual proto fails to serialize";
    return false;
  }
  if (expected_serialized != actual_serialized) {
    *result_listener << "Provided proto did not match the expected proto"
                     << "\n Serialized Expected Proto: " << expected_serialized
                     << "\n Serialized Provided Proto: " << actual_serialized;
    return false;
  }
  return true;
}

}  // namespace base::test

#endif  // BASE_TEST_PROTOBUF_MATCHERS_H_
