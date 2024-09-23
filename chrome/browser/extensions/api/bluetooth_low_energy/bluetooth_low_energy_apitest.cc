// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_descriptor.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_api.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::test::RunOnceCallback;
using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothGattConnection;
using device::BluetoothGattNotifySession;
using device::BluetoothGattService;
using device::BluetoothRemoteGattCharacteristic;
using device::BluetoothRemoteGattDescriptor;
using device::BluetoothRemoteGattService;
using device::BluetoothUUID;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothGattCharacteristic;
using device::MockBluetoothGattConnection;
using device::MockBluetoothGattDescriptor;
using device::MockBluetoothGattNotifySession;
using device::MockBluetoothGattService;
using extensions::BluetoothLowEnergyEventRouter;
using extensions::ResultCatcher;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;
using testing::SaveArg;

namespace {

// Test service constants.
const char kTestLeDeviceAddress0[] = "11:22:33:44:55:66";
const char kTestLeDeviceName0[] = "Test LE Device 0";

const char kTestLeDeviceAddress1[] = "77:88:99:AA:BB:CC";
const char kTestLeDeviceName1[] = "Test LE Device 1";

const char kTestServiceId0[] = "service_id0";
const char kTestServiceUuid0[] = "1234";

const char kTestServiceId1[] = "service_id1";
const char kTestServiceUuid1[] = "5678";

// Test characteristic constants.
const char kTestCharacteristicId0[] = "char_id0";
const char kTestCharacteristicUuid0[] = "1211";
const BluetoothRemoteGattCharacteristic::Properties
    kTestCharacteristicProperties0 =
        BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
        BluetoothRemoteGattCharacteristic::PROPERTY_READ |
        BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE |
        BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE;
const uint8_t kTestCharacteristicDefaultValue0[] = {0x01, 0x02, 0x03, 0x04,
                                                    0x05};

const char kTestCharacteristicId1[] = "char_id1";
const char kTestCharacteristicUuid1[] = "1212";
const BluetoothRemoteGattCharacteristic::Properties
    kTestCharacteristicProperties1 =
        BluetoothRemoteGattCharacteristic::PROPERTY_READ |
        BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
        BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY;
const uint8_t kTestCharacteristicDefaultValue1[] = {0x06, 0x07, 0x08};

const char kTestCharacteristicId2[] = "char_id2";
const char kTestCharacteristicUuid2[] = "1213";
const BluetoothRemoteGattCharacteristic::Properties
    kTestCharacteristicProperties2 =
        BluetoothRemoteGattCharacteristic::PROPERTY_NONE;

// Test descriptor constants.
const char kTestDescriptorId0[] = "desc_id0";
const char kTestDescriptorUuid0[] = "1221";
const uint8_t kTestDescriptorDefaultValue0[] = {0x01, 0x02, 0x03};

const char kTestDescriptorId1[] = "desc_id1";
const char kTestDescriptorUuid1[] = "1222";
const uint8_t kTestDescriptorDefaultValue1[] = {0x04, 0x05};

class BluetoothLowEnergyApiTest : public extensions::ExtensionApiTest {
 public:
  BluetoothLowEnergyApiTest() {}

  ~BluetoothLowEnergyApiTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    empty_extension_ = extensions::ExtensionBuilder("Test").Build();
    SetUpMocks();
  }

  void TearDownOnMainThread() override {
    EXPECT_CALL(*mock_adapter_, RemoveObserver(_));
  }

  void SetUpMocks() {
    mock_adapter_ = new testing::StrictMock<MockBluetoothAdapter>();
    EXPECT_CALL(*mock_adapter_, GetDevices())
        .WillOnce(Return(BluetoothAdapter::ConstDeviceList()));

    event_router()->SetAdapterForTesting(mock_adapter_);

    device0_ = std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
        mock_adapter_, 0, kTestLeDeviceName0, kTestLeDeviceAddress0,
        /*paired=*/false, /*connected=*/true);

    device1_ = std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
        mock_adapter_, 0, kTestLeDeviceName1, kTestLeDeviceAddress1,
        /*paired=*/false, /*connected=*/true);

    service0_ = std::make_unique<testing::NiceMock<MockBluetoothGattService>>(
        device0_.get(), kTestServiceId0, BluetoothUUID(kTestServiceUuid0),
        /*is_primary=*/true);

    service1_ = std::make_unique<testing::NiceMock<MockBluetoothGattService>>(
        device0_.get(), kTestServiceId1, BluetoothUUID(kTestServiceUuid1),
        /*is_primary=*/false);

    // Assign characteristics some random properties and permissions. They don't
    // need to reflect what the characteristic is actually capable of, since
    // the JS API just passes values through from
    // device::BluetoothRemoteGattCharacteristic.
    std::vector<uint8_t> default_value;
    chrc0_ =
        std::make_unique<testing::NiceMock<MockBluetoothGattCharacteristic>>(
            service0_.get(), kTestCharacteristicId0,
            BluetoothUUID(kTestCharacteristicUuid0),
            kTestCharacteristicProperties0,
            BluetoothRemoteGattCharacteristic::PERMISSION_NONE);
    default_value.assign(kTestCharacteristicDefaultValue0,
                         (kTestCharacteristicDefaultValue0 +
                          sizeof(kTestCharacteristicDefaultValue0)));
    ON_CALL(*chrc0_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));

    chrc1_ =
        std::make_unique<testing::NiceMock<MockBluetoothGattCharacteristic>>(
            service0_.get(), kTestCharacteristicId1,
            BluetoothUUID(kTestCharacteristicUuid1),
            kTestCharacteristicProperties1,
            BluetoothRemoteGattCharacteristic::PERMISSION_NONE);
    default_value.assign(kTestCharacteristicDefaultValue1,
                         (kTestCharacteristicDefaultValue1 +
                          sizeof(kTestCharacteristicDefaultValue1)));
    ON_CALL(*chrc1_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));

    chrc2_ =
        std::make_unique<testing::NiceMock<MockBluetoothGattCharacteristic>>(
            service1_.get(), kTestCharacteristicId2,
            BluetoothUUID(kTestCharacteristicUuid2),
            kTestCharacteristicProperties2,
            BluetoothRemoteGattCharacteristic::PERMISSION_NONE);

    desc0_ = std::make_unique<testing::NiceMock<MockBluetoothGattDescriptor>>(
        chrc0_.get(), kTestDescriptorId0, BluetoothUUID(kTestDescriptorUuid0),
        BluetoothRemoteGattCharacteristic::PERMISSION_NONE);
    default_value.assign(
        kTestDescriptorDefaultValue0,
        (kTestDescriptorDefaultValue0 + sizeof(kTestDescriptorDefaultValue0)));
    ON_CALL(*desc0_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));

    desc1_ = std::make_unique<testing::NiceMock<MockBluetoothGattDescriptor>>(
        chrc0_.get(), kTestDescriptorId1, BluetoothUUID(kTestDescriptorUuid1),
        BluetoothRemoteGattCharacteristic::PERMISSION_NONE);
    default_value.assign(
        kTestDescriptorDefaultValue1,
        (kTestDescriptorDefaultValue1 + sizeof(kTestDescriptorDefaultValue1)));
    ON_CALL(*desc1_, GetValue()).WillByDefault(ReturnRefOfCopy(default_value));
  }

 protected:
  BluetoothLowEnergyEventRouter* event_router() {
    return extensions::BluetoothLowEnergyAPI::Get(browser()->profile())
        ->event_router();
  }

  // This field is not a raw_ptr<> because problems related to passing to a
  // templated && parameter, which is later forwarded to something that doesn't
  // vibe with raw_ptr<T>.
  RAW_PTR_EXCLUSION testing::StrictMock<MockBluetoothAdapter>* mock_adapter_;
  std::unique_ptr<testing::NiceMock<MockBluetoothDevice>> device0_;
  std::unique_ptr<testing::NiceMock<MockBluetoothDevice>> device1_;
  std::unique_ptr<testing::NiceMock<MockBluetoothGattService>> service0_;
  std::unique_ptr<testing::NiceMock<MockBluetoothGattService>> service1_;
  std::unique_ptr<testing::NiceMock<MockBluetoothGattCharacteristic>> chrc0_;
  std::unique_ptr<testing::NiceMock<MockBluetoothGattCharacteristic>> chrc1_;
  std::unique_ptr<testing::NiceMock<MockBluetoothGattCharacteristic>> chrc2_;
  std::unique_ptr<testing::NiceMock<MockBluetoothGattDescriptor>> desc0_;
  std::unique_ptr<testing::NiceMock<MockBluetoothGattDescriptor>> desc1_;

 private:
  scoped_refptr<const extensions::Extension> empty_extension_;
};

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  std::move(std::get<k>(args)).Run();
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(std::get<k>(args)).Run(p0);
}

ACTION_TEMPLATE(InvokeCallbackArgument,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  std::move(std::get<k>(args)).Run(p0, p1);
}

ACTION_TEMPLATE(InvokeCallbackWithScopedPtrArg,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_1_VALUE_PARAMS(p0)) {
  std::move(std::get<k>(args)).Run(std::unique_ptr<T>(p0));
}

ACTION_TEMPLATE(InvokeCallbackWithScopedPtrArg,
                HAS_2_TEMPLATE_PARAMS(int, k, typename, T),
                AND_2_VALUE_PARAMS(p0, p1)) {
  std::move(std::get<k>(args)).Run(std::unique_ptr<T>(p0), p1);
}

std::unique_ptr<BluetoothGattConnection> CreateGattConnection(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address,
    bool expect_disconnect) {
  auto conn = std::make_unique<testing::NiceMock<MockBluetoothGattConnection>>(
      adapter, device_address);
  EXPECT_CALL(*conn, Disconnect()).Times(expect_disconnect ? 1 : 0);
  return conn;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetServices) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  std::vector<BluetoothRemoteGattService*> services;
  services.push_back(service0_.get());
  services.push_back(service1_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothDevice*>(nullptr)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattServices())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothRemoteGattService*>()))
      .WillOnce(Return(services));

  // Load and wait for setup.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_services")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetService) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothDevice*>(nullptr)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(2)
      .WillOnce(Return(static_cast<BluetoothRemoteGattService*>(nullptr)))
      .WillOnce(Return(service0_.get()));

  // Load and wait for setup.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_service")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ServiceEvents) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // Load the extension and let it set up.
  ExtensionTestMessageListener listener(ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/service_events")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // These will create the identifier mappings.
  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service1_.get());

  // These will send the onServiceAdded event to apps.
  event_router()->GattDiscoveryCompleteForService(mock_adapter_,
                                                  service0_.get());
  event_router()->GattDiscoveryCompleteForService(mock_adapter_,
                                                  service1_.get());

  // This will send the onServiceChanged event to apps.
  event_router()->GattServiceChanged(mock_adapter_, service1_.get());

  // This will send the  onServiceRemoved event to apps.
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());

  ASSERT_EQ("ready", listener.message()) << listener.message();
  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service1_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedService) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // Load the extension and let it set up.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_removed_service")));

  // 1. getService success.
  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));

  ExtensionTestMessageListener get_service_success_listener;

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattDiscoveryCompleteForService(mock_adapter_,
                                                  service0_.get());

  EXPECT_TRUE(get_service_success_listener.WaitUntilSatisfied());
  ASSERT_EQ("getServiceSuccess", get_service_success_listener.message())
      << get_service_success_listener.message();
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());

  // 2. getService fail.
  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0)).Times(0);

  ExtensionTestMessageListener get_service_fail_listener(
      ReplyBehavior::kWillReply);

  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());

  EXPECT_TRUE(get_service_fail_listener.WaitUntilSatisfied());
  ASSERT_EQ("getServiceFail", get_service_fail_listener.message())
      << get_service_fail_listener.message();
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());

  get_service_fail_listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetIncludedServices) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // Wait for initial call to end with failure as there is no mapping.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_included_services")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Set up for the rest of the calls before replying. Included services can be
  // returned even if there is no instance ID mapping for them yet, so no need
  // to call GattServiceAdded for |service1_| here.
  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());

  std::vector<BluetoothRemoteGattService*> includes;
  includes.push_back(service1_.get());
  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .Times(2)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(2)
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetIncludedServices())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothRemoteGattService*>()))
      .WillOnce(Return(includes));

  listener.Reply("go");
  listener.Reset();

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetCharacteristics) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  std::vector<BluetoothRemoteGattCharacteristic*> characteristics;
  characteristics.push_back(chrc0_.get());
  characteristics.push_back(chrc1_.get());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(3).WillRepeatedly(
      Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothRemoteGattService*>(nullptr)))
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristics())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothRemoteGattCharacteristic*>()))
      .WillOnce(Return(characteristics));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_characteristics")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetCharacteristic) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(4)
      .WillOnce(Return(static_cast<BluetoothDevice*>(nullptr)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillOnce(Return(static_cast<BluetoothRemoteGattService*>(nullptr)))
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(2)
      .WillOnce(
          Return(static_cast<BluetoothRemoteGattCharacteristic*>(nullptr)))
      .WillOnce(Return(chrc0_.get()));

  // Load the extension and wait for first test.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_characteristic")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, CharacteristicProperties) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(12)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(12)
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(12)
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetProperties())
      .Times(12)
      .WillOnce(Return(BluetoothRemoteGattCharacteristic::PROPERTY_NONE))
      .WillOnce(Return(BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST))
      .WillOnce(Return(BluetoothRemoteGattCharacteristic::PROPERTY_READ))
      .WillOnce(Return(
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE))
      .WillOnce(Return(BluetoothRemoteGattCharacteristic::PROPERTY_WRITE))
      .WillOnce(Return(BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY))
      .WillOnce(Return(BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE))
      .WillOnce(Return(BluetoothRemoteGattCharacteristic::
                           PROPERTY_AUTHENTICATED_SIGNED_WRITES))
      .WillOnce(Return(
          BluetoothRemoteGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES))
      .WillOnce(
          Return(BluetoothRemoteGattCharacteristic::PROPERTY_RELIABLE_WRITE))
      .WillOnce(Return(
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES))
      .WillOnce(Return(
          BluetoothRemoteGattCharacteristic::PROPERTY_BROADCAST |
          BluetoothRemoteGattCharacteristic::PROPERTY_READ |
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE |
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
          BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY |
          BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE |
          BluetoothRemoteGattCharacteristic::
              PROPERTY_AUTHENTICATED_SIGNED_WRITES |
          BluetoothRemoteGattCharacteristic::PROPERTY_EXTENDED_PROPERTIES |
          BluetoothRemoteGattCharacteristic::PROPERTY_RELIABLE_WRITE |
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITABLE_AUXILIARIES));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/characteristic_properties")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedCharacteristic) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());

  ExtensionTestMessageListener listener(ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_removed_characteristic")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("ready", listener.message()) << listener.message();
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());
  testing::Mock::VerifyAndClearExpectations(service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device0_, GetGattService(_)).Times(0);
  EXPECT_CALL(*service0_, GetCharacteristic(_)).Times(0);

  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());

  listener.Reply("go");
  listener.Reset();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("ready", listener.message()) << listener.message();

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, CharacteristicValueChanged) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // Cause events to be sent to the extension.
  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service1_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc2_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(2)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId1))
      .Times(1)
      .WillOnce(Return(service1_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));
  EXPECT_CALL(*service1_, GetCharacteristic(kTestCharacteristicId2))
      .Times(1)
      .WillOnce(Return(chrc2_.get()));

  BluetoothGattNotifySession* session0 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          chrc0_->GetWeakPtr());
  BluetoothGattNotifySession* session1 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          chrc2_->GetWeakPtr());

  EXPECT_CALL(*chrc0_, StartNotifySession_(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
          session0));
  EXPECT_CALL(*chrc2_, StartNotifySession_(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
          session1));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/characteristic_value_changed")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  std::vector<uint8_t> value;
  event_router()->GattCharacteristicValueChanged(
      mock_adapter_, chrc0_.get(), value);
  event_router()->GattCharacteristicValueChanged(
      mock_adapter_, chrc2_.get(), value);

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc2_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service1_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ReadCharacteristicValue) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(4)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(4)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(4)
      .WillRepeatedly(Return(chrc0_.get()));

  std::vector<uint8_t> value;
  EXPECT_CALL(*chrc0_, ReadRemoteCharacteristic_(_))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kFailed,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(std::nullopt, value));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/read_characteristic_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, WriteCharacteristicValue) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  std::vector<uint8_t> write_value;
  EXPECT_CALL(*chrc0_, DeprecatedWriteRemoteCharacteristic_(_, _, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<2>(
          BluetoothGattService::GattErrorCode::kFailed))
      .WillOnce(DoAll(SaveArg<0>(&write_value), InvokeCallbackArgument<1>()));

  EXPECT_CALL(*chrc0_, GetValue()).Times(1).WillOnce(ReturnRef(write_value));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/write_characteristic_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetDescriptors) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  std::vector<BluetoothRemoteGattDescriptor*> descriptors;
  descriptors.push_back(desc0_.get());
  descriptors.push_back(desc1_.get());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillOnce(
          Return(static_cast<BluetoothRemoteGattCharacteristic*>(nullptr)))
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetDescriptors())
      .Times(2)
      .WillOnce(Return(std::vector<BluetoothRemoteGattDescriptor*>()))
      .WillOnce(Return(descriptors));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_descriptors")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetDescriptor) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(5)
      .WillOnce(Return(static_cast<BluetoothDevice*>(nullptr)))
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(4)
      .WillOnce(Return(static_cast<BluetoothRemoteGattService*>(nullptr)))
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillOnce(
          Return(static_cast<BluetoothRemoteGattCharacteristic*>(nullptr)))
      .WillRepeatedly(Return(chrc0_.get()));

  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(2)
      .WillOnce(Return(static_cast<BluetoothRemoteGattDescriptor*>(nullptr)))
      .WillOnce(Return(desc0_.get()));

  // Load the extension and wait for first test.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/get_descriptor")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(mock_adapter_, desc0_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GetRemovedDescriptor) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(1)
      .WillOnce(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(1)
      .WillOnce(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(1)
      .WillOnce(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(1)
      .WillOnce(Return(desc0_.get()));

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc0_.get());

  ExtensionTestMessageListener listener(ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/get_removed_descriptor")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("ready", listener.message()) << listener.message();
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device0_.get());
  testing::Mock::VerifyAndClearExpectations(service0_.get());
  testing::Mock::VerifyAndClearExpectations(chrc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_)).Times(0);
  EXPECT_CALL(*device0_, GetGattService(_)).Times(0);
  EXPECT_CALL(*service0_, GetCharacteristic(_)).Times(0);
  EXPECT_CALL(*chrc0_, GetDescriptor(_)).Times(0);

  event_router()->GattDescriptorRemoved(mock_adapter_, desc0_.get());

  listener.Reply("go");
  listener.Reset();
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("ready", listener.message()) << listener.message();

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, DescriptorValueChanged) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc1_.get());

  // Load the extension and let it set up.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/descriptor_value_changed")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Cause events to be sent to the extension.
  std::vector<uint8_t> value;
  event_router()->GattDescriptorValueChanged(
      mock_adapter_, desc0_.get(), value);
  event_router()->GattDescriptorValueChanged(
      mock_adapter_, desc1_.get(), value);

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattDescriptorRemoved(mock_adapter_, desc1_.get());
  event_router()->GattDescriptorRemoved(mock_adapter_, desc0_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ReadDescriptorValue) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(9)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(9)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(9)
      .WillRepeatedly(Return(chrc0_.get()));

  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(9)
      .WillRepeatedly(Return(desc0_.get()));

  std::vector<uint8_t> value;
  EXPECT_CALL(*desc0_, ReadRemoteDescriptor_(_))
      .Times(8)
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kFailed,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kInvalidLength,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kNotPermitted,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kNotAuthorized,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kNotPaired,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kNotSupported,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(
          BluetoothGattService::GattErrorCode::kInProgress,
          /*value=*/std::vector<uint8_t>()))
      .WillOnce(InvokeCallbackArgument<0>(/*error_code=*/std::nullopt, value));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/read_descriptor_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(mock_adapter_, desc0_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, WriteDescriptorValue) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .Times(3)
      .WillRepeatedly(Return(device0_.get()));

  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .Times(3)
      .WillRepeatedly(Return(service0_.get()));

  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(3)
      .WillRepeatedly(Return(chrc0_.get()));

  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .Times(3)
      .WillRepeatedly(Return(desc0_.get()));

  std::vector<uint8_t> write_value;
  EXPECT_CALL(*desc0_, WriteRemoteDescriptor_(_, _, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<2>(
          BluetoothGattService::GattErrorCode::kFailed))
      .WillOnce(DoAll(SaveArg<0>(&write_value), InvokeCallbackArgument<1>()));

  EXPECT_CALL(*desc0_, GetValue()).Times(1).WillOnce(ReturnRef(write_value));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/write_descriptor_value")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(mock_adapter_, desc0_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, PermissionDenied) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/permission_denied")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, UuidPermissionMethods) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc0_.get());

  std::vector<BluetoothRemoteGattService*> services;
  services.push_back(service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattServices()).WillOnce(Return(services));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*chrc0_, GetDescriptor(kTestDescriptorId0))
      .WillRepeatedly(Return(desc0_.get()));

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/uuid_permission_methods")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattDescriptorRemoved(mock_adapter_, desc0_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, UuidPermissionEvents) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener listener(ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/uuid_permission_events")));

  // Cause events to be sent to the extension.
  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattDescriptorAdded(mock_adapter_, desc0_.get());

  std::vector<uint8_t> value;
  event_router()->GattCharacteristicValueChanged(
      mock_adapter_, chrc0_.get(), value);
  event_router()->GattDescriptorValueChanged(
      mock_adapter_, desc0_.get(), value);
  event_router()->GattServiceChanged(mock_adapter_, service0_.get());

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_EQ("ready", listener.message()) << listener.message();

  event_router()->GattDescriptorRemoved(mock_adapter_, desc0_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, GattConnection) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .WillRepeatedly(Return(static_cast<BluetoothDevice*>(nullptr)));
  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress1))
      .WillRepeatedly(Return(device1_.get()));
  static_assert(
      BluetoothDevice::NUM_CONNECT_ERROR_CODES == 21,
      "Update required if the number of BluetoothDevice enums changes.");
  EXPECT_CALL(*device0_, CreateGattConnection(_, _))
      .Times(13)
      .WillOnce(RunOnceCallback<0>(/*connection=*/nullptr,
                                   BluetoothDevice::ERROR_FAILED))
      .WillOnce(RunOnceCallback<0>(/*connection=*/nullptr,
                                   BluetoothDevice::ERROR_INPROGRESS))
      .WillOnce(RunOnceCallback<0>(/*connection=*/nullptr,
                                   BluetoothDevice::ERROR_AUTH_FAILED))
      .WillOnce(RunOnceCallback<0>(/*connection=*/nullptr,
                                   BluetoothDevice::ERROR_AUTH_REJECTED))
      .WillOnce(RunOnceCallback<0>(/*connection=*/nullptr,
                                   BluetoothDevice::ERROR_AUTH_CANCELED))
      .WillOnce(RunOnceCallback<0>(/*connection=*/nullptr,
                                   BluetoothDevice::ERROR_AUTH_TIMEOUT))
      .WillOnce(RunOnceCallback<0>(
          /*connection=*/nullptr, BluetoothDevice::ERROR_UNSUPPORTED_DEVICE))
      .WillOnce(RunOnceCallback<0>(
          /*connection=*/nullptr, BluetoothDevice::ERROR_NO_MEMORY))
      .WillOnce(RunOnceCallback<0>(
          /*connection=*/nullptr, BluetoothDevice::ERROR_JNI_ENVIRONMENT))
      .WillOnce(RunOnceCallback<0>(
          /*connection=*/nullptr, BluetoothDevice::ERROR_JNI_THREAD_ATTACH))
      .WillOnce(RunOnceCallback<0>(
          /*connection=*/nullptr, BluetoothDevice::ERROR_WAKELOCK))
      .WillOnce(RunOnceCallback<0>(
          CreateGattConnection(mock_adapter_, kTestLeDeviceAddress0,
                               /*expect_disconnect=*/true),
          /*error_code=*/std::nullopt))
      .WillOnce(RunOnceCallback<0>(
          CreateGattConnection(mock_adapter_, kTestLeDeviceAddress0,
                               /*expect_disconnect=*/false),
          /*error_code=*/std::nullopt));
  EXPECT_CALL(*device1_, CreateGattConnection(_, _))
      .Times(1)
      .WillOnce(RunOnceCallback<0>(
          CreateGattConnection(mock_adapter_, kTestLeDeviceAddress1,
                               /*expect_disconnect=*/true),
          /*error_code=*/std::nullopt));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/gatt_connection")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ReconnectAfterDisconnected) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .WillRepeatedly(Return(device0_.get()));

  auto first_conn = CreateGattConnection(mock_adapter_, kTestLeDeviceAddress0,
                                         /*expect_disconnect=*/false);
  MockBluetoothGattConnection* first_conn_ptr =
      static_cast<MockBluetoothGattConnection*>(first_conn.get());
  EXPECT_CALL(*first_conn_ptr, IsConnected())
      .Times(2)
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  EXPECT_CALL(*device0_, CreateGattConnection(_, _))
      .Times(2)
      .WillOnce(RunOnceCallback<0>(std::move(first_conn),
                                   /*error_code=*/std::nullopt))
      .WillOnce(RunOnceCallback<0>(
          CreateGattConnection(mock_adapter_, kTestLeDeviceAddress0,
                               /*expect_disconnect=*/false),
          /*error_code=*/std::nullopt));

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/reconnect_after_disconnected")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, ConnectInProgress) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  EXPECT_CALL(*mock_adapter_, GetDevice(kTestLeDeviceAddress0))
      .WillRepeatedly(Return(device0_.get()));

  BluetoothDevice::GattConnectionCallback connect_callback;

  testing::NiceMock<MockBluetoothGattConnection>* conn =
      new testing::NiceMock<MockBluetoothGattConnection>(mock_adapter_,
                                                         kTestLeDeviceAddress0);
  std::unique_ptr<BluetoothGattConnection> conn_ptr(conn);
  EXPECT_CALL(*conn, Disconnect()).Times(1);

  EXPECT_CALL(*device0_, CreateGattConnection(_, _))
      .Times(1)
      .WillOnce(MoveArg<0>(&connect_callback));

  ExtensionTestMessageListener listener;
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/connect_in_progress")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("After 2nd connect fails due to 1st connect being in progress.",
            listener.message())
      << listener.message();
  listener.Reset();

  std::move(connect_callback)
      .Run(std::move(conn_ptr), /*error_code=*/std::nullopt);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("After 2nd call to disconnect.", listener.message())
      << listener.message();

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, StartStopNotifications) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service0_.get());
  event_router()->GattServiceAdded(
      mock_adapter_, device0_.get(), service1_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc0_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc1_.get());
  event_router()->GattCharacteristicAdded(mock_adapter_, chrc2_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .WillRepeatedly(Return(service0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId1))
      .WillRepeatedly(Return(service1_.get()));
  EXPECT_CALL(*service1_, GetCharacteristic(kTestCharacteristicId2))
      .Times(1)
      .WillOnce(Return(chrc2_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId0))
      .Times(2)
      .WillRepeatedly(Return(chrc0_.get()));
  EXPECT_CALL(*service0_, GetCharacteristic(kTestCharacteristicId1))
      .Times(1)
      .WillOnce(Return(chrc1_.get()));

  BluetoothGattNotifySession* session0 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          chrc0_->GetWeakPtr());
  MockBluetoothGattNotifySession* session1 =
      new testing::NiceMock<MockBluetoothGattNotifySession>(
          chrc1_->GetWeakPtr());

  EXPECT_CALL(*session1, Stop_(_))
      .Times(1)
      .WillOnce(InvokeCallbackArgument<0>());

  EXPECT_CALL(*chrc0_, StartNotifySession_(_, _))
      .Times(2)
      .WillOnce(InvokeCallbackArgument<1>(
          BluetoothGattService::GattErrorCode::kFailed))
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
          session0));
  EXPECT_CALL(*chrc1_, StartNotifySession_(_, _))
      .Times(1)
      .WillOnce(InvokeCallbackWithScopedPtrArg<0, BluetoothGattNotifySession>(
          session1));

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(
      "bluetooth_low_energy/start_stop_notifications")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());

  std::vector<uint8_t> value;
  event_router()->GattCharacteristicValueChanged(
      mock_adapter_, chrc0_.get(), value);
  event_router()->GattCharacteristicValueChanged(
      mock_adapter_, chrc1_.get(), value);
  event_router()->GattCharacteristicValueChanged(
      mock_adapter_, chrc2_.get(), value);

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc2_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc1_.get());
  event_router()->GattCharacteristicRemoved(mock_adapter_, chrc0_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service1_.get());
  event_router()->GattServiceRemoved(
      mock_adapter_, device0_.get(), service0_.get());
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTest, AddressChange) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  event_router()->GattServiceAdded(mock_adapter_, device0_.get(),
                                   service0_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(_))
      .WillRepeatedly(Return(device0_.get()));
  EXPECT_CALL(*device0_, GetGattService(kTestServiceId0))
      .WillRepeatedly(Return(service0_.get()));

  const std::string kTestLeDeviceNewAddress0 = std::string("11:22:33:44:55:77");
  EXPECT_CALL(*device0_, GetAddress())
      .WillRepeatedly(Return(kTestLeDeviceNewAddress0));

  std::string old_address;
  event_router()->DeviceAddressChanged(mock_adapter_, device0_.get(),
                                       kTestLeDeviceAddress0);

  // Load and wait for setup.
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  listener.set_failure_message("fail");
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bluetooth_low_energy/address_change")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  event_router()->GattServiceRemoved(mock_adapter_, device0_.get(),
                                     service0_.get());
}

}  // namespace
