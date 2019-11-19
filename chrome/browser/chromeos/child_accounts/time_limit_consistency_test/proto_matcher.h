// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A gMock matcher for comparing protos and producing a human-readable
// message if the assertion fails.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_PROTO_MATCHER_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_PROTO_MATCHER_H_

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace chromeos {
namespace time_limit_consistency {

MATCHER_P(EqualsProto, message, "equals golden proto") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);

  if (expected_serialized == actual_serialized) {
    return true;
  }

  std::string expected_readable, actual_readable;
  google::protobuf::TextFormat::PrintToString(message, &expected_readable);
  google::protobuf::TextFormat::PrintToString(arg, &actual_readable);

  *result_listener << "\n\noutput parses to: \n----------\n"
                   << actual_readable
                   << "---------\n\n and should be: \n----------\n"
                   << expected_readable << "----------";

  return expected_serialized == actual_serialized;
}

}  // namespace time_limit_consistency
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMIT_CONSISTENCY_TEST_PROTO_MATCHER_H_
