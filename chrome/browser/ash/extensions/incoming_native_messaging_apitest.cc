// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "chrome/browser/extensions/api/messaging/native_message_port.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/messaging/channel_endpoint.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/test/result_catcher.h"
#include "ipc/ipc_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::SaveArg;
using testing::StrictMock;

namespace {

const char kFakeNativeAppName[] = "com.google.chrome.test.initiator";

class MockNativeMessageHost : public extensions::NativeMessageHost {
 public:
  MOCK_METHOD1(OnMessage, void(const std::string& message));
  MOCK_METHOD1(Start, void(Client* client));

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::SingleThreadTaskRunner::GetCurrentDefault();
};

// Test fixture for testing native messaging API when the communication is
// initiated by the native application.
class ExtensionIncomingNativeMessagingTest
    : public extensions::ExtensionApiTest {
 public:
  ExtensionIncomingNativeMessagingTest(
      const ExtensionIncomingNativeMessagingTest&) = delete;
  ExtensionIncomingNativeMessagingTest& operator=(
      const ExtensionIncomingNativeMessagingTest&) = delete;

 protected:
  ExtensionIncomingNativeMessagingTest() = default;
  ~ExtensionIncomingNativeMessagingTest() override = default;

  bool LoadTestExtension() {
    extension_ =
        LoadExtension(test_data_dir_.AppendASCII("incoming_native_messaging"));
    return extension_ != nullptr;
  }

  void OpenMessageChannelToExtension(
      std::unique_ptr<extensions::NativeMessageHost> native_message_host) {
    auto* const message_service = extensions::MessageService::Get(profile());
    const extensions::PortId port_id(
        base::UnguessableToken::Create(), 1 /* port_number */,
        true /* is_opener */, extensions::mojom::SerializationFormat::kJson);
    auto native_message_port = std::make_unique<extensions::NativeMessagePort>(
        message_service->GetChannelDelegate(), port_id,
        std::move(native_message_host));
    message_service->OpenChannelToExtension(
        extensions::ChannelEndpoint(profile()), port_id,
        extensions::MessagingEndpoint::ForNativeApp(kFakeNativeAppName),
        std::move(native_message_port), extension_->id(), GURL(),
        extensions::mojom::ChannelType::kNative,
        std::string() /* channel_name */);
  }

 private:
  raw_ptr<const extensions::Extension, DanglingUntriaged> extension_ = nullptr;
};

// Tests that the extension receives the onConnectNative event when the native
// application opens a message channel to it, and that each of them can
// successfully send a message.
IN_PROC_BROWSER_TEST_F(ExtensionIncomingNativeMessagingTest,
                       SingleRequestResponse) {
  extensions::ResultCatcher catcher;

  ASSERT_TRUE(LoadTestExtension());

  auto owned_native_message_host =
      std::make_unique<StrictMock<MockNativeMessageHost>>();
  auto* const native_message_host = owned_native_message_host.get();

  extensions::NativeMessageHost::Client* native_message_host_client = nullptr;
  EXPECT_CALL(*native_message_host, Start(_))
      .WillOnce(SaveArg<0>(&native_message_host_client));

  const char kExpectedMessageFromExtension[] = R"({"request":"foo"})";
  base::RunLoop message_awaiting_run_loop;
  EXPECT_CALL(*native_message_host, OnMessage(kExpectedMessageFromExtension))
      .WillOnce(
          InvokeWithoutArgs(&message_awaiting_run_loop, &base::RunLoop::Quit));

  OpenMessageChannelToExtension(std::move(owned_native_message_host));

  // Wait until the extension sends a message to the native application.
  message_awaiting_run_loop.Run();
  Mock::VerifyAndClearExpectations(native_message_host);
  ASSERT_TRUE(native_message_host_client);

  // Post a reply to the extension.
  const char kResponseMessageToSend[] = R"({"response":"bar"})";
  native_message_host_client->PostMessageFromNativeHost(kResponseMessageToSend);

  // Wait till the extension receives and validates the reply.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace
