// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

namespace {

const char kFormat[] = "[\"%s\"]";
const char kTestMessage[] = "test message";
const char kTestMessage2[] = "test message 2";
const char kFailureMessage[] = "failure";

}  // namespace

class ExtensionTestMessageListenerUnittest : public ExtensionApiUnittest {};

TEST_F(ExtensionTestMessageListenerUnittest, BasicTestExtensionMessageTest) {
  // A basic test of sending a message and ensuring the listener is satisfied.
  {
    ExtensionTestMessageListener listener(kTestMessage);
    EXPECT_FALSE(listener.was_satisfied());
    RunFunction(new TestSendMessageFunction,
                base::StringPrintf(kFormat, kTestMessage));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_EQ(kTestMessage, listener.message());
  }

  // Test that we can receive an arbitrary message.
  {
    ExtensionTestMessageListener listener;  // won't reply
    EXPECT_FALSE(listener.was_satisfied());
    RunFunction(new TestSendMessageFunction,
                base::StringPrintf(kFormat, kTestMessage2));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_EQ(kTestMessage2, listener.message());
  }

  // Test that we can set the listener to be reused, and send/receive multiple
  // messages.
  {
    ExtensionTestMessageListener listener;  // won't reply
    EXPECT_FALSE(listener.was_satisfied());
    RunFunction(new TestSendMessageFunction,
                base::StringPrintf(kFormat, kTestMessage));
    EXPECT_EQ(kTestMessage, listener.message());
    EXPECT_TRUE(listener.was_satisfied());
    listener.Reset();
    EXPECT_FALSE(listener.was_satisfied());
    EXPECT_TRUE(listener.message().empty());
    RunFunction(new TestSendMessageFunction,
                base::StringPrintf(kFormat, kTestMessage2));
    EXPECT_TRUE(listener.was_satisfied());
    EXPECT_EQ(kTestMessage2, listener.message());
  }

  // Test that we can listen for two explicit messages: a success, and a
  // failure.
  {
    ExtensionTestMessageListener listener(kTestMessage);
    listener.set_failure_message(kFailureMessage);
    RunFunction(new TestSendMessageFunction,
                base::StringPrintf(kFormat, kTestMessage));
    EXPECT_TRUE(listener.WaitUntilSatisfied());  // succeeds
    EXPECT_EQ(kTestMessage, listener.message());
    listener.Reset();
    RunFunction(new TestSendMessageFunction,
                base::StringPrintf(kFormat, kFailureMessage));
    EXPECT_FALSE(listener.WaitUntilSatisfied());  // fails
    EXPECT_EQ(kFailureMessage, listener.message());
  }
}

}  // namespace extensions
