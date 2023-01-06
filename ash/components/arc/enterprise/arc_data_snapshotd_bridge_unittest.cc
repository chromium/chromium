// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/enterprise/arc_data_snapshotd_bridge.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/arc/arc_data_snapshotd_client.h"
#include "chromeos/ash/components/dbus/arc/fake_arc_data_snapshotd_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace data_snapshotd {

namespace {

constexpr char kFakeAccountId[] = "fake_account_id@localhost";

void RunGenerateKeyPair(ArcDataSnapshotdBridge* bridge, bool expected_result) {
  base::RunLoop run_loop;
  bridge->GenerateKeyPair(
      base::BindLambdaForTesting([expected_result, &run_loop](bool success) {
        EXPECT_EQ(expected_result, success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

void RunClearSnapshot(ArcDataSnapshotdBridge* bridge, bool expected_result) {
  base::RunLoop run_loop;
  bridge->ClearSnapshot(
      false /* last */,
      base::BindLambdaForTesting([expected_result, &run_loop](bool success) {
        EXPECT_EQ(expected_result, success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

void RunTakeSnapshot(ArcDataSnapshotdBridge* bridge, bool expected_result) {
  base::RunLoop run_loop;
  bridge->TakeSnapshot(
      kFakeAccountId,
      base::BindLambdaForTesting([expected_result, &run_loop](bool success) {
        EXPECT_EQ(expected_result, success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

void RunLoadSnapshot(ArcDataSnapshotdBridge* bridge, bool expected_result) {
  base::RunLoop run_loop;
  bridge->LoadSnapshot(kFakeAccountId,
                       base::BindLambdaForTesting([expected_result, &run_loop](
                                                      bool success, bool last) {
                         EXPECT_EQ(expected_result, success);
                         // If LoadSnapshot fails, last = false.
                         // If succeeds, last = true for tests.
                         EXPECT_EQ(expected_result, last);
                         run_loop.Quit();
                       }));
  run_loop.Run();
}

void RunUpdate(ArcDataSnapshotdBridge* bridge, bool expected_result) {
  base::RunLoop run_loop;
  bridge->Update(
      50 /* percent */,
      base::BindLambdaForTesting([expected_result, &run_loop](bool success) {
        EXPECT_EQ(expected_result, success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

void RunAll(ArcDataSnapshotdBridge* bridge, bool expected_result) {
  RunGenerateKeyPair(bridge, expected_result);
  RunClearSnapshot(bridge, expected_result);
  RunTakeSnapshot(bridge, expected_result);
  RunLoadSnapshot(bridge, expected_result);
  RunUpdate(bridge, expected_result);
}

// Tests ArcDataSnapshotdBridge class instance.
class ArcDataSnapshotdBridgeTest : public testing::Test {
 protected:
  ArcDataSnapshotdBridgeTest() {
    ash::ArcDataSnapshotdClient::InitializeFake();
  }

  ~ArcDataSnapshotdBridgeTest() override {
    ash::ArcDataSnapshotdClient::Shutdown();
  }

  ash::FakeArcDataSnapshotdClient* dbus_client() {
    auto* client = ash::ArcDataSnapshotdClient::Get();
    DCHECK(client);
    return static_cast<ash::FakeArcDataSnapshotdClient*>(client);
  }

  void FastForwardAttempt() {
    task_environment_.FastForwardBy(
        ArcDataSnapshotdBridge::connection_attempt_interval_for_testing());
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test basic scenario: D-Bus service is available immediately.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceAvailable) {
  dbus_client()->set_available(true /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};
  EXPECT_FALSE(bridge.is_available_for_testing());
  RunAll(&bridge, false /* expected_result */);

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(bridge.is_available_for_testing());
  RunAll(&bridge, true /* expected_result */);
}

// Test basic scenario: D-Bus service is not available.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceUnavailable) {
  dbus_client()->set_available(false /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};

  task_environment_.RunUntilIdle();

  EXPECT_FALSE(bridge.is_available_for_testing());
  RunAll(&bridge, false /* expected_result */);
}

// Test that service is available from the max attempt.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceAvailableMaxAttempt) {
  dbus_client()->set_available(false /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};

  // Not available from the first attempt.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bridge.is_available_for_testing());

  size_t attempts_number =
      ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing() - 1;
  for (size_t i = 1; i < attempts_number; i++) {
    // Not available from the next max - 2 attempts.
    FastForwardAttempt();
    EXPECT_FALSE(bridge.is_available_for_testing());
  }
  // Available from the max attempt.
  dbus_client()->set_available(true /* is_available */);
  FastForwardAttempt();
  EXPECT_TRUE(bridge.is_available_for_testing());
  RunAll(&bridge, true /* expected_result */);
}

// Test that service is available from the max + 1 attempt and is not picked up.
TEST_F(ArcDataSnapshotdBridgeTest, ServiceUnavailableMaxAttempts) {
  dbus_client()->set_available(false /* is_available */);
  ArcDataSnapshotdBridge bridge{base::DoNothing()};

  // Not available from the first attempt.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(bridge.is_available_for_testing());

  size_t attempts_number =
      ArcDataSnapshotdBridge::max_connection_attempt_count_for_testing();
  for (size_t i = 1; i < attempts_number; i++) {
    // Not available from the next max - 1 attempts.
    FastForwardAttempt();
    EXPECT_FALSE(bridge.is_available_for_testing());
  }
  // Available from the max + 1 attempt, but bridge is not listening.
  dbus_client()->set_available(true /* is_available */);
  FastForwardAttempt();
  EXPECT_FALSE(bridge.is_available_for_testing());
  RunAll(&bridge, false /* expected_result */);
}

}  // namespace

}  // namespace data_snapshotd
}  // namespace arc
