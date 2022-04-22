// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/telemetry_extension_ui/services/system_events_service.h"

#include <memory>

#include "ash/webui/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class MockBluetoothObserver : public health::mojom::BluetoothObserver {
 public:
  MockBluetoothObserver() : receiver_{this} {}
  MockBluetoothObserver(const MockBluetoothObserver&) = delete;
  MockBluetoothObserver& operator=(const MockBluetoothObserver&) = delete;

  mojo::PendingRemote<health::mojom::BluetoothObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnAdapterAdded, (), (override));
  MOCK_METHOD(void, OnAdapterRemoved, (), (override));
  MOCK_METHOD(void, OnAdapterPropertyChanged, (), (override));
  MOCK_METHOD(void, OnDeviceAdded, (), (override));
  MOCK_METHOD(void, OnDeviceRemoved, (), (override));
  MOCK_METHOD(void, OnDevicePropertyChanged, (), (override));

 private:
  mojo::Receiver<health::mojom::BluetoothObserver> receiver_;
};

class MockLidObserver : public health::mojom::LidObserver {
 public:
  MockLidObserver() : receiver_{this} {}
  MockLidObserver(const MockLidObserver&) = delete;
  MockLidObserver& operator=(const MockLidObserver&) = delete;

  mojo::PendingRemote<health::mojom::LidObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnLidClosed, (), (override));
  MOCK_METHOD(void, OnLidOpened, (), (override));

 private:
  mojo::Receiver<health::mojom::LidObserver> receiver_;
};

class MockPowerObserver : public health::mojom::PowerObserver {
 public:
  MockPowerObserver() : receiver_{this} {}
  MockPowerObserver(const MockPowerObserver&) = delete;
  MockPowerObserver& operator=(const MockPowerObserver&) = delete;

  mojo::PendingRemote<health::mojom::PowerObserver> pending_remote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, OnAcInserted, (), (override));
  MOCK_METHOD(void, OnAcRemoved, (), (override));
  MOCK_METHOD(void, OnOsSuspend, (), (override));
  MOCK_METHOD(void, OnOsResume, (), (override));

 private:
  mojo::Receiver<health::mojom::PowerObserver> receiver_;
};

}  // namespace

class SystemEventsServiceTest : public testing::Test {
 public:
  SystemEventsServiceTest() {
    cros_healthd::FakeCrosHealthd::Initialize();
    system_events_service_ = std::make_unique<SystemEventsService>(
        remote_system_events_service_.BindNewPipeAndPassReceiver());
    mock_bluetooth_observer_ =
        std::make_unique<testing::StrictMock<MockBluetoothObserver>>();
    mock_lid_observer_ =
        std::make_unique<testing::StrictMock<MockLidObserver>>();
    mock_power_observer_ =
        std::make_unique<testing::StrictMock<MockPowerObserver>>();
  }

  ~SystemEventsServiceTest() override {
    cros_healthd::FakeCrosHealthd::Shutdown();
  }

  health::mojom::SystemEventsServiceProxy* remote_system_events_service()
      const {
    return remote_system_events_service_.get();
  }

  SystemEventsService* system_events_service() const {
    return system_events_service_.get();
  }

  mojo::PendingRemote<health::mojom::BluetoothObserver> bluetooth_observer()
      const {
    return mock_bluetooth_observer_->pending_remote();
  }

  mojo::PendingRemote<health::mojom::LidObserver> lid_observer() const {
    return mock_lid_observer_->pending_remote();
  }

  mojo::PendingRemote<health::mojom::PowerObserver> power_observer() const {
    return mock_power_observer_->pending_remote();
  }

  MockBluetoothObserver* mock_bluetooth_observer() const {
    return mock_bluetooth_observer_.get();
  }

  MockLidObserver* mock_lid_observer() const {
    return mock_lid_observer_.get();
  }

  MockPowerObserver* mock_power_observer() const {
    return mock_power_observer_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<health::mojom::SystemEventsService>
      remote_system_events_service_;
  std::unique_ptr<SystemEventsService> system_events_service_;
  std::unique_ptr<testing::StrictMock<MockBluetoothObserver>>
      mock_bluetooth_observer_;
  std::unique_ptr<testing::StrictMock<MockLidObserver>> mock_lid_observer_;
  std::unique_ptr<testing::StrictMock<MockPowerObserver>> mock_power_observer_;
};

// Tests that in case of cros_healthd crash Bluetooth Observer will reconnect.
TEST_F(SystemEventsServiceTest, BluetoothObserverReconnect) {
  remote_system_events_service()->AddBluetoothObserver(bluetooth_observer());

  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_bluetooth_observer(), OnAdapterAdded)
      .WillOnce([&run_loop1]() { run_loop1.Quit(); });
  cros_healthd::FakeCrosHealthd::Get()->EmitAdapterAddedEventForTesting();
  run_loop1.Run();

  // Shutdown cros_healthd to simulate crash.
  cros_healthd::FakeCrosHealthd::Shutdown();
  // Restart cros_healthd.
  cros_healthd::FakeCrosHealthd::Initialize();

  // Ensure disconnect handler is called for bluetooth observer from System
  // Events Service. After this call, we will have a Mojo pending connection
  // task in Mojo message queue.
  system_events_service()->FlushForTesting();

  // Ensure that Mojo pending connection task from bluetooth observer gets
  // processed and observer is bound. After this call, we are sure that
  // bluetooth observer reconnected and we can safely emit events.
  cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();

  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_bluetooth_observer(), OnAdapterAdded)
      .WillOnce([&run_loop2]() { run_loop2.Quit(); });
  cros_healthd::FakeCrosHealthd::Get()->EmitAdapterAddedEventForTesting();
  run_loop2.Run();
}

// Tests that in case of cros_healthd crash Lid Observer will reconnect.
TEST_F(SystemEventsServiceTest, LidObserverReconnect) {
  remote_system_events_service()->AddLidObserver(lid_observer());

  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_lid_observer(), OnLidClosed).WillOnce([&run_loop1]() {
    run_loop1.Quit();
  });
  cros_healthd::FakeCrosHealthd::Get()->EmitLidClosedEventForTesting();
  run_loop1.Run();

  // Shutdown cros_healthd to simulate crash.
  cros_healthd::FakeCrosHealthd::Shutdown();
  // Restart cros_healthd.
  cros_healthd::FakeCrosHealthd::Initialize();

  // Ensure disconnect handler is called for lid observer from System Event
  // Service. After this call, we will have a Mojo pending connection task in
  // Mojo message queue.
  system_events_service()->FlushForTesting();

  // Ensure that Mojo pending connection task from lid observer gets processed
  // and observer is bound. After this call, we are sure that lid observer
  // reconnected and we can safely emit events.
  cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();

  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_lid_observer(), OnLidClosed).WillOnce([&run_loop2]() {
    run_loop2.Quit();
  });

  cros_healthd::FakeCrosHealthd::Get()->EmitLidClosedEventForTesting();
  run_loop2.Run();
}

// Tests that in case of cros_healthd crash Power Observer will reconnect.
TEST_F(SystemEventsServiceTest, PowerObserverReconnect) {
  remote_system_events_service()->AddPowerObserver(power_observer());

  base::RunLoop run_loop1;
  EXPECT_CALL(*mock_power_observer(), OnAcInserted).WillOnce([&run_loop1]() {
    run_loop1.Quit();
  });
  cros_healthd::FakeCrosHealthd::Get()->EmitAcInsertedEventForTesting();
  run_loop1.Run();

  // Shutdown cros_healthd to simulate crash.
  cros_healthd::FakeCrosHealthd::Shutdown();
  // Restart cros_healthd.
  cros_healthd::FakeCrosHealthd::Initialize();

  // Ensure disconnect handler is called for power observer from System Event
  // Service. After this call, we will have a Mojo pending connection task in
  // Mojo message queue.
  system_events_service()->FlushForTesting();

  // Ensure that Mojo pending connection task from power observer gets processed
  // and observer is bound. After this call, we are sure that power observer
  // reconnected and we can safely emit events.
  cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();

  base::RunLoop run_loop2;
  EXPECT_CALL(*mock_power_observer(), OnAcInserted).WillOnce([&run_loop2]() {
    run_loop2.Quit();
  });
  cros_healthd::FakeCrosHealthd::Get()->EmitAcInsertedEventForTesting();
  run_loop2.Run();
}

}  // namespace ash
