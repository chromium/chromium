// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/fullscreen_controller_ash.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/fullscreen_controller.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

using mojom::FullscreenControllerClient;

using ShouldExitFullscreenBeforeLockCallback =
    mojom::FullscreenControllerClient::ShouldExitFullscreenBeforeLockCallback;

class FullscreenControllerAshBaseTest : public testing::Test {
 public:
  void InvokeShouldExitFullscreenBeforeLock(bool expected_result) {
    base::test::TestFuture<bool> future;
    fullscreen_controller_ash()->ShouldExitFullscreenBeforeLock(
        future.GetCallback());

    EXPECT_EQ(future.Get(), expected_result);
  }

  FullscreenControllerAsh* fullscreen_controller_ash() {
    return &fullscreen_controller_ash_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  FullscreenControllerAsh fullscreen_controller_ash_;
};

// Test that the default (True) is returned if
// `ShouldExitFullscreenBeforeLock()` is invoked with no client bound.
TEST_F(FullscreenControllerAshBaseTest,
       ShouldExitFullscreenBeforeLockWithoutBoundClient) {
  InvokeShouldExitFullscreenBeforeLock(/*expected_result=*/true);
}

class FullscreenControllerAshTest : public FullscreenControllerAshBaseTest,
                                    public testing::WithParamInterface<bool> {
 public:
  class MockFullscreenControllerClient : public FullscreenControllerClient {
   public:
    MOCK_METHOD(void,
                ShouldExitFullscreenBeforeLock,
                (ShouldExitFullscreenBeforeLockCallback callback),
                (override));
  };

 protected:
  testing::StrictMock<MockFullscreenControllerClient> client_;
};

// Test that the correct response is returned if
// `ShouldExitFullscreenBeforeLock()` is invoked with a client bound.
TEST_P(FullscreenControllerAshTest,
       ShouldExitFullscreenBeforeLockWithBoundClient) {
  bool expected_result = GetParam();

  // Bind `client_`.
  mojo::Receiver<FullscreenControllerClient> client_receiver{&client_};
  fullscreen_controller_ash()->AddClient(
      client_receiver.BindNewPipeAndPassRemoteWithVersion());

  // Mock `client_` response for `ShouldExitFullscreenBeforeLock()`.
  EXPECT_CALL(client_, ShouldExitFullscreenBeforeLock)
      .WillOnce(testing::Invoke(
          [expected_result](ShouldExitFullscreenBeforeLockCallback callback) {
            std::move(callback).Run(expected_result);
          }));

  InvokeShouldExitFullscreenBeforeLock(expected_result);
}

INSTANTIATE_TEST_SUITE_P(All, FullscreenControllerAshTest, testing::Bool());

}  // namespace crosapi
