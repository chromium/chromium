// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_proto_loader.h"

#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/protobuf/src/google/protobuf/message.h"
#include "third_party/protobuf/src/google/protobuf/text_format.h"

namespace base {

TestProtoLoader::TestProtoLoader(const base::FilePath& descriptor_path,
                                 base::StringPiece type_name) {
  // Load the descriptors and find the one for |type_name|.
  std::string package, name;
  std::vector<std::string> type_name_parts = base::SplitString(
      type_name, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  DCHECK_GE(type_name_parts.size(), 2U) << "|type_name| should include package";

  prototype_ = GetPrototype(
      descriptor_path, /*package =*/
      base::JoinString(
          base::make_span(type_name_parts.begin(), type_name_parts.size() - 1),
          "."),
      /* name = */ type_name_parts.back());
  DCHECK_NE(nullptr, prototype_);
}

TestProtoLoader::~TestProtoLoader() = default;

const google::protobuf::Message* TestProtoLoader::GetPrototype(
    base::FilePath descriptor_path,
    base::StringPiece package,
    base::StringPiece name) {
  std::string file_contents;

  if (!base::ReadFileToString(descriptor_path, &file_contents)) {
    NOTREACHED() << "Couldn't load contents of " << descriptor_path;
    return nullptr;
  }

  if (!descriptor_set_.ParseFromString(file_contents)) {
    NOTREACHED() << "Couldn't parse descriptor from " << descriptor_path;
    return nullptr;
  }

  for (int file_i = 0; file_i < descriptor_set_.file_size(); ++file_i) {
    const google::protobuf::FileDescriptorProto& file =
        descriptor_set_.file(file_i);
    if (file.package() != package) {
      continue;
    }
    const google::protobuf::FileDescriptor* descriptor =
        descriptor_pool_.BuildFile(file);
    for (int message_type_i = 0;
         message_type_i < descriptor->message_type_count(); ++message_type_i) {
      const google::protobuf::Descriptor* message_type =
          descriptor->message_type(message_type_i);
      if (message_type->name() != name) {
        continue;
      }
      return dynamic_message_factory_.GetPrototype(message_type);
    }
  }
  NOTREACHED() << "Couldn't find " << package << "." << name << "in "
               << descriptor_path;
  return nullptr;
}

void TestProtoLoader::ParseFromText(const std::string& proto_text,
                                    std::string& serialized_message) {
  // Parse the text using the descriptor-generated message and send it to
  // |destination|.
  std::unique_ptr<google::protobuf::Message> message(prototype_->New());
  bool success =
      google::protobuf::TextFormat::ParseFromString(proto_text, message.get());
  success |= message->SerializeToString(&serialized_message);
  DCHECK(success);
}

void TestProtoLoader::PrintToText(const std::string& serialized_message,
                                  std::string& proto_text) {
  // Parse the text using the descriptor-generated message and send it to
  // |destination|.
  std::unique_ptr<google::protobuf::Message> message(prototype_->New());
  bool success = message->ParseFromString(serialized_message);
  success |= google::protobuf::TextFormat::PrintToString(*message, &proto_text);
  DCHECK(success);
}

}  // namespace base
