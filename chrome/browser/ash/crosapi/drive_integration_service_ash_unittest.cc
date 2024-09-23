// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/drive_integration_service_ash.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class DriveIntegrationServiceAshTest : public testing::Test {
 public:
  DriveIntegrationServiceAshTest() = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    testing_profile_manager_.CreateTestingProfile("profile",
                                                  /*is_main_profile=*/true);
  }

  DriveIntegrationServiceAsh* drive_integration_service_ash() {
    return &drive_integration_service_ash_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};

  DriveIntegrationServiceAsh drive_integration_service_ash_;
};

class MockNativeMessagingPort : drivefs::mojom::NativeMessagingPort {
 public:
  MockNativeMessagingPort() : receiver_(this) {}

  mojo::Receiver<drivefs::mojom::NativeMessagingPort>* receiver() {
    return &receiver_;
  }

  MOCK_METHOD(void, PostMessageToExtension, (const std::string&), (override));

 private:
  mojo::Receiver<drivefs::mojom::NativeMessagingPort> receiver_;
};

TEST_F(DriveIntegrationServiceAshTest,
       CreateNativeHostSessionWithoutDriveService) {
  MockNativeMessagingPort mock_port;
  mojo::Remote<drivefs::mojom::NativeMessagingHost> drivefs_remote;
  auto extension_remote = mock_port.receiver()->BindNewPipeAndPassRemote();

  base::MockCallback<mojo::ConnectionErrorWithReasonCallback>
      disconnect_callback;
  base::test::TestFuture<void> waiter;
  EXPECT_CALL(disconnect_callback, Run(-drive::FILE_ERROR_SERVICE_UNAVAILABLE,
                                       "DriveFS is unavailable."))
      .WillOnce([&] { waiter.SetValue(); });
  mock_port.receiver()->set_disconnect_with_reason_handler(
      disconnect_callback.Get());

  drive_integration_service_ash()->CreateNativeHostSession(
      drivefs::mojom::ExtensionConnectionParams::New(),
      drivefs_remote.BindNewPipeAndPassReceiver(), std::move(extension_remote));

  EXPECT_TRUE(waiter.Wait());
}

}  // namespace crosapi
