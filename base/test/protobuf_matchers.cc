// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/protobuf_matchers.h"

#include <string>

#include "base/strings/string_view_util.h"
#include "base/test/test_proto_loader.h"

namespace base::test {

std::string BinaryProtoToRawTextProto(const std::string& binary_message) {
  // This just serializes binary_message into an empty protobuf message. All
  // content is interpreted as unknown fields, and reflected in the resulting
  // text format.
  // A proto descriptor set with a single "message E{}".
  constexpr char kEmptyDescriptor[] = {
      0x0a, 0x08, 0x0a, 0x01, 0x74, 0x22, 0x03, 0x0a, 0x01, 0x45,
  };
  return base::TestProtoSetLoader(base::as_string_view(kEmptyDescriptor))
      .PrintToText("E", binary_message);
}

}  // namespace base::test
