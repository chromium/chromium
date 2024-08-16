// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/cpp/test/fake_usb_device.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

using device::FakeUsbDevice;
using device::FakeUsbDeviceInfo;
using device::FakeUsbDeviceManager;
using device::mojom::UsbAlternateInterfaceInfo;
using device::mojom::UsbConfigurationInfo;
using device::mojom::UsbConfigurationInfoPtr;
using device::mojom::UsbDevice;
using device::mojom::UsbEndpointInfo;
using device::mojom::UsbEndpointInfoPtr;
using device::mojom::UsbInterfaceInfo;
using device::mojom::UsbInterfaceInfoPtr;
using device::mojom::UsbTransferDirection;
using device::mojom::UsbTransferStatus;
using device::mojom::UsbTransferType;

namespace {

struct NoConfigTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = false;
  static const bool kConfigured = false;
};

struct AndroidTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = false;
  static const bool kConfigured = true;
};

struct NonAndroidTraits {
  static const int kClass = 0xf0;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x2;
  static const bool kBreaks = false;
  static const bool kConfigured = true;
};

struct BreakingAndroidTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = true;
  static const bool kConfigured = true;
};

const uint32_t kMaxPayload = 4096;
const uint32_t kVersion = 0x01000000;
const uint8_t kAndroidConfigValue = 1;

const char kDeviceManufacturer[] = "Test Manufacturer";
const char kDeviceModel[] = "Nexus 6";
const char kDeviceSerial[] = "01498B321301A00A";

UsbConfigurationInfoPtr ConstructAndroidConfig(uint8_t class_code,
                                               uint8_t subclass_code,
                                               uint8_t protocol_code) {
  std::vector<UsbEndpointInfoPtr> endpoints;
  auto endpoint_1 = UsbEndpointInfo::New();
  endpoint_1->endpoint_number = 0x01;
  endpoint_1->direction = UsbTransferDirection::INBOUND;
  endpoint_1->type = UsbTransferType::BULK;
  endpoint_1->packet_size = 512;
  endpoints.push_back(std::move(endpoint_1));

  auto endpoint_2 = UsbEndpointInfo::New();
  endpoint_2->endpoint_number = 0x01;
  endpoint_2->direction = UsbTransferDirection::OUTBOUND;
  endpoint_2->type = UsbTransferType::BULK;
  endpoint_2->packet_size = 512;
  endpoints.push_back(std::move(endpoint_2));

  auto alternate = UsbAlternateInterfaceInfo::New();
  alternate->alternate_setting = 0;
  alternate->class_code = class_code;
  alternate->subclass_code = subclass_code;
  alternate->protocol_code = protocol_code;
  alternate->endpoints = std::move(endpoints);

  auto interface = UsbInterfaceInfo::New();
  interface->interface_number = 0;
  interface->alternates.push_back(std::move(alternate));

  auto config = UsbConfigurationInfo::New();
  config->configuration_value = kAndroidConfigValue;
  config->interfaces.push_back(std::move(interface));

  return config;
}

class FakeAndroidUsbDeviceInfo : public FakeUsbDeviceInfo {
 public:
  explicit FakeAndroidUsbDeviceInfo(bool is_broken)
      : FakeUsbDeviceInfo(/*vendor_id=*/0,
                          /*product_id=*/0,
                          kDeviceManufacturer,
                          kDeviceModel,
                          kDeviceSerial,
                          std::vector<UsbConfigurationInfoPtr>()),
        broken_traits_(is_broken) {}

  bool broken_traits() const { return broken_traits_; }
  device::mojom::UsbDeviceInfoPtr ClonePtr() { return GetDeviceInfo().Clone(); }

 private:
  ~FakeAndroidUsbDeviceInfo() override = default;

  bool broken_traits_;
};

template <class T>
scoped_refptr<FakeAndroidUsbDeviceInfo> ConstructFakeUsbDevice() {
  const bool broken = T::kBreaks;
  auto device = base::MakeRefCounted<FakeAndroidUsbDeviceInfo>(broken);
  auto config = ConstructAndroidConfig(T::kClass, T::kSubclass, T::kProtocol);
  auto config_value = config->configuration_value;
  device->AddConfig(std::move(config));
  if (T::kConfigured)
    device->SetActiveConfig(config_value);

  return device;
}

class MockLocalSocket : public MockAndroidConnection::Delegate {
 public:
  using Callback =
      base::RepeatingCallback<void(int command, const std::string& message)>;

  MockLocalSocket(const Callback& callback,
                  const std::string& serial,
                  const std::string& command)
      : callback_(callback),
        connection_(
            std::make_unique<MockAndroidConnection>(this, serial, command)) {}
  ~MockLocalSocket() override = default;

  void Receive(const std::string& data) {
    connection_->Receive(data);
  }

 private:
  void SendSuccess(const std::string& message) override {
    if (!message.empty())
      callback_.Run(AdbMessage::kCommandWRTE, message);
  }

  void SendRaw(const std::string& message) override {
    callback_.Run(AdbMessage::kCommandWRTE, message);
  }

  void Close() override {
    callback_.Run(AdbMessage::kCommandCLSE, std::string());
  }

  Callback callback_;
  std::unique_ptr<MockAndroidConnection> connection_;
};

class FakeAndroidUsbDevice : public FakeUsbDevice {
 public:
  static void Create(
      scoped_refptr<FakeUsbDeviceInfo> device_info,
      mojo::PendingReceiver<device::mojom::UsbDevice> receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> client) {
    auto* device_object =
        new FakeAndroidUsbDevice(device_info, std::move(client));
    device_object->receiver_ = mojo::MakeSelfOwnedReceiver(
        base::WrapUnique(device_object), std::move(receiver));
  }

  ~FakeAndroidUsbDevice() override = default;

 protected:
  FakeAndroidUsbDevice(
      scoped_refptr<FakeUsbDeviceInfo> device,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> client)
      : FakeUsbDevice(device,
                      /*blocked_interface_classes=*/{},
                      std::move(client)) {
    broken_traits_ =
        static_cast<FakeAndroidUsbDeviceInfo*>(device.get())->broken_traits();
  }

  // override device::FakeUsbDevice
  void GenericTransferIn(uint8_t endpoint_number,
                         uint32_t length,
                         uint32_t timeout,
                         GenericTransferInCallback callback) override {
    queries_.push(Query(std::move(callback), length));
    ProcessQueries();
  }

  void GenericTransferOut(uint8_t endpoint_number,
                          base::span<const uint8_t> buffer,
                          uint32_t timeout,
                          GenericTransferOutCallback callback) override {
    if (remaining_body_length_ == 0) {
      // A new message, parse header first.
      DCHECK_GE(buffer.size(), 6u);
      const auto* header = reinterpret_cast<const uint32_t*>(buffer.data());
      current_message_ = std::make_unique<AdbMessage>(header[0], header[1],
                                                      header[2], std::string());
      remaining_body_length_ = header[3];
      uint32_t magic = header[5];
      if ((current_message_->command ^ 0xffffffff) != magic) {
        DCHECK(false) << "Header checksum error";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      UsbTransferStatus::TRANSFER_ERROR));
        return;
      }
    } else {
      // Parse body.
      DCHECK(current_message_.get());
      current_message_->body += std::string(
          reinterpret_cast<const char*>(buffer.data()), buffer.size());
      remaining_body_length_ -= buffer.size();
    }

    if (remaining_body_length_ == 0) {
      ProcessIncoming();
    }

    UsbTransferStatus status = broken_ ? UsbTransferStatus::TRANSFER_ERROR
                                       : UsbTransferStatus::COMPLETED;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
    ProcessQueries();
  }

  template <class D>
  void append(D data) {
    std::copy(reinterpret_cast<uint8_t*>(&data),
              (reinterpret_cast<uint8_t*>(&data)) + sizeof(D),
              std::back_inserter(output_buffer_));
  }

  // Copied from AndroidUsbDevice::Checksum
  uint32_t Checksum(const std::string& data) {
    unsigned char* x = (unsigned char*)data.data();
    int count = data.length();
    uint32_t sum = 0;
    while (count-- > 0)
      sum += *x++;
    return sum;
  }

  void ProcessIncoming() {
    DCHECK(current_message_.get());
    switch (current_message_->command) {
      case AdbMessage::kCommandCNXN: {
        WriteResponse(kVersion,
                      kMaxPayload,
                      AdbMessage::kCommandCNXN,
                      "device::ro.product.name=SampleProduct;ro.product.model="
                      "SampleModel;ro.product.device=SampleDevice;");
        break;
      }
      case AdbMessage::kCommandCLSE: {
        WriteResponse(0,
                      current_message_->arg0,
                      AdbMessage::kCommandCLSE,
                      std::string());
        local_sockets_.erase(current_message_->arg0);
        break;
      }
      case AdbMessage::kCommandWRTE: {
        if (broken_traits_) {
          broken_ = true;
          return;
        }

        auto it = local_sockets_.find(current_message_->arg0);
        if (it == local_sockets_.end())
          return;

        DCHECK(current_message_->arg1);
        WriteResponse(current_message_->arg1,
                      current_message_->arg0,
                      AdbMessage::kCommandOKAY,
                      std::string());
        it->second->Receive(current_message_->body);
        break;
      }
      case AdbMessage::kCommandOPEN: {
        DCHECK(!current_message_->arg1);
        DCHECK(current_message_->arg0);
        std::string response;
        WriteResponse(++last_local_socket_,
                      current_message_->arg0,
                      AdbMessage::kCommandOKAY,
                      std::string());
        local_sockets_[current_message_->arg0] =
            std::make_unique<MockLocalSocket>(
                base::BindRepeating(&FakeAndroidUsbDevice::WriteResponse,
                                    base::Unretained(this), last_local_socket_,
                                    current_message_->arg0),
                kDeviceSerial,
                current_message_->body.substr(
                    0, current_message_->body.size() - 1));
        return;
      }
      default: {
        return;
      }
    }
    ProcessQueries();
  }

  void WriteResponse(int arg0, int arg1, int command, const std::string& body) {
    append(command);
    append(arg0);
    append(arg1);
    bool add_zero = !body.empty() && (command != AdbMessage::kCommandWRTE);
    append(static_cast<uint32_t>(body.size() + (add_zero ? 1 : 0)));
    append(Checksum(body));
    append(command ^ 0xffffffff);
    const auto* body_head = reinterpret_cast<const uint8_t*>(body.data());
    std::copy(body_head, body_head + body.size(),
              std::back_inserter(output_buffer_));
    if (add_zero) {
      output_buffer_.push_back(0);
    }
    ProcessQueries();
  }

  void ProcessQueries() {
    if (queries_.empty())
      return;

    if (broken_) {
      Query query = std::move(queries_.front());
      queries_.pop();
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(query.callback),
                                    UsbTransferStatus::TRANSFER_ERROR,
                                    std::vector<uint8_t>()));
      return;
    }

    if (queries_.front().size > output_buffer_.size())
      return;

    Query query = std::move(queries_.front());
    queries_.pop();
    std::vector<uint8_t> response_buffer;
    std::copy(output_buffer_.begin(), output_buffer_.begin() + query.size,
              std::back_inserter(response_buffer));
    output_buffer_.erase(output_buffer_.begin(),
                         output_buffer_.begin() + query.size);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(query.callback), UsbTransferStatus::COMPLETED,
                       std::move(response_buffer)));
  }

  struct Query {
    GenericTransferInCallback callback;
    size_t size;

    Query(GenericTransferInCallback callback, int size)
        : callback(std::move(callback)), size(size) {}
  };

  uint32_t remaining_body_length_ = 0;
  std::unique_ptr<AdbMessage> current_message_;
  std::vector<uint8_t> output_buffer_;
  base::queue<Query> queries_;
  std::unordered_map<int, std::unique_ptr<MockLocalSocket>> local_sockets_;
  int last_local_socket_ = 0;
  bool broken_ = false;
  bool broken_traits_ = false;
};

class FakeAndroidUsbManager : public FakeUsbDeviceManager {
 public:
  FakeAndroidUsbManager() = default;
  ~FakeAndroidUsbManager() override = default;

  void GetDevice(
      const std::string& guid,
      const std::vector<uint8_t>& blocked_interface_classes,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver,
      mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client)
      override {
    DCHECK(base::Contains(devices(), guid));
    FakeAndroidUsbDevice::Create(devices()[guid], std::move(device_receiver),
                                 std::move(device_client));
  }
};

class FakeUsbManagerForCheckingTraits : public FakeAndroidUsbManager {
 public:
  FakeUsbManagerForCheckingTraits() = default;
  ~FakeUsbManagerForCheckingTraits() override = default;

  void GetDevices(device::mojom::UsbEnumerationOptionsPtr options,
                  GetDevicesCallback callback) override {
    std::vector<device::mojom::UsbDeviceInfoPtr> device_infos;
    // This switch should be kept in sync with
    // AndroidUsbBrowserTest::DeviceCountChanged.
    switch (step_) {
      case 0:
        // No devices.
        break;
      case 1:
        // Android device.
        device_infos.push_back(
            ConstructFakeUsbDevice<AndroidTraits>()->ClonePtr());
        break;
      case 2:
        // Android and non-android device.
        device_infos.push_back(
            ConstructFakeUsbDevice<AndroidTraits>()->ClonePtr());
        device_infos.push_back(
            ConstructFakeUsbDevice<NonAndroidTraits>()->ClonePtr());
        break;
      case 3:
        // Non-android device.
        device_infos.push_back(
            ConstructFakeUsbDevice<NonAndroidTraits>()->ClonePtr());
        break;
    }
    step_++;
    std::move(callback).Run(std::move(device_infos));
  }

 private:
  int step_ = 0;
};

class DevToolsAndroidBridgeWarmUp
    : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  DevToolsAndroidBridgeWarmUp(base::OnceClosure closure,
                              DevToolsAndroidBridge* adb_bridge)
      : closure_(std::move(closure)), adb_bridge_(adb_bridge) {}
  ~DevToolsAndroidBridgeWarmUp() override = default;

  void DeviceCountChanged(int count) override {
    adb_bridge_->RemoveDeviceCountListener(this);
    std::move(closure_).Run();
  }

 private:
  base::OnceClosure closure_;
  raw_ptr<DevToolsAndroidBridge> adb_bridge_;
};

class AndroidUsbDiscoveryTest : public InProcessBrowserTest {
 protected:
  AndroidUsbDiscoveryTest() = default;
  ~AndroidUsbDiscoveryTest() override = default;

  void SetUpOnMainThread() override {
    adb_bridge_ =
        DevToolsAndroidBridge::Factory::GetForProfile(browser()->profile());
    DCHECK(adb_bridge_);
    adb_bridge_->set_task_scheduler_for_test(base::BindRepeating(
        &AndroidUsbDiscoveryTest::ScheduleDeviceCountRequest,
        base::Unretained(this)));

    AndroidDeviceManager::DeviceProviders providers;
    providers.push_back(
        base::MakeRefCounted<UsbDeviceProvider>(browser()->profile()));
    adb_bridge_->set_device_providers_for_test(providers);
    runner_ = base::MakeRefCounted<content::MessageLoopRunner>();

    // Set a fake USB device manager for AndroidUsbDevice.
    usb_manager_ = CreateFakeUsbManager();
    DCHECK(usb_manager_);
    mojo::PendingRemote<device::mojom::UsbDeviceManager> manager;
    usb_manager_->AddReceiver(manager.InitWithNewPipeAndPassReceiver());
    adb_bridge_->set_usb_device_manager_for_test(std::move(manager));
  }

  void ScheduleDeviceCountRequest(base::OnceClosure request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    scheduler_invoked_++;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(request));
  }

  virtual std::unique_ptr<FakeUsbDeviceManager> CreateFakeUsbManager() {
    auto manager = std::make_unique<FakeAndroidUsbManager>();
    manager->AddDevice(ConstructFakeUsbDevice<AndroidTraits>());
    return manager;
  }

  scoped_refptr<content::MessageLoopRunner> runner_;
  std::unique_ptr<FakeUsbDeviceManager> usb_manager_;
  raw_ptr<DevToolsAndroidBridge, DanglingUntriaged> adb_bridge_;
  int scheduler_invoked_ = 0;
};

class AndroidUsbCountTest : public AndroidUsbDiscoveryTest {
 protected:
  AndroidUsbCountTest() = default;
  ~AndroidUsbCountTest() override = default;

  void SetUpOnMainThread() override {
    AndroidUsbDiscoveryTest::SetUpOnMainThread();
    DevToolsAndroidBridgeWarmUp warmup(runner_->QuitClosure(), adb_bridge_);
    adb_bridge_->AddDeviceCountListener(&warmup);
    runner_->Run();
    runner_ = base::MakeRefCounted<content::MessageLoopRunner>();
  }
};

class AndroidUsbTraitsTest : public AndroidUsbDiscoveryTest {
 protected:
  AndroidUsbTraitsTest() = default;
  ~AndroidUsbTraitsTest() override = default;

  std::unique_ptr<FakeUsbDeviceManager> CreateFakeUsbManager() override {
    return std::make_unique<FakeUsbManagerForCheckingTraits>();
  }
};

class AndroidBreakingUsbTest : public AndroidUsbDiscoveryTest {
 protected:
  AndroidBreakingUsbTest() = default;
  ~AndroidBreakingUsbTest() override = default;

  std::unique_ptr<FakeUsbDeviceManager> CreateFakeUsbManager() override {
    auto manager = std::make_unique<FakeAndroidUsbManager>();
    manager->AddDevice(ConstructFakeUsbDevice<BreakingAndroidTraits>());
    return manager;
  }
};

class AndroidNoConfigUsbTest : public AndroidUsbDiscoveryTest {
 protected:
  AndroidNoConfigUsbTest() = default;
  ~AndroidNoConfigUsbTest() override = default;

  std::unique_ptr<FakeUsbDeviceManager> CreateFakeUsbManager() override {
    auto manager = std::make_unique<FakeAndroidUsbManager>();
    manager->AddDevice(ConstructFakeUsbDevice<AndroidTraits>());
    manager->AddDevice(ConstructFakeUsbDevice<NoConfigTraits>());
    return manager;
  }
};

class MockListListener : public DevToolsAndroidBridge::DeviceListListener {
 public:
  MockListListener(DevToolsAndroidBridge* adb_bridge,
                   base::OnceClosure callback)
      : adb_bridge_(adb_bridge), callback_(std::move(callback)) {}
  ~MockListListener() override = default;

  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override {
    for (const auto& device : devices) {
      if (device->is_connected()) {
        ASSERT_EQ(kDeviceModel, device->model());
        ASSERT_EQ(kDeviceSerial, device->serial());
        adb_bridge_->RemoveDeviceListListener(this);
        std::move(callback_).Run();
        break;
      }
    }
  }

  raw_ptr<DevToolsAndroidBridge> adb_bridge_;
  base::OnceClosure callback_;
};

class MockCountListener : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  explicit MockCountListener(DevToolsAndroidBridge* adb_bridge,
                             base::OnceClosure callback)
      : adb_bridge_(adb_bridge), callback_(std::move(callback)) {}
  ~MockCountListener() override = default;

  void DeviceCountChanged(int count) override {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    Shutdown();
  }

  void Shutdown() { std::move(callback_).Run(); }

  raw_ptr<DevToolsAndroidBridge> adb_bridge_;
  base::OnceClosure callback_;
  int invoked_ = 0;
};

class MockCountListenerWithReAdd : public MockCountListener {
 public:
  explicit MockCountListenerWithReAdd(DevToolsAndroidBridge* adb_bridge,
                                      base::OnceClosure callback)
      : MockCountListener(adb_bridge, std::move(callback)) {}
  ~MockCountListenerWithReAdd() override = default;

  void DeviceCountChanged(int count) override {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    if (readd_count_ > 0) {
      readd_count_--;
      adb_bridge_->AddDeviceCountListener(this);
      adb_bridge_->RemoveDeviceCountListener(this);
      adb_bridge_->AddDeviceCountListener(this);
    } else {
      Shutdown();
    }
  }

  int readd_count_ = 2;
};

class MockCountListenerWithReAddWhileQueued : public MockCountListener {
 public:
  explicit MockCountListenerWithReAddWhileQueued(
      DevToolsAndroidBridge* adb_bridge,
      base::OnceClosure callback)
      : MockCountListener(adb_bridge, std::move(callback)) {}
  ~MockCountListenerWithReAddWhileQueued() override = default;

  void DeviceCountChanged(int count) override {
    ++invoked_;
    if (!readded_) {
      readded_ = true;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&MockCountListenerWithReAddWhileQueued::ReAdd,
                         base::Unretained(this)));
    } else {
      adb_bridge_->RemoveDeviceCountListener(this);
      Shutdown();
    }
  }

  void ReAdd() {
    adb_bridge_->RemoveDeviceCountListener(this);
    adb_bridge_->AddDeviceCountListener(this);
  }

  bool readded_ = false;
};

class MockCountListenerForCheckingTraits : public MockCountListener {
 public:
  explicit MockCountListenerForCheckingTraits(DevToolsAndroidBridge* adb_bridge,
                                              base::OnceClosure callback)
      : MockCountListener(adb_bridge, std::move(callback)) {}
  ~MockCountListenerForCheckingTraits() override = default;

  void DeviceCountChanged(int count) override {
    switch (step_) {
      case 0:
        // Check for 0 devices when no devices present.
        EXPECT_EQ(0, count);
        break;
      case 1:
        // Check for 1 device when only android device present.
        EXPECT_EQ(1, count);
        break;
      case 2:
        // Check for 1 device when android and non-android devices present.
        EXPECT_EQ(1, count);
        break;
      case 3:
        // Check for 0 devices when only non-android devices present.
        EXPECT_EQ(0, count);
        adb_bridge_->RemoveDeviceCountListener(this);
        Shutdown();
        break;
      default:
        EXPECT_TRUE(false) << "Unknown step " << step_;
    }
    step_++;
  }

  int step_ = 0;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AndroidUsbDiscoveryTest, TestDeviceDiscovery) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidBreakingUsbTest, TestDeviceBreaking) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidNoConfigUsbTest, TestDeviceNoConfig) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveInCallback) {
  MockCountListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddInCallback) {
  MockCountListenerWithReAdd listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(3, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddOnStart) {
  MockCountListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceCountListener(&listener);
  adb_bridge_->RemoveDeviceCountListener(&listener);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddWhileQueued) {
  MockCountListenerWithReAddWhileQueued listener(adb_bridge_,
                                                 runner_->QuitClosure());
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(2, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbTraitsTest, TestDeviceCounting) {
  MockCountListenerForCheckingTraits listener(adb_bridge_,
                                              runner_->QuitClosure());
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
}
