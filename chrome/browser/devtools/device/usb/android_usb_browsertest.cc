// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/containers/queue.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/devtools/device/adb/mock_adb_server.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "device/base/device_client.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using device::DeviceClient;
using device::MockUsbService;
using device::UsbConfigDescriptor;
using device::UsbDevice;
using device::UsbDeviceHandle;
using device::UsbEndpointDescriptor;
using device::UsbInterfaceDescriptor;
using device::UsbService;
using device::UsbSynchronizationType;
using device::UsbTransferDirection;
using device::UsbTransferStatus;
using device::UsbTransferType;
using device::UsbUsageType;

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

const char kDeviceManufacturer[] = "Test Manufacturer";
const char kDeviceModel[] = "Nexus 6";
const char kDeviceSerial[] = "01498B321301A00A";

template <class T>
class MockUsbDevice;

class MockLocalSocket : public MockAndroidConnection::Delegate {
 public:
  using Callback = base::Callback<void(int command,
                                       const std::string& message)>;

  MockLocalSocket(const Callback& callback,
                  const std::string& serial,
                  const std::string& command)
      : callback_(callback),
        connection_(new MockAndroidConnection(this, serial, command)) {
  }

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

template <class T>
class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  explicit MockUsbDeviceHandle(MockUsbDevice<T>* device)
      : device_(device),
        remaining_body_length_(0),
        last_local_socket_(0),
        broken_(false) {
  }

  scoped_refptr<UsbDevice> GetDevice() const override {
    return device_;
  }

  void Close() override {
    device_->set_open(false);
    device_ = nullptr;
  }

  void SetConfiguration(int configuration_value,
                        ResultCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ClaimInterface(int interface_number, ResultCallback callback) override {
    bool success = false;
    if (device_->claimed_interfaces_.find(interface_number) ==
        device_->claimed_interfaces_.end()) {
      device_->claimed_interfaces_.insert(interface_number);
      success = true;
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), success));
  }

  void ReleaseInterface(int interface_number,
                        ResultCallback callback) override {
    bool success = false;
    if (device_->claimed_interfaces_.find(interface_number) ==
        device_->claimed_interfaces_.end())
      success = false;

    device_->claimed_interfaces_.erase(interface_number);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), success));
  }

  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    ResultCallback callback) override {
    NOTIMPLEMENTED();
  }

  void ResetDevice(ResultCallback callback) override { NOTIMPLEMENTED(); }

  void ClearHalt(uint8_t endpoint, ResultCallback callback) override {
    NOTIMPLEMENTED();
  }

  // Async IO. Can be called on any thread.
  void ControlTransfer(UsbTransferDirection direction,
                       device::UsbControlTransferType request_type,
                       device::UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override {}

  void GenericTransfer(UsbTransferDirection direction,
                       uint8_t endpoint,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override {
    if (direction == device::UsbTransferDirection::OUTBOUND) {
      if (remaining_body_length_ == 0) {
        std::vector<uint32_t> header(6);
        memcpy(&header[0], buffer->front(), buffer->size());
        current_message_.reset(
            new AdbMessage(header[0], header[1], header[2], std::string()));
        remaining_body_length_ = header[3];
        uint32_t magic = header[5];
        if ((current_message_->command ^ 0xffffffff) != magic) {
          DCHECK(false) << "Header checksum error";
          return;
        }
      } else {
        DCHECK(current_message_.get());
        current_message_->body +=
            std::string(buffer->front_as<char>(), buffer->size());
        remaining_body_length_ -= buffer->size();
      }

      if (remaining_body_length_ == 0) {
        ProcessIncoming();
      }

      device::UsbTransferStatus status = broken_
                                             ? UsbTransferStatus::TRANSFER_ERROR
                                             : UsbTransferStatus::COMPLETED;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), status, nullptr, 0));
      ProcessQueries();
    } else if (direction == device::UsbTransferDirection::INBOUND) {
      queries_.push(Query(std::move(callback), buffer, buffer->size()));
      ProcessQueries();
    }
  }

  const UsbInterfaceDescriptor* FindInterfaceByEndpoint(
      uint8_t endpoint_address) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  template <class D>
  void append(D data) {
    std::copy(reinterpret_cast<char*>(&data),
              (reinterpret_cast<char*>(&data)) + sizeof(D),
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
        if (T::kBreaks) {
          broken_ = true;
          return;
        }
        auto it = local_sockets_.find(current_message_->arg0);
        if (it == local_sockets_.end())
          return;

        DCHECK(current_message_->arg1 != 0);
        WriteResponse(current_message_->arg1,
                      current_message_->arg0,
                      AdbMessage::kCommandOKAY,
                      std::string());
        it->second->Receive(current_message_->body);
        break;
      }
      case AdbMessage::kCommandOPEN: {
        DCHECK(current_message_->arg1 == 0);
        DCHECK(current_message_->arg0 != 0);
        std::string response;
        WriteResponse(++last_local_socket_,
                      current_message_->arg0,
                      AdbMessage::kCommandOKAY,
                      std::string());
        local_sockets_[current_message_->arg0] =
            std::make_unique<MockLocalSocket>(
                base::Bind(&MockUsbDeviceHandle::WriteResponse,
                           base::Unretained(this), last_local_socket_,
                           current_message_->arg0),
                kDeviceSerial, current_message_->body.substr(
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
    std::copy(body.begin(), body.end(), std::back_inserter(output_buffer_));
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
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(query.callback),
                         UsbTransferStatus::TRANSFER_ERROR, nullptr, 0));
      return;
    }

    if (queries_.front().size > output_buffer_.size())
      return;

    Query query = std::move(queries_.front());
    queries_.pop();
    std::copy(output_buffer_.begin(), output_buffer_.begin() + query.size,
              query.buffer->front());
    output_buffer_.erase(output_buffer_.begin(),
                         output_buffer_.begin() + query.size);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(query.callback), UsbTransferStatus::COMPLETED,
                       query.buffer, query.size));
  }

  void IsochronousTransferIn(uint8_t endpoint_number,
                             const std::vector<uint32_t>& packet_lengths,
                             unsigned int timeout,
                             IsochronousTransferCallback callback) override {}

  void IsochronousTransferOut(uint8_t endpoint_number,
                              scoped_refptr<base::RefCountedBytes> buffer,
                              const std::vector<uint32_t>& packet_lengths,
                              unsigned int timeout,
                              IsochronousTransferCallback callback) override {}

 protected:
  virtual ~MockUsbDeviceHandle() {}

  struct Query {
    TransferCallback callback;
    scoped_refptr<base::RefCountedBytes> buffer;
    size_t size;

    Query(TransferCallback callback,
          scoped_refptr<base::RefCountedBytes> buffer,
          int size)
        : callback(std::move(callback)), buffer(buffer), size(size) {}
  };

  scoped_refptr<MockUsbDevice<T> > device_;
  uint32_t remaining_body_length_;
  std::unique_ptr<AdbMessage> current_message_;
  std::vector<char> output_buffer_;
  base::queue<Query> queries_;
  std::unordered_map<int, std::unique_ptr<MockLocalSocket>> local_sockets_;
  int last_local_socket_;
  bool broken_;
};

template <class T>
class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice()
      : UsbDevice(0x0200,  // usb_version
                  0,       // device_class
                  0,       // device_subclass
                  0,       // device_protocol
                  0,       // vendor_id
                  0,       // product_id
                  0x0100,  // device_version
                  base::UTF8ToUTF16(kDeviceManufacturer),
                  base::UTF8ToUTF16(kDeviceModel),
                  base::UTF8ToUTF16(kDeviceSerial)) {
    UsbConfigDescriptor config_desc(1, false, false, 0);
    UsbInterfaceDescriptor interface_desc(0, 0, T::kClass, T::kSubclass,
                                          T::kProtocol);
    interface_desc.endpoints.emplace_back(0x81, 0x02, 512, 0);
    interface_desc.endpoints.emplace_back(0x01, 0x02, 512, 0);
    config_desc.interfaces.push_back(interface_desc);
    descriptor_.configurations.push_back(config_desc);
    if (T::kConfigured)
      ActiveConfigurationChanged(1);
  }

  void Open(OpenCallback callback) override {
    // While most operating systems allow multiple applications to open a
    // device simultaneously so that they may claim separate interfaces DevTools
    // will always be trying to claim the same interface and so multiple
    // connections are more likely to cause problems. https://crbug.com/725320
    EXPECT_FALSE(open_);
    open_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::WrapRefCounted(new MockUsbDeviceHandle<T>(this))));
  }

  void set_open(bool open) { open_ = open; }

  bool open_ = false;
  std::set<int> claimed_interfaces_;

 protected:
  virtual ~MockUsbDevice() {}
};

class MockUsbServiceForCheckingTraits : public MockUsbService {
 public:
  MockUsbServiceForCheckingTraits() : step_(0) {}

  void GetDevices(const GetDevicesCallback& callback) override {
    std::vector<scoped_refptr<UsbDevice>> devices;
    // This switch should be kept in sync with
    // AndroidUsbBrowserTest::DeviceCountChanged.
    switch (step_) {
      case 0:
        // No devices.
        break;
      case 1:
        // Android device.
        devices.push_back(new MockUsbDevice<AndroidTraits>());
        break;
      case 2:
        // Android and non-android device.
        devices.push_back(new MockUsbDevice<AndroidTraits>());
        devices.push_back(new MockUsbDevice<NonAndroidTraits>());
        break;
      case 3:
        // Non-android device.
        devices.push_back(new MockUsbDevice<NonAndroidTraits>());
        break;
    }
    step_++;
    callback.Run(devices);
  }

 private:
  int step_;
};

class TestDeviceClient : public DeviceClient {
 public:
  explicit TestDeviceClient(std::unique_ptr<UsbService> service)
      : DeviceClient(), usb_service_(std::move(service)) {}
  ~TestDeviceClient() override {}

 private:
  UsbService* GetUsbService() override { return usb_service_.get(); }

  std::unique_ptr<UsbService> usb_service_;
};

class DevToolsAndroidBridgeWarmUp
    : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  DevToolsAndroidBridgeWarmUp(base::Closure closure,
                              DevToolsAndroidBridge* adb_bridge)
      : closure_(closure), adb_bridge_(adb_bridge) {}

  void DeviceCountChanged(int count) override {
    adb_bridge_->RemoveDeviceCountListener(this);
    closure_.Run();
  }

  base::Closure closure_;
  DevToolsAndroidBridge* adb_bridge_;
};

class AndroidUsbDiscoveryTest : public InProcessBrowserTest {
 protected:
  AndroidUsbDiscoveryTest()
      : scheduler_invoked_(0) {
  }

  void SetUpOnMainThread() override {
    device_client_.reset(new TestDeviceClient(CreateMockService()));
    adb_bridge_ =
        DevToolsAndroidBridge::Factory::GetForProfile(browser()->profile());
    DCHECK(adb_bridge_);
    adb_bridge_->set_task_scheduler_for_test(base::Bind(
        &AndroidUsbDiscoveryTest::ScheduleDeviceCountRequest,
        base::Unretained(this)));

    scoped_refptr<UsbDeviceProvider> provider =
        new UsbDeviceProvider(browser()->profile());

    AndroidDeviceManager::DeviceProviders providers;
    providers.push_back(provider);
    adb_bridge_->set_device_providers_for_test(providers);
    runner_ = new content::MessageLoopRunner;
  }

  void ScheduleDeviceCountRequest(const base::Closure& request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    scheduler_invoked_++;
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, request);
  }

  virtual std::unique_ptr<MockUsbService> CreateMockService() {
    std::unique_ptr<MockUsbService> service(new MockUsbService());
    service->AddDevice(new MockUsbDevice<AndroidTraits>());
    return service;
  }

  scoped_refptr<content::MessageLoopRunner> runner_;
  std::unique_ptr<TestDeviceClient> device_client_;
  DevToolsAndroidBridge* adb_bridge_;
  int scheduler_invoked_;
};

class AndroidUsbCountTest : public AndroidUsbDiscoveryTest {
 protected:
  void SetUpOnMainThread() override {
    AndroidUsbDiscoveryTest::SetUpOnMainThread();
    DevToolsAndroidBridgeWarmUp warmup(runner_->QuitClosure(), adb_bridge_);
    adb_bridge_->AddDeviceCountListener(&warmup);
    runner_->Run();
    runner_ = new content::MessageLoopRunner;
  }
};

class AndroidUsbTraitsTest : public AndroidUsbDiscoveryTest {
 protected:
  std::unique_ptr<MockUsbService> CreateMockService() override {
    return std::make_unique<MockUsbServiceForCheckingTraits>();
  }
};

class AndroidBreakingUsbTest : public AndroidUsbDiscoveryTest {
 protected:
  std::unique_ptr<MockUsbService> CreateMockService() override {
    std::unique_ptr<MockUsbService> service(new MockUsbService());
    service->AddDevice(new MockUsbDevice<BreakingAndroidTraits>());
    return service;
  }
};

class AndroidNoConfigUsbTest : public AndroidUsbDiscoveryTest {
 protected:
  std::unique_ptr<MockUsbService> CreateMockService() override {
    std::unique_ptr<MockUsbService> service(new MockUsbService());
    service->AddDevice(new MockUsbDevice<AndroidTraits>());
    service->AddDevice(new MockUsbDevice<NoConfigTraits>());
    return service;
  }
};

class MockListListener : public DevToolsAndroidBridge::DeviceListListener {
 public:
  MockListListener(DevToolsAndroidBridge* adb_bridge,
                   const base::Closure& callback)
      : adb_bridge_(adb_bridge),
        callback_(callback) {
  }

  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override {
    if (devices.size() > 0) {
      for (const auto& device : devices) {
        if (device->is_connected()) {
          ASSERT_EQ(kDeviceModel, device->model());
          ASSERT_EQ(kDeviceSerial, device->serial());
          adb_bridge_->RemoveDeviceListListener(this);
          callback_.Run();
          break;
        }
      }
    }
  }

  DevToolsAndroidBridge* adb_bridge_;
  base::Closure callback_;
};

class MockCountListener : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  explicit MockCountListener(DevToolsAndroidBridge* adb_bridge)
      : adb_bridge_(adb_bridge), invoked_(0) {}

  void DeviceCountChanged(int count) override {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    Shutdown();
  }

  void Shutdown() { base::RunLoop::QuitCurrentWhenIdleDeprecated(); }

  DevToolsAndroidBridge* adb_bridge_;
  int invoked_;
};

class MockCountListenerWithReAdd : public MockCountListener {
 public:
  explicit MockCountListenerWithReAdd(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        readd_count_(2) {
  }

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

  int readd_count_;
};

class MockCountListenerWithReAddWhileQueued : public MockCountListener {
 public:
  MockCountListenerWithReAddWhileQueued(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        readded_(false) {
  }

  void DeviceCountChanged(int count) override {
    ++invoked_;
    if (!readded_) {
      readded_ = true;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  bool readded_;
};

class MockCountListenerForCheckingTraits : public MockCountListener {
 public:
  MockCountListenerForCheckingTraits(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        step_(0) {
  }
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

  int step_;
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

// Test is flaky. See: http://crbug.com/883680
IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       DISABLED_TestNoMultipleCallsRemoveInCallback) {
  MockCountListener listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddInCallback) {
  MockCountListenerWithReAdd listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(3, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddOnStart) {
  MockCountListener listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  adb_bridge_->RemoveDeviceCountListener(&listener);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddWhileQueued) {
  MockCountListenerWithReAddWhileQueued listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(2, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
  EXPECT_TRUE(base::MessageLoopCurrent::Get()->IsIdleForTesting());
}

IN_PROC_BROWSER_TEST_F(AndroidUsbTraitsTest, TestDeviceCounting) {
  MockCountListenerForCheckingTraits listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
}
