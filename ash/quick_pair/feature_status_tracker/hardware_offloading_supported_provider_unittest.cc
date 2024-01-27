// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/feature_status_tracker/hardware_offloading_supported_provider.h"

#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {
namespace quick_pair {

class HardwareOffloadingSupportedProviderTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
  }

  // IMPORTANT: must be called for `hardware_offloading_supported_provider_` to
  // not be null. Configures adapter instance that the provider's ctor
  // retrieves and initializes the provider.
  void ConfigureAdapterForProvider(bool is_present, bool is_powered) {
    adapter_->SetBluetoothIsPresent(is_present);
    adapter_->SetBluetoothIsPowered(is_powered);
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    hardware_offloading_supported_provider_ =
        std::make_unique<HardwareOffloadingSupportedProvider>();
  }

  bool IsHardwareOffloadingSupported() {
    return hardware_offloading_supported_provider_ &&
           hardware_offloading_supported_provider_->is_enabled();
  }

 protected:
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  std::unique_ptr<HardwareOffloadingSupportedProvider>
      hardware_offloading_supported_provider_;
};

TEST_F(HardwareOffloadingSupportedProviderTest,
       ProviderStateChangeOnAdapterPoweredToggle) {
  ConfigureAdapterForProvider(/*is_present=*/true, /*is_powered=*/true);

  // Hardware offloading initially supported.
  EXPECT_TRUE(IsHardwareOffloadingSupported());

  // Set adapter to not be powered. Expect hardware offloading to not be
  // supported.
  adapter_->SetBluetoothIsPowered(false);
  EXPECT_FALSE(IsHardwareOffloadingSupported());

  // Set adapter to be powered again. Expect hardware offloading to be supported
  // again.
  adapter_->SetBluetoothIsPowered(true);
  EXPECT_TRUE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest,
       ProviderStateChangeOnAdapterPresentToggle) {
  ConfigureAdapterForProvider(/*is_present=*/true, /*is_powered=*/true);

  // Hardware offloading initially supported.
  EXPECT_TRUE(IsHardwareOffloadingSupported());

  // Set adapter to not be present. Expect hardware offloading to not be
  // supported.
  adapter_->SetBluetoothIsPresent(false);
  EXPECT_FALSE(IsHardwareOffloadingSupported());

  // Set adapter to be present again. Expect hardware offloading to be supported
  // again.
  adapter_->SetBluetoothIsPresent(true);
  EXPECT_TRUE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest,
       ProviderStateChangeOnAdapterHardwareOffloadingStatusToFromUndetermined) {
  ConfigureAdapterForProvider(/*is_present=*/true, /*is_powered=*/true);

  // Hardware offloading initially supported.
  EXPECT_TRUE(IsHardwareOffloadingSupported());

  // Set adapter hardware offloading status to undetermined. Expect hardware
  // offloading to not be supported.
  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kUndetermined);
  EXPECT_FALSE(IsHardwareOffloadingSupported());

  // Set adapter hardware offloading status to be supported again. Expect
  // hardware offloading to be supported again.
  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kSupported);
  EXPECT_TRUE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest,
       ProviderStateChangeOnAdapterHardwareOffloadingStatusToFromNotSupported) {
  ConfigureAdapterForProvider(/*is_present=*/true, /*is_powered=*/true);

  // Hardware offloading initially supported.
  EXPECT_TRUE(IsHardwareOffloadingSupported());

  // Set adapter hardware offloading status to not supported. Expect hardware
  // offloading to not be supported.
  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kNotSupported);
  EXPECT_FALSE(IsHardwareOffloadingSupported());

  // Set adapter hardware offloading status to be supported again. Expect
  // hardware offloading to be supported again.
  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kSupported);
  EXPECT_TRUE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest, AdapterNotPresent) {
  ConfigureAdapterForProvider(/*is_present=*/false, /*is_powered=*/false);

  EXPECT_FALSE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest, AdapterPresentNotPowered) {
  ConfigureAdapterForProvider(/*is_present=*/true, /*is_powered*/ false);

  EXPECT_FALSE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest, UndeterminedOffloadingStatus) {
  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kUndetermined);
  EXPECT_FALSE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest, NotSupportedOffloadingStatus) {
  ConfigureAdapterForProvider(/*is_present=*/true, /*is_powered=*/true);
  adapter_->SetHardwareOffloadingStatus(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus::
          kNotSupported);
  EXPECT_FALSE(IsHardwareOffloadingSupported());
}

TEST_F(HardwareOffloadingSupportedProviderTest, SupportedOffloadingStatus) {
  ConfigureAdapterForProvider(/*is_present=*/true, /*is_powered=*/true);
  EXPECT_TRUE(IsHardwareOffloadingSupported());
}

}  // namespace quick_pair
}  // namespace ash
