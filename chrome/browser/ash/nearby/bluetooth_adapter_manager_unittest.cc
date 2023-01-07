// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/bluetooth_adapter_manager.h"

#include "base/test/with_feature_override.h"
#include "device/bluetooth/floss/floss_features.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;

namespace ash {
namespace nearby {

class BluetoothAdapterManagerTest : public base::test::WithFeatureOverride,
                                    public testing::Test {
 public:
  BluetoothAdapterManagerTest()
      : base::test::WithFeatureOverride(floss::features::kFlossEnabled) {}

  ~BluetoothAdapterManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    bluetooth_adapter_manager_ = std::make_unique<BluetoothAdapterManager>();

    mock_bluetooth_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_bluetooth_adapter_, SetStandardChromeOSAdapterName())
        .WillByDefault(
            Invoke(this, &BluetoothAdapterManagerTest::SetStandardName));
    ON_CALL(*mock_bluetooth_adapter_, SetDiscoverable(_, _, _))
        .WillByDefault(
            Invoke(this, &BluetoothAdapterManagerTest::SetDiscoverable));
  }

  void Initialize() {
    mojo::PendingReceiver<bluetooth::mojom::Adapter> pending_receiver;
    bluetooth_manager()->Initialize(std::move(pending_receiver),
                                    mock_bluetooth_adapter_);
  }

  void SetStandardName() { set_standard_name_called_ = true; }

  bool IsSetStandardNameCalled() { return set_standard_name_called_; }

  void SetDiscoverable() { set_discoverable_called_ = true; }

  bool IsSetDiscoverableCalled() { return set_discoverable_called_; }

  BluetoothAdapterManager* bluetooth_manager() {
    return bluetooth_adapter_manager_.get();
  }

 private:
  std::unique_ptr<BluetoothAdapterManager> bluetooth_adapter_manager_;
  scoped_refptr<NiceMock<device::MockBluetoothAdapter>> mock_bluetooth_adapter_;
  bool set_standard_name_called_ = false;
  bool set_discoverable_called_ = false;
};

TEST_P(BluetoothAdapterManagerTest, Shutdown) {
  Initialize();
  bluetooth_manager()->Shutdown();
  EXPECT_TRUE(IsSetStandardNameCalled());
  EXPECT_TRUE(IsSetDiscoverableCalled());
}

TEST_P(BluetoothAdapterManagerTest, Shutdown_NeverInitialized) {
  bluetooth_manager()->Shutdown();
  EXPECT_FALSE(IsSetStandardNameCalled());
  EXPECT_FALSE(IsSetDiscoverableCalled());
  // Verify that nothing crashes.
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(BluetoothAdapterManagerTest);

}  // namespace nearby
}  // namespace ash
