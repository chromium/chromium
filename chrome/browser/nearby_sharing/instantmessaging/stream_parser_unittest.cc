// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/nearby_sharing/instantmessaging/proto/instantmessaging.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
CreateReceiveMessagesResponse(const std::string& msg) {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
      response;
  response.mutable_inbox_message()->set_message(msg);
  return response;
}

chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
CreateFastPathReadyResponse() {
  chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse
      response;
  response.mutable_fast_path_ready();
  return response;
}

chrome_browser_nearby_sharing_instantmessaging::StreamBody BuildProto(
    const std::vector<std::string>& messages,
    bool include_fast_path = false) {
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body;
  if (include_fast_path) {
    stream_body.add_messages(CreateFastPathReadyResponse().SerializeAsString());
  }
  for (const auto& msg : messages) {
    stream_body.add_messages(
        CreateReceiveMessagesResponse(msg).SerializeAsString());
  }
  return stream_body;
}

}  // namespace

class StreamParserTest : public testing::Test {
 public:
  StreamParserTest() = default;
  ~StreamParserTest() override = default;

  StreamParser& GetStreamParser() { return stream_parser_; }

 private:
  StreamParser stream_parser_;
};

// The entire message is sent in one response body.
TEST_F(StreamParserTest, SingleEntireMessageAtOnce) {
  std::vector<std::string> messages = {"random 42"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      responses = GetStreamParser().Append(stream_body.SerializeAsString());
  EXPECT_EQ(responses.size(), 1u);
  EXPECT_EQ(responses[0].mutable_inbox_message()->message(), messages[0]);
}

// More than one message is sent in one response body.
TEST_F(StreamParserTest, MultipleEntireMessagesAtOnce) {
  std::vector<std::string> messages = {"random 42", "more random 98",
                                       "helloworld 25"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      responses = GetStreamParser().Append(stream_body.SerializeAsString());

  EXPECT_EQ(responses.size(), 3u);
  EXPECT_EQ(responses[0].mutable_inbox_message()->message(), messages[0]);
  EXPECT_EQ(responses[1].mutable_inbox_message()->message(), messages[1]);
  EXPECT_EQ(responses[2].mutable_inbox_message()->message(), messages[2]);
}

// A single message is sent over multiple response bodies.
TEST_F(StreamParserTest, SingleMessageSplit) {
  std::vector<std::string> messages = {"random 42 and random 92"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  std::string serialized_msg = stream_body.SerializeAsString();

  // Randomly chosen.
  int pos = 13;

  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      responses = GetStreamParser().Append(serialized_msg.substr(0, pos));
  EXPECT_EQ(responses.size(), 0u);

  responses = GetStreamParser().Append(serialized_msg.substr(pos));
  EXPECT_EQ(responses.size(), 1u);
  EXPECT_EQ(responses[0].mutable_inbox_message()->message(), messages[0]);
}

// Multiple messages are sent over multiple response bodies.
TEST_F(StreamParserTest, MultipleMessagesSplit) {
  std::vector<std::string> messages_1 = {"The quick", "brown fox", "jumps"};
  std::vector<std::string> messages_2 = {"over the lazy", "dog."};

  std::string serialized_msg_1 = BuildProto(messages_1).SerializeAsString();
  std::string serialized_msg_2 = BuildProto(messages_2).SerializeAsString();

  // Randomly chosen.
  int pos = 7;

  std::string first_message =
      serialized_msg_1 + serialized_msg_2.substr(0, pos);
  std::string second_message = serialized_msg_2.substr(pos);

  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      responses = GetStreamParser().Append(first_message);
  EXPECT_EQ(3u, responses.size());
  EXPECT_EQ(messages_1[0], responses[0].mutable_inbox_message()->message());
  EXPECT_EQ(messages_1[1], responses[1].mutable_inbox_message()->message());
  EXPECT_EQ(messages_1[2], responses[2].mutable_inbox_message()->message());

  responses = GetStreamParser().Append(second_message);
  EXPECT_EQ(2u, responses.size());
  EXPECT_EQ(messages_2[0], responses[0].mutable_inbox_message()->message());
  EXPECT_EQ(messages_2[1], responses[1].mutable_inbox_message()->message());
}

// Check that the buffer resizes properly when a long message is sent at once.
TEST_F(StreamParserTest, LongMessageAtOnce) {
  std::vector<std::string> messages = {
      "This is a long test message to see if the buffer breaks if we send a "
      "big message: "
      "111111111111111111111111111111111111111111111111111111111111111111111111"
      "111111111111111111111111111111111111111111111111111111111111111111111111"
      "111111111111111111111111111111111111111111111111111111111111111111111111"
      "111111111111111111111111111111111111111111111111111111111111111111111111"
      "111111111111111111111111111111111111111111111111111111111111111111111111"
      "111111111111111111111111111111111111111111111111111111111111111111111111"
      "111111"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      responses = GetStreamParser().Append(stream_body.SerializeAsString());
  EXPECT_EQ(1u, responses.size());
  EXPECT_EQ(messages[0], responses[0].mutable_inbox_message()->message());
}

// Check that when we have a tag failure, no message is received.
TEST_F(StreamParserTest, TagFailure) {
  std::string message = "";
  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      responses = GetStreamParser().Append(message);
  EXPECT_EQ(0u, responses.size());

  char bytes[2] = {0x0f, 0x00};
  auto bytes_string = std::string_view(bytes);
  EXPECT_EQ(1u, bytes_string.length());
  responses = GetStreamParser().Append(bytes);
  EXPECT_EQ(0u, responses.size());
}

// Check that when we have a ReadBytes failure, no message is received.
TEST_F(StreamParserTest, ReadBytesFailure) {
  std::vector<std::string> messages = {"random 42 and random 92"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  std::string serialized_msg = stream_body.SerializeAsString();

  // Randomly chosen.
  int pos = 13;

  std::vector<
      chrome_browser_nearby_sharing_instantmessaging::ReceiveMessagesResponse>
      responses = GetStreamParser().Append(serialized_msg.substr(0, pos));
  EXPECT_EQ(responses.size(), 0u);
}
