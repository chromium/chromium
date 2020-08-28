// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/instantmessaging/stream_parser.h"

#include <string>
#include <vector>

#include "base/bind.h"
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

chrome_browser_nearby_sharing_instantmessaging::StreamBody BuildProto(
    const std::vector<std::string>& messages) {
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body;
  for (const auto& msg : messages) {
    stream_body.add_messages(
        CreateReceiveMessagesResponse(msg).SerializeAsString());
  }
  return stream_body;
}

}  // namespace

class StreamParserTest : public testing::Test {
 public:
  StreamParserTest()
      : stream_parser_(base::BindRepeating(&StreamParserTest::OnMessageReceived,
                                           base::Unretained(this))) {}
  ~StreamParserTest() override = default;

  StreamParser& GetStreamParser() { return stream_parser_; }

  int MessagesReceived() { return messages_received_.size(); }

  const std::vector<std::string> GetMessages() { return messages_received_; }

 private:
  void OnMessageReceived(const std::string& message) {
    messages_received_.push_back(message);
  }

  StreamParser stream_parser_;
  std::vector<std::string> messages_received_;
};

// The entire message is sent in one response body.
TEST_F(StreamParserTest, SingleEntireMessageAtOnce) {
  std::vector<std::string> messages = {"random 42"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  GetStreamParser().Append(stream_body.SerializeAsString());

  EXPECT_EQ(1, MessagesReceived());
  EXPECT_EQ(messages, GetMessages());
}

// More than one message is sent in one response body.
TEST_F(StreamParserTest, MultipleEntireMessagesAtOnce) {
  std::vector<std::string> messages = {"random 42", "more random 98",
                                       "helloworld 25"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  GetStreamParser().Append(stream_body.SerializeAsString());

  EXPECT_EQ(3, MessagesReceived());
  EXPECT_EQ(messages, GetMessages());
}

// A single message is sent over multiple response bodies.
TEST_F(StreamParserTest, SingleMessageSplit) {
  std::vector<std::string> messages = {"random 42 and random 92"};
  chrome_browser_nearby_sharing_instantmessaging::StreamBody stream_body =
      BuildProto(messages);
  std::string serialized_msg = stream_body.SerializeAsString();

  // Randomly chosen.
  int pos = 13;

  GetStreamParser().Append(serialized_msg.substr(0, pos));
  EXPECT_EQ(0, MessagesReceived());
  EXPECT_EQ(std::vector<std::string>(), GetMessages());

  GetStreamParser().Append(serialized_msg.substr(pos));
  EXPECT_EQ(1, MessagesReceived());
  EXPECT_EQ(messages, GetMessages());
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

  GetStreamParser().Append(first_message);
  EXPECT_EQ(3, MessagesReceived());
  EXPECT_EQ(messages_1, GetMessages());

  messages_1.insert(messages_1.end(), messages_2.begin(), messages_2.end());
  GetStreamParser().Append(second_message);
  EXPECT_EQ(5, MessagesReceived());
  EXPECT_EQ(messages_1, GetMessages());
}
