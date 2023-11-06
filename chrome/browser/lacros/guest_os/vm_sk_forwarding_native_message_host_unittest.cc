// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is copied from
// //chrome/browser/ash/guest_os/vm_sk_forwarding_native_message_host_unittest.cc

#include "chrome/browser/lacros/guest_os/vm_sk_forwarding_native_message_host.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::WithArg;

namespace guest_os {

namespace {

const char kMessageFromExtension[] =
    "{\"text\":\"test message from extension\"}";
const char kMessageFromDBus[] = "{\"text\":\"test message from DBus\"}";

class MockNativeMessageHostClient
    : public extensions::NativeMessageHost::Client {
 public:
  MOCK_METHOD1(PostMessageFromNativeHost, void(const std::string& message));
  MOCK_METHOD1(CloseChannel, void(const std::string& error_message));
};

}  // namespace

// Test NM host returns "Not Supported" error for communication channel
// initiated by an extension.
TEST(VmSKForwardingNativeMessageHostTests, ChannelCreatedByExtension) {
  // Create the message host.
  std::unique_ptr<extensions::NativeMessageHost> message_host =
      VmSKForwardingNativeMessageHost::CreateFromExtension(nullptr);

  StrictMock<MockNativeMessageHostClient> message_host_client;
  EXPECT_CALL(
      message_host_client,
      PostMessageFromNativeHost(VmSKForwardingNativeMessageHost ::
                                    kHostCreatedByExtensionNotSupportedError));
  message_host->Start(&message_host_client);
  EXPECT_CALL(message_host_client, CloseChannel(""));
  message_host->OnMessage(kMessageFromExtension);
}

// Test the basic successful scenario when NM host is created with message and
// callback (resembles situation when channel is initiated from DBus
// service provider). Message is delivered to extension once channel is created
// and response from extension is forwarded to callback method.
TEST(VmSKForwardingNativeMessageHostTests, SimpleRequestResponse) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::string captured_response;
  VmSKForwardingNativeMessageHost::ResponseCallback callback =
      base::BindLambdaForTesting(
          [&](const std::string& response) { captured_response = response; });

  base::RunLoop run_loop;

  // Create the message host.
  std::unique_ptr<extensions::NativeMessageHost> message_host =
      VmSKForwardingNativeMessageHost::CreateFromDBus(kMessageFromDBus,
                                                      std::move(callback));

  StrictMock<MockNativeMessageHostClient> message_host_client;
  EXPECT_CALL(message_host_client, PostMessageFromNativeHost(kMessageFromDBus))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  message_host->Start(&message_host_client);
  EXPECT_CALL(message_host_client, CloseChannel(""));

  message_host->OnMessage(kMessageFromExtension);
  run_loop.Run();
  EXPECT_EQ(captured_response, kMessageFromExtension);
}

// Test scenario when extension sends multiple messages to the channel.
// On consecuitive messages NM's OnMessage should be no-op.
TEST(VmSKForwardingNativeMessageHostTests, MultipleMessagesFromExtension) {
  base::test::SingleThreadTaskEnvironment task_environment;

  VmSKForwardingNativeMessageHost::ResponseCallback callback =
      base::DoNothing();

  base::RunLoop run_loop;

  // Create the message host.
  std::unique_ptr<extensions::NativeMessageHost> message_host =
      VmSKForwardingNativeMessageHost::CreateFromDBus(kMessageFromDBus,
                                                      std::move(callback));

  StrictMock<MockNativeMessageHostClient> message_host_client;
  EXPECT_CALL(message_host_client, PostMessageFromNativeHost(kMessageFromDBus))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  message_host->Start(&message_host_client);
  EXPECT_CALL(message_host_client, CloseChannel(""));

  message_host->OnMessage(kMessageFromExtension);
  run_loop.Run();
  message_host->OnMessage(kMessageFromExtension);
}

}  // namespace guest_os
