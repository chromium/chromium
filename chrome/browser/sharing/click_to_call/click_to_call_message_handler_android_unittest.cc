// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_message_handler_android.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "components/sharing_message/proto/click_to_call_message.pb.h"
#include "components/sharing_message/proto/sharing_message.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test implementation of ClickToCallMessageHandler that does not call out to
// Java via JNI but records the last phone number it would have sent over.
class TestClickToCallMessageHandler : public ClickToCallMessageHandler {
 public:
  TestClickToCallMessageHandler() = default;
  ~TestClickToCallMessageHandler() override = default;

  std::optional<std::string> last_phone_number() { return last_phone_number_; }

 protected:
  void HandlePhoneNumber(const std::string& phone_number) override {
    last_phone_number_ = phone_number;
  }

 private:
  std::optional<std::string> last_phone_number_;
};

}  // namespace

TEST(ClickToCallMessageHandlerTest, HandlesValidPhoneNumber) {
  TestClickToCallMessageHandler handler;
  components_sharing_message::SharingMessage message;

  message.mutable_click_to_call_message()->set_phone_number("12345678");
  handler.OnMessage(std::move(message), base::DoNothing());
  EXPECT_EQ("12345678", handler.last_phone_number());
}

TEST(ClickToCallMessageHandlerTest, IgnoresInvalidPhoneNumbers) {
  TestClickToCallMessageHandler handler;
  components_sharing_message::SharingMessage message;

  message.mutable_click_to_call_message()->set_phone_number("*#06#");
  handler.OnMessage(std::move(message), base::DoNothing());
  EXPECT_FALSE(handler.last_phone_number().has_value());

  message.mutable_click_to_call_message()->set_phone_number("%2A%2306%23");
  handler.OnMessage(std::move(message), base::DoNothing());
  EXPECT_FALSE(handler.last_phone_number().has_value());

  message.mutable_click_to_call_message()->set_phone_number(
      "%252A%252306%2523");
  handler.OnMessage(std::move(message), base::DoNothing());
  EXPECT_FALSE(handler.last_phone_number().has_value());
}
