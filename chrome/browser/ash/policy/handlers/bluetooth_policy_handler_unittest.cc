// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/bluetooth_policy_handler.h"

#include <utility>

#include "base/test/task_environment.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class BluetoothPolicyHandlerTest : public testing::Test {
 protected:
  class TestingBluetoothAdapter : public device::MockBluetoothAdapter {
   public:
    TestingBluetoothAdapter() = default;

    void Shutdown() override {
      is_shutdown_ = true;
      for (auto observer : GetObservers()) {
        observer.AdapterPresentChanged(this, false);
      }
    }
    void Initialize(base::OnceClosure callback) override {
      is_shutdown_ = false;

      adapter_observer_->AdapterPresentChanged(this, true);
      std::move(callback).Run();
    }
    void SetPowered(bool powered,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {
      is_powered_ = powered;

      adapter_observer_->AdapterPoweredChanged(this, powered);
      std::move(callback).Run();
    }
    void SetServiceAllowList(const UUIDList& uuids,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override {
      uuids_ = uuids;
    }
    bool IsPresent() const override { return !is_shutdown_; }
    bool IsPowered() const override { return is_powered_; }
    const UUIDList& GetAllowList() const { return uuids_; }

    void AddObserver(device::BluetoothAdapter::Observer* observer) override {
      DCHECK(!adapter_observer_);
      adapter_observer_ = observer;
    }

   protected:
    ~TestingBluetoothAdapter() override = default;

    bool is_shutdown_ = false;
    bool is_powered_ = true;
    UUIDList uuids_;
    raw_ptr<device::BluetoothAdapter::Observer, DanglingUntriaged>
        adapter_observer_ = nullptr;
  };

  BluetoothPolicyHandlerTest()
      : adapter_(base::MakeRefCounted<TestingBluetoothAdapter>()) {}
  ~BluetoothPolicyHandlerTest() override = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

 protected:
  void SetAllowBluetooth(bool allow_bluetooth) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kAllowBluetooth, allow_bluetooth);
  }

  void SetDeviceAllowedBluetoothServices(base::Value::List allowlist) {
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kDeviceAllowedBluetoothServices,
        base::Value(std::move(allowlist)));
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<TestingBluetoothAdapter> adapter_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

TEST_F(BluetoothPolicyHandlerTest, TestZeroOnOffOn) {
  BluetoothPolicyHandler shutdown_policy_handler(ash::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());

  SetAllowBluetooth(true);
  EXPECT_TRUE(adapter_->IsPresent());

  SetAllowBluetooth(false);
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());

  // Once the Bluetooth stack goes down, it needs a reboot to come back up.
  SetAllowBluetooth(true);
  EXPECT_FALSE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, OffDuringStartup) {
  SetAllowBluetooth(false);
  BluetoothPolicyHandler shutdown_policy_handler(ash::CrosSettings::Get());
  EXPECT_FALSE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothPolicyHandlerTest, OnDuringStartup) {
  SetAllowBluetooth(true);
  BluetoothPolicyHandler shutdown_policy_handler(ash::CrosSettings::Get());
  EXPECT_TRUE(adapter_->IsPresent());
}

TEST_F(BluetoothPolicyHandlerTest, TestSetServiceAllowList) {
  base::Value::List allowlist;
  const char kTestUuid1[] = "0x1124";
  const char kTestUuid2[] = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
  allowlist.Append(kTestUuid1);
  allowlist.Append(kTestUuid2);
  SetDeviceAllowedBluetoothServices(std::move(allowlist));
  BluetoothPolicyHandler bluetooth_policy_handler(ash::CrosSettings::Get());
  const device::BluetoothUUID test_uuid1(kTestUuid1);
  const device::BluetoothUUID test_uuid2(kTestUuid2);
  const std::vector<device::BluetoothUUID>& allowlist_result =
      adapter_->GetAllowList();
  ASSERT_EQ(2u, allowlist_result.size());
  EXPECT_EQ(test_uuid1, allowlist_result[0]);
  EXPECT_EQ(test_uuid2, allowlist_result[1]);
}

TEST_F(BluetoothPolicyHandlerTest, TestPolicySettingsWhileBTNotReady) {
  base::Value::List allowlist;
  const char kTestUuid1[] = "0x1124";
  const char kTestUuid2[] = "0x1108";
  std::vector<device::BluetoothUUID> allowlist_result;
  const device::BluetoothUUID test_uuid1(kTestUuid1);
  const device::BluetoothUUID test_uuid2(kTestUuid2);

  BluetoothPolicyHandler bluetooth_policy_handler(ash::CrosSettings::Get());

  allowlist.Append(kTestUuid1);

  adapter_->Shutdown();
  ASSERT_TRUE(!adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());

  SetDeviceAllowedBluetoothServices(allowlist.Clone());

  // policy is not propagated to adapter since it's not yet ready.
  allowlist_result = adapter_->GetAllowList();
  EXPECT_EQ(0u, allowlist_result.size());

  adapter_->Initialize(base::DoNothing());
  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsInitialized());

  allowlist_result = adapter_->GetAllowList();
  // policy should be set to adapter after adapter is ready.
  ASSERT_EQ(1u, allowlist_result.size());
  EXPECT_EQ(test_uuid1, allowlist_result[0]);

  adapter_->SetPowered(false, base::DoNothing(), base::DoNothing());

  allowlist.Append(kTestUuid2);
  SetDeviceAllowedBluetoothServices(allowlist.Clone());

  // policy is not propagated to adapter since it's not powered.
  allowlist_result = adapter_->GetAllowList();
  ASSERT_EQ(1u, allowlist_result.size());
  EXPECT_EQ(test_uuid1, allowlist_result[0]);

  adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
  ASSERT_TRUE(adapter_->IsPresent());
  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsInitialized());

  // policy should be set to adapter after adapter is ready.
  allowlist_result = adapter_->GetAllowList();
  ASSERT_EQ(2u, allowlist_result.size());
  EXPECT_EQ(test_uuid1, allowlist_result[0]);
  EXPECT_EQ(test_uuid2, allowlist_result[1]);
}
}  // namespace policy
