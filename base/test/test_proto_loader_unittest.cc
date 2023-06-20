// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_proto_loader.h"

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/test/test.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

base::FilePath GetTestDataRoot() {
  return base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
}

void LoadTestProto(const std::string& proto_text,
                   google::protobuf::MessageLite& proto) {
  base::TestProtoLoader loader(GetTestDataRoot().Append(FILE_PATH_LITERAL(
                                   "base/test/test_proto.descriptor")),
                               "base.test.TestMessage");
  std::string serialized_message;
  loader.ParseFromText(proto_text, serialized_message);
  ASSERT_TRUE(proto.ParseFromString(serialized_message));
}

}  // namespace

TEST(TestProtoLoaderTest, LoadProto) {
  test::TestMessage proto;
  LoadTestProto(
      R"pb(
        test: "Message"
      )pb",
      proto);
  EXPECT_EQ("Message", proto.test());
}

}  // namespace base
