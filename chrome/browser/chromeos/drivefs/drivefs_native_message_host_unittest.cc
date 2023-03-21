// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drivefs/drivefs_native_message_host.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace {

using base::test::RunOnceClosure;
using testing::_;
using testing::InvokeWithoutArgs;

class MockClient : public extensions::NativeMessageHost::Client {
 public:
  MockClient() = default;

  MockClient(const MockClient&) = delete;
  MockClient& operator=(const MockClient&) = delete;

  MOCK_METHOD(void,
              PostMessageFromNativeHost,
              (const std::string& message),
              (override));
  MOCK_METHOD(void,
              CloseChannel,
              (const std::string& error_message),
              (override));
};

class DriveFsNativeMessageHostTest
    : public testing::Test,
      public drivefs::mojom::NativeMessagingHost {
 public:
  DriveFsNativeMessageHostTest() = default;

  DriveFsNativeMessageHostTest(const DriveFsNativeMessageHostTest&) = delete;
  DriveFsNativeMessageHostTest& operator=(const DriveFsNativeMessageHostTest&) =
      delete;

  void CreateNativeHostSession(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost> session,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> port) {
    params_ = std::move(params);
    extension_port_.Bind(std::move(port));
    receiver_.Bind(std::move(session));
  }

  MOCK_METHOD(void,
              HandleMessageFromExtension,
              (const std::string& message),
              (override));

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  drivefs::mojom::ExtensionConnectionParamsPtr params_;
  mojo::Receiver<drivefs::mojom::NativeMessagingHost> receiver_{this};
  mojo::Remote<drivefs::mojom::NativeMessagingPort> extension_port_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DriveFsNativeMessageHostTest, DriveFsInitiatedMessaging) {
  base::RunLoop run_loop;

  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsInitiatedNativeMessageHostInternal(
          &profile_, extension_port_.BindNewPipeAndPassReceiver(),
          receiver_.BindNewPipeAndPassRemote());
  MockClient client;
  EXPECT_CALL(client, PostMessageFromNativeHost("foo"))
      .WillOnce(InvokeWithoutArgs([&host]() { host->OnMessage("bar"); }));
  EXPECT_CALL(*this, HandleMessageFromExtension("bar"))
      .WillOnce(InvokeWithoutArgs(
          [this]() { extension_port_->PostMessageToExtension("baz"); }));
  EXPECT_CALL(client, PostMessageFromNativeHost("baz"))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));

  host->Start(&client);
  extension_port_->PostMessageToExtension("foo");
  run_loop.Run();
}

TEST_F(DriveFsNativeMessageHostTest, ExtensionInitiatedMessaging) {
  base::RunLoop run_loop;

  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsNativeMessageHost(
          base::BindOnce(&DriveFsNativeMessageHostTest::CreateNativeHostSession,
                         base::Unretained(this)));
  MockClient client;
  host->Start(&client);
  EXPECT_CALL(*this, HandleMessageFromExtension("foo"))
      .WillOnce(InvokeWithoutArgs(
          [this]() { extension_port_->PostMessageToExtension("bar"); }));
  EXPECT_CALL(client, PostMessageFromNativeHost("bar"))
      .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
  host->OnMessage("foo");
  run_loop.Run();
}

TEST_F(DriveFsNativeMessageHostTest, NativeHostSendsMessageBeforeStart) {
  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsInitiatedNativeMessageHostInternal(
          &profile_, extension_port_.BindNewPipeAndPassReceiver(),
          receiver_.BindNewPipeAndPassRemote());
  MockClient client;

  EXPECT_CALL(client, PostMessageFromNativeHost(_)).Times(0);
  extension_port_->PostMessageToExtension("foo");
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, PostMessageFromNativeHost("foo"))
        .WillOnce(InvokeWithoutArgs([&host]() { host->OnMessage("bar"); }));
    EXPECT_CALL(*this, HandleMessageFromExtension("bar"))
        .WillOnce(RunOnceClosure(run_loop.QuitClosure()));
    host->Start(&client);
    run_loop.Run();
  }
}

TEST_F(DriveFsNativeMessageHostTest, Error) {
  base::RunLoop run_loop;

  std::unique_ptr<extensions::NativeMessageHost> host =
      CreateDriveFsInitiatedNativeMessageHostInternal(
          &profile_, extension_port_.BindNewPipeAndPassReceiver(),
          receiver_.BindNewPipeAndPassRemote());
  MockClient client;
  EXPECT_CALL(*this, HandleMessageFromExtension).Times(0);
  EXPECT_CALL(client, PostMessageFromNativeHost).Times(0);
  EXPECT_CALL(client, CloseChannel("FILE_ERROR_FAILED: foo"));
  receiver_.set_disconnect_handler(run_loop.QuitClosure());

  host->Start(&client);
  extension_port_.ResetWithReason(1u, "foo");

  run_loop.Run();

  host->OnMessage("bar");
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace drive
