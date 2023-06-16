// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_PROTO_LOADER_H_
#define BASE_TEST_TEST_PROTO_LOADER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.pb.h"
#include "third_party/protobuf/src/google/protobuf/dynamic_message.h"

namespace base {

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PROTO_TEST_IMPLEMENTATION)
#define PROTO_TEST_EXPORT __declspec(dllexport)
#else
#define PROTO_TEST_EXPORT __declspec(dllimport)
#endif  // defined(PROTO_TEST_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PROTO_TEST_IMPLEMENTATION)
#define PROTO_TEST_EXPORT __attribute__((visibility("default")))
#else
#define PROTO_TEST_EXPORT
#endif
#endif
#else  // defined(COMPONENT_BUILD)
#define PROTO_TEST_EXPORT
#endif

// This class works around the fact that chrome only includes the lite runtime
// of protobufs. Lite protobufs inherit from |MessageLite| and cannot be used to
// parse from text format. Parsing from text
// format is useful in tests. We cannot include the full version of a protobuf
// in test code because it would clash with the lite version.
//
// This class uses the protobuf descriptors (generated at compile time) to
// generate a |Message| that can be used to parse from text. This message can
// then be serialized to binary which can be parsed by the |MessageLite|.
class PROTO_TEST_EXPORT TestProtoLoader {
 public:
  TestProtoLoader(const base::FilePath& descriptor_path,
                  base::StringPiece type_name);
  ~TestProtoLoader();
  TestProtoLoader(const TestProtoLoader&) = delete;
  TestProtoLoader& operator=(const TestProtoLoader&) = delete;

  void ParseFromText(const std::string& proto_text, std::string& message);
  void PrintToText(const std::string& message, std::string& proto_text);

 private:
  const google::protobuf::Message* GetPrototype(base::FilePath descriptor_path,
                                                base::StringPiece package,
                                                base::StringPiece name);

  google::protobuf::DescriptorPool descriptor_pool_;
  google::protobuf::FileDescriptorSet descriptor_set_;
  google::protobuf::DynamicMessageFactory dynamic_message_factory_;
  raw_ptr<const google::protobuf::Message> prototype_;
};

}  // namespace base

#endif  // BASE_TEST_TEST_PROTO_LOADER_H_
