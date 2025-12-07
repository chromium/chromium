// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_proto_loader.h"

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.h"
#include "third_party/protobuf/src/google/protobuf/descriptor.pb.h"
#include "third_party/protobuf/src/google/protobuf/descriptor_database.h"
#include "third_party/protobuf/src/google/protobuf/dynamic_message.h"
#include "third_party/protobuf/src/google/protobuf/message.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace base {

class TestProtoSetLoader::State {
 public:
  explicit State(std::string_view descriptor_binary_proto) {
    CHECK(descriptor_set_.ParseFromArray(descriptor_binary_proto.data(),
                                         descriptor_binary_proto.size()))
        << "Couldn't parse descriptor";
    for (auto& file : descriptor_set_.file()) {
      descriptor_database_.Add(file);
    }
    descriptor_pool_ = std::make_unique<google::protobuf::DescriptorPool>(
        &descriptor_database_);
  }

  std::unique_ptr<google::protobuf::Message> NewMessage(
      std::string_view full_type_name) {
    const google::protobuf::Descriptor* message_type =
        descriptor_pool_->FindMessageTypeByName(std::string(full_type_name));
    CHECK(message_type) << "Couldn't find proto message type "
                        << full_type_name;
    return base::WrapUnique(
        dynamic_message_factory_.GetPrototype(message_type)->New());
  }

 private:
  google::protobuf::FileDescriptorSet descriptor_set_;
  google::protobuf::SimpleDescriptorDatabase descriptor_database_;
  std::unique_ptr<google::protobuf::DescriptorPool> descriptor_pool_;
  google::protobuf::DynamicMessageFactory dynamic_message_factory_;
};

TestProtoSetLoader::TestProtoSetLoader(std::string_view descriptor_binary_proto)
    : state_(std::make_unique<State>(descriptor_binary_proto)) {}

TestProtoSetLoader::TestProtoSetLoader(const base::FilePath& descriptor_path) {
  std::string file_contents;
  CHECK(base::ReadFileToString(descriptor_path, &file_contents))
      << "Couldn't load contents of " << descriptor_path;
  state_ = std::make_unique<State>(file_contents);
}

TestProtoSetLoader::~TestProtoSetLoader() = default;

std::string TestProtoSetLoader::ParseFromText(
    std::string_view type_name,
    const std::string& proto_text) const {
  // Create a message of the given type, parse, and return.
  std::unique_ptr<google::protobuf::Message> message =
      state_->NewMessage(type_name);
  CHECK(
      google::protobuf::TextFormat::ParseFromString(proto_text, message.get()));
  return message->SerializeAsString();
}

std::string TestProtoSetLoader::PrintToText(
    std::string_view type_name,
    const std::string& serialized_message) const {
  // Create a message of the given type, read the serialized message, and
  // print to text format.
  std::unique_ptr<google::protobuf::Message> message =
      state_->NewMessage(type_name);
  CHECK(message->ParseFromString(serialized_message));
  std::string proto_text;
  CHECK(google::protobuf::TextFormat::PrintToString(*message, &proto_text));
  return proto_text;
}

TestProtoLoader::TestProtoLoader(std::string_view descriptor_binary_proto,
                                 std::string_view type_name)
    : set_loader_(descriptor_binary_proto), type_name_(type_name) {}

TestProtoLoader::TestProtoLoader(const base::FilePath& descriptor_path,
                                 std::string_view type_name)
    : set_loader_(descriptor_path), type_name_(type_name) {}

TestProtoLoader::~TestProtoLoader() = default;

void TestProtoLoader::ParseFromText(const std::string& proto_text,
                                    std::string& serialized_message) const {
  serialized_message = set_loader_.ParseFromText(type_name_, proto_text);
}

void TestProtoLoader::PrintToText(const std::string& serialized_message,
                                  std::string& proto_text) const {
  proto_text = set_loader_.PrintToText(type_name_, serialized_message);
}

}  // namespace base
