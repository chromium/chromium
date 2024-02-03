// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/test_proto_loader.h"

namespace base::test {
namespace {

const base::TestProtoSetLoader& EmptyMessageLoader() {
  // A proto descriptor set with a single "message E{}".
  const char kEmptyDescriptor[] = {
      0x0a, 0x08, 0x0a, 0x01, 0x74, 0x22, 0x03, 0x0a, 0x01, 0x45,
  };
  static const base::TestProtoSetLoader loader(
      std::string_view(kEmptyDescriptor, sizeof(kEmptyDescriptor)));
  return loader;
}

}  // namespace

std::string BinaryProtoToRawTextProto(const std::string& binary_message) {
  // This just serializes binary_message into an empty protobuf message. All
  // content is interpreted as unknown fields, and reflected in the resulting
  // text format.
  return EmptyMessageLoader().PrintToText("E", binary_message);
}

}  // namespace base::test
