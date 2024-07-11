// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/chrome_hid_delegate.h"

#include <memory>
#include <string_view>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/embedded_worker_instance_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "services/device/public/cpp/test/fake_hid_manager.h"
#include "services/device/public/cpp/test/hid_test_util.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace {

using ::base::test::RunClosure;
using ::base::test::TestFuture;
using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::UnorderedElementsAre;

constexpr std::string_view kDefaultTestUrl{"https://www.google.com"};
constexpr std::string_view kCrossOriginTestUrl{"https://www.chromium.org"};
constexpr char kTestUserEmail[] = "user@example.com";

#if BUILDFLAG(ENABLE_EXTENSIONS)
constexpr std::string_view kPrivilegedExtensionId{
    "ckcendljdlmgnhghiaomidhiiclmapok"};
constexpr std::string_view kExtensionDocumentFileName{"index.html"};
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

MATCHER_P(HasGuid, matcher, "") {
  return ExplainMatchResult(matcher, arg->guid, result_listener);
}

device::mojom::HidDeviceInfoPtr CreateFakeDevice() {
  auto device = device::CreateDeviceFromReportDescriptor(
      /*vendor_id=*/0x1234, /*product_id=*/0xabcd,
      device::TestReportDescriptors::JabraLink380c());

  // Ensure `serial_number` is unique.
  device->serial_number = device->guid;

  return device;
}

// Create a partially-initialized device.
device::mojom::HidDeviceInfoPtr CreateIncompleteFakeDevice() {
  auto device = CreateFakeDevice();
  EXPECT_GT(device->collections.size(), 1u);
  device->collections.pop_back();
  return device;
}

device::mojom::HidDeviceInfoPtr CreateFakeFidoDevice() {
  return device::CreateDeviceFromReportDescriptor(
      /*vendor_id=*/0x1234,
      /*product_id=*/0xabcd, device::TestReportDescriptors::FidoU2fHid());
}

// A mock HidManagerClient implementation that can be used to listen for HID
// device connection events.
class MockHidManagerClient : public HidChooserContext::HidManagerClient {
 public:
  MockHidManagerClient() = default;
  MockHidManagerClient(const MockHidManagerClient&) = delete;
  MockHidManagerClient& operator=(const MockHidManagerClient&) = delete;
  ~MockHidManagerClient() override = default;

  mojo::PendingAssociatedRemote<HidManagerClient> BindReceiverAndPassRemote() {
    auto client = receiver_.BindNewEndpointAndPassRemote();
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockHidManagerClient::OnConnectionError, base::Unretained(this)));
    return client;
  }

  MOCK_METHOD1(DeviceAdded, void(device::mojom::HidDeviceInfoPtr));
  MOCK_METHOD1(DeviceRemoved, void(device::mojom::HidDeviceInfoPtr));
  MOCK_METHOD1(DeviceChanged, void(device::mojom::HidDeviceInfoPtr));
  MOCK_METHOD0(OnHidChooserContextShutdown, void());
  MOCK_METHOD0(ConnectionError, void());
  void OnConnectionError() {
    receiver_.reset();
    ConnectionError();
  }

 private:
  mojo::AssociatedReceiver<HidManagerClient> receiver_{this};
};

// A fake HidConnectionClient implementation.
class FakeHidConnectionClient : public device::mojom::HidConnectionClient {
 public:
  FakeHidConnectionClient() = default;
  FakeHidConnectionClient(FakeHidConnectionClient&) = delete;
  FakeHidConnectionClient& operator=(FakeHidConnectionClient&) = delete;
  ~FakeHidConnectionClient() override = default;

  void Bind(
      mojo::PendingReceiver<device::mojom::HidConnectionClient> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // mojom::HidConnectionClient:
  void OnInputReport(uint8_t report_id,
                     const std::vector<uint8_t>& buffer) override {}

 private:
  mojo::Receiver<device::mojom::HidConnectionClient> receiver_{this};
};

class MockHidConnectionTracker : public HidConnectionTracker {
 public:
  explicit MockHidConnectionTracker(Profile* profile)
      : HidConnectionTracker(profile) {}
  ~MockHidConnectionTracker() override = default;

  MOCK_METHOD(void, IncrementConnectionCount, (const url::Origin&), (override));
  MOCK_METHOD(void, DecrementConnectionCount, (const url::Origin&), (override));
};

class ChromeHidTestHelper {
 public:
  void SimulateDeviceServiceCrash() {
    hid_manager_->SimulateConnectionError();
    hid_manager_.reset();
    base::RunLoop().RunUntilIdle();

    // Re-bind a new fake HidManager so tests don't create a real one.
    BindHidManager();
  }

  void AddDevice(const device::mojom::HidDeviceInfoPtr& device) {
    hid_manager_->AddDevice(device.Clone());
  }

  void ChangeDevice(const device::mojom::HidDeviceInfoPtr& device) {
    hid_manager_->ChangeDevice(device.Clone());
  }

  void RemoveDevice(const device::mojom::HidDeviceInfoPtr& device) {
    hid_manager_->RemoveDevice(device->guid);
  }

  HidChooserContext* GetChooserContext() {
    return HidChooserContextFactory::GetForProfile(profile_);
  }

  virtual void ConnectToService(
      mojo::PendingReceiver<blink::mojom::HidService> receiver) = 0;

  void BindHidManager() {
    EXPECT_FALSE(hid_manager_);
    hid_manager_ = std::make_unique<device::FakeHidManager>();
    mojo::PendingRemote<device::mojom::HidManager> pending_remote;
    hid_manager_->Bind(pending_remote.InitWithNewPipeAndPassReceiver());
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    GetChooserContext()->SetHidManagerForTesting(std::move(pending_remote),
                                                 devices_future.GetCallback());
    EXPECT_TRUE(devices_future.Wait());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* SetUpUserManager() {
    // On ChromeOS a user account is needed in order to check whether the user
    // account is affiliated with the device owner for the purposes of applying
    // enterprise policy.
    constexpr char kTestUserGaiaId[] = "1111111111";
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager_ptr = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    const user_manager::User* user = fake_user_manager_ptr->AddUser(account_id);

    fake_user_manager_ptr->LoginUser(account_id);

    return user;
  }

  void TearDownUserManager() { scoped_user_manager_.reset(); }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Creates a fake extension with the specified `extension_id` so that it can
  // exercise behaviors that are only enabled for privileged extensions.
  scoped_refptr<const extensions::Extension> CreateExtensionWithId(
      std::string_view extension_id) {
    auto manifest =
        base::Value::Dict()
            .Set("name", "Fake extension")
            .Set("description", "For testing.")
            .Set("version", "0.1")
            .Set("manifest_version", 2)
            .Set("web_accessible_resources",
                 base::Value::List().Append(kExtensionDocumentFileName));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder()
            .SetManifest(std::move(manifest))
            .SetID(std::string(extension_id))
            .Build();
    if (!extension) {
      return nullptr;
    }
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile_));
    extensions::ExtensionService* extension_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service->AddExtension(extension.get());
    return extension;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  void SetUpWebPageOriginUrl() { origin_url_ = GURL(kDefaultTestUrl); }

  void SetUpExtensionOriginUrl() {
    extension_ = CreateExtensionWithId(kPrivilegedExtensionId);
    origin_url_ = extension_->origin().GetURL();
  }

  virtual void SetUpOriginUrl() = 0;

  BrowserContextKeyedServiceFactory::TestingFactory
  GetHidConnectionTrackerTestingFactory() {
    return base::BindRepeating([](content::BrowserContext* browser_context) {
      return static_cast<std::unique_ptr<KeyedService>>(
          std::make_unique<testing::NiceMock<MockHidConnectionTracker>>(
              Profile::FromBrowserContext(browser_context)));
    });
  }

  void SetUpHidConnectionTracker() {
    // Even MockHidConnectionTracker can be lazily created in ChromeHidDelegate,
    // we intentionally create it ahead of time so that we can test EXPECT_CALL
    // for invoking mock method for the first time.
    hid_connection_tracker_ = static_cast<MockHidConnectionTracker*>(
        HidConnectionTrackerFactory::GetForProfile(profile_, /*create=*/true));
  }

  MockHidConnectionTracker& hid_connection_tracker() {
    return *hid_connection_tracker_;
  }

  MockHidManagerClient& hid_manager_client() { return hid_manager_client_; }

  void TestHidServiceNotConnected() {
    base::RunLoop run_loop;
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());
    hid_service.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(hid_service.is_connected());
  }

  void TestAddChangeRemoveDevice() {
    const auto origin = url::Origin::Create(origin_url_);
    // Connect a device with one of its collections missing.
    auto incomplete_device = CreateIncompleteFakeDevice();
    AddDevice(incomplete_device);

    // Grant permission to access `incomplete_device` from `origin`.
    GetChooserContext()->GrantDevicePermission(origin, *incomplete_device);

    // Create the HidService and register a mock client to receive
    // notifications on device connections and disconnections.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());
    hid_service->RegisterClient(
        hid_manager_client().BindReceiverAndPassRemote());

    // Call GetDevices to ensure the service is started and the client is set.
    {
      TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
      hid_service->GetDevices(devices_future.GetCallback());
      ASSERT_THAT(devices_future.Get(),
                  ElementsAre(HasGuid(incomplete_device->guid)));
      EXPECT_EQ(devices_future.Get().front()->collections.size(),
                incomplete_device->collections.size());
    }

    // Update the device with the missing collection.
    auto complete_device = CreateFakeDevice();
    complete_device->guid = incomplete_device->guid;
    complete_device->serial_number = incomplete_device->serial_number;
    TestFuture<device::mojom::HidDeviceInfoPtr> device_changed_future;
    EXPECT_CALL(hid_manager_client(), DeviceChanged).WillOnce([&](auto d) {
      device_changed_future.SetValue(std::move(d));
    });
    ChangeDevice(complete_device);
    EXPECT_EQ(device_changed_future.Get()->guid, complete_device->guid);

    // Call GetDevices and make sure there is still only one device with the
    // same `guid` but the complete device info is returned.
    {
      TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
      hid_service->GetDevices(devices_future.GetCallback());
      ASSERT_THAT(devices_future.Get(),
                  ElementsAre(HasGuid(incomplete_device->guid)));
      EXPECT_EQ(devices_future.Get().front()->collections.size(),
                complete_device->collections.size());
    }

    // Disconnect the devices. The mock client should be notified.
    TestFuture<device::mojom::HidDeviceInfoPtr> device_removed_future;
    EXPECT_CALL(hid_manager_client(), DeviceRemoved).WillOnce([&](auto d) {
      device_removed_future.SetValue(std::move(d));
    });
    RemoveDevice(incomplete_device);
    EXPECT_EQ(device_removed_future.Get()->guid, incomplete_device->guid);

    // Reconnect the device. The mock client should be notified.
    TestFuture<device::mojom::HidDeviceInfoPtr> device_added_future;
    EXPECT_CALL(hid_manager_client(), DeviceAdded).WillOnce([&](auto d) {
      device_added_future.SetValue(std::move(d));
    });
    AddDevice(complete_device);
    EXPECT_EQ(device_added_future.Get()->guid, incomplete_device->guid);
  }

  void TestNoPermissionDevice() {
    const auto origin = url::Origin::Create(origin_url_);

    // Connect two devices.
    auto allowed_device1 = CreateFakeDevice();
    AddDevice(allowed_device1);
    auto other_device1 = CreateFakeDevice();
    AddDevice(other_device1);

    // Grant permission to access `allowed_device1` from `origin`.
    GetChooserContext()->GrantDevicePermission(origin, *allowed_device1);

    // Create the HidService and register a mock client to receive
    // notifications on device connections and disconnections.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());
    hid_service->RegisterClient(
        hid_manager_client().BindReceiverAndPassRemote());

    // Call GetDevices to ensure the service is started and the client is set.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_THAT(devices_future.Get(),
                ElementsAre(HasGuid(allowed_device1->guid)));

    // Connect two more devices.
    auto allowed_device2 = CreateFakeDevice();
    AddDevice(allowed_device2);
    auto other_device2 = CreateFakeDevice();
    AddDevice(other_device2);

    // Grant permission to access `allowed_device2` from `origin`.
    GetChooserContext()->GrantDevicePermission(origin, *allowed_device2);

    // Disconnect all four devices. The mock client should be notified only
    // for the devices it has permission to access.
    TestFuture<device::mojom::HidDeviceInfoPtr> device_removed_future;
    EXPECT_CALL(hid_manager_client(), DeviceRemoved)
        .Times(2)
        .WillRepeatedly(
            [&](auto d) { device_removed_future.SetValue(std::move(d)); });
    RemoveDevice(allowed_device1);
    RemoveDevice(allowed_device2);
    RemoveDevice(other_device1);
    RemoveDevice(other_device2);
    EXPECT_EQ(device_removed_future.Take()->guid, allowed_device1->guid);
    EXPECT_EQ(device_removed_future.Take()->guid, allowed_device2->guid);

    // Reconnect all four devices. The mock client should be notified only for
    // the devices it has permission to access.
    TestFuture<device::mojom::HidDeviceInfoPtr> device_added_future;
    EXPECT_CALL(hid_manager_client(), DeviceAdded)
        .Times(2)
        .WillRepeatedly(
            [&](auto d) { device_added_future.SetValue(std::move(d)); });
    AddDevice(allowed_device1);
    AddDevice(allowed_device2);
    AddDevice(other_device1);
    AddDevice(other_device2);
    EXPECT_EQ(device_added_future.Take()->guid, allowed_device1->guid);
    EXPECT_EQ(device_added_future.Take()->guid, allowed_device2->guid);
  }

  void TestReconnectHidService() {
    const auto origin = url::Origin::Create(origin_url_);

    // Connect two devices. Configure `ephemeral_device` with no serial number
    // so it is not eligible for persistent permissions.
    auto device = CreateFakeDevice();
    auto ephemeral_device = CreateFakeDevice();
    ephemeral_device->serial_number = "";
    AddDevice(device);
    AddDevice(ephemeral_device);

    // Grant permission for `origin` to access both devices. `device` is
    // eligible for persistent permissions and `ephemeral_device` is only
    // eligible for ephemeral permissions.
    GetChooserContext()->GrantDevicePermission(origin, *device);
    GetChooserContext()->GrantDevicePermission(origin, *ephemeral_device);

    // Create the HidService and register a mock client to receive
    // notifications on device connections and disconnections. Call `GetDevices`
    // to ensure the service is started and the client is set.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());
    hid_service->RegisterClient(
        hid_manager_client().BindReceiverAndPassRemote());
    {
      TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
      hid_service->GetDevices(devices_future.GetCallback());
      EXPECT_THAT(devices_future.Get(),
                  UnorderedElementsAre(HasGuid(device->guid),
                                       HasGuid(ephemeral_device->guid)));
    }

    // Both permissions are granted.
    EXPECT_TRUE(GetChooserContext()->HasDevicePermission(origin, *device));
    EXPECT_TRUE(
        GetChooserContext()->HasDevicePermission(origin, *ephemeral_device));

    // Simulate a device service crash.
    base::RunLoop loop;
    EXPECT_CALL(hid_manager_client(), ConnectionError).WillOnce([&]() {
      loop.Quit();
    });
    SimulateDeviceServiceCrash();
    loop.Run();

    // The ephemeral permission is revoked.
    EXPECT_TRUE(GetChooserContext()->HasDevicePermission(origin, *device));
    EXPECT_FALSE(
        GetChooserContext()->HasDevicePermission(origin, *ephemeral_device));

    // Add a new device eligible for persistent permissions.
    auto another_device = CreateFakeDevice();
    AddDevice(another_device);
    EXPECT_CALL(hid_manager_client(), DeviceAdded).Times(0);
    base::RunLoop().RunUntilIdle();

    // Grant the device permission while the service is off.
    GetChooserContext()->GrantDevicePermission(origin, *another_device);

    // `mock_client` is not notified when `device` is removed because the
    // service is off.
    RemoveDevice(device);
    EXPECT_CALL(hid_manager_client(), DeviceRemoved).Times(0);
    base::RunLoop().RunUntilIdle();

    // Reconnect the service.
    hid_service.reset();
    testing::Mock::VerifyAndClearExpectations(&hid_manager_client());
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());
    hid_service->RegisterClient(
        hid_manager_client().BindReceiverAndPassRemote());
    {
      TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
      hid_service->GetDevices(devices_future.GetCallback());
      EXPECT_THAT(devices_future.Get(),
                  ElementsAre(HasGuid(another_device->guid)));
    }

    // The persistent permissions are still granted.
    EXPECT_TRUE(GetChooserContext()->HasDevicePermission(origin, *device));
    EXPECT_TRUE(
        GetChooserContext()->HasDevicePermission(origin, *another_device));
    EXPECT_FALSE(
        GetChooserContext()->HasDevicePermission(origin, *ephemeral_device));
  }

  void TestRevokeDevicePermission() {
    const auto origin = url::Origin::Create(origin_url_);

    // Connect a device.
    auto device = CreateFakeDevice();
    AddDevice(device);

    // Create the `HidService`.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());

    // Call GetDevices to ensure the service is started.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_TRUE(devices_future.Get().empty());

    // Grant permission to access the connected device.
    GetChooserContext()->GrantDevicePermission(origin, *device);
    auto objects = GetChooserContext()->GetGrantedObjects(origin);
    ASSERT_EQ(1u, objects.size());

    // Open a connection to `device`.
    FakeHidConnectionClient connection_client;
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    if (supports_hid_connection_tracker_) {
      EXPECT_CALL(hid_connection_tracker(), IncrementConnectionCount(origin));
    }
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    ASSERT_TRUE(connection);

    // Revoke the permission. The device should be disconnected.
    base::RunLoop disconnect_loop;
    connection.set_disconnect_handler(disconnect_loop.QuitClosure());

    base::RunLoop decrement_connection_count_loop;
    if (supports_hid_connection_tracker_) {
      EXPECT_CALL(hid_connection_tracker(), DecrementConnectionCount(origin))
          .WillOnce(RunClosure(decrement_connection_count_loop.QuitClosure()));
    }

    GetChooserContext()->RevokeDevicePermission(origin, *device);
    disconnect_loop.Run();
    if (supports_hid_connection_tracker_) {
      decrement_connection_count_loop.Run();
    }
  }

  void TestRevokeDevicePermissionEphemeral() {
    const auto origin = url::Origin::Create(origin_url_);

    // Connect a device. Configure it with no serial number so it is not
    // eligible for persistent permissions.
    auto device = CreateFakeDevice();
    device->serial_number = "";
    AddDevice(device);

    // Create the `HidService`.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());

    // Call GetDevices to ensure the service is started.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_TRUE(devices_future.Get().empty());

    // Grant permission to access the connected device.
    GetChooserContext()->GrantDevicePermission(origin, *device);
    auto objects = GetChooserContext()->GetGrantedObjects(origin);
    ASSERT_EQ(1u, objects.size());

    // Open a connection to `device`.
    FakeHidConnectionClient connection_client;
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    if (supports_hid_connection_tracker_) {
      EXPECT_CALL(hid_connection_tracker(), IncrementConnectionCount(origin));
    }
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    ASSERT_TRUE(connection);

    // Revoke the permission. The device should be disconnected.
    base::RunLoop disconnect_loop;
    connection.set_disconnect_handler(disconnect_loop.QuitClosure());

    base::RunLoop decrement_connection_count_loop;
    if (supports_hid_connection_tracker_) {
      EXPECT_CALL(hid_connection_tracker(), DecrementConnectionCount(origin))
          .WillOnce(RunClosure(decrement_connection_count_loop.QuitClosure()));
    }

    GetChooserContext()->RevokeDevicePermission(origin, *device);
    disconnect_loop.Run();
    if (supports_hid_connection_tracker_) {
      decrement_connection_count_loop.Run();
    }
  }

  void TestConnectAndDisconnect(content::WebContents* web_contents) {
    const auto origin = url::Origin::Create(origin_url_);

    // Create the `HidService`.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());

    // Connect a device.
    auto device = CreateFakeDevice();
    AddDevice(device);

    // Grant permission to access `device` from `origin`.
    GetChooserContext()->GrantDevicePermission(origin, *device);

    // Call `GetDevices` and expect the device to be returned.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_THAT(devices_future.Take(), ElementsAre(HasGuid(device->guid)));

    if (web_contents) {
      // The `WebContents` should not indicate we are connected to a device.
      EXPECT_FALSE(web_contents->IsConnectedToHidDevice());
    }

    // Open a connection to `device`.
    FakeHidConnectionClient connection_client;
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    if (supports_hid_connection_tracker_) {
      EXPECT_CALL(hid_connection_tracker(), IncrementConnectionCount(origin));
    }
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    ASSERT_TRUE(connection);

    if (web_contents) {
      // Now the `WebContents` should indicate we are connected to a device.
      EXPECT_TRUE(web_contents->IsConnectedToHidDevice());
    }

    // Close `connection` and check that the `WebContents` no longer indicates
    // we are connected.
    // If the scenario supports HidConnectionTracker, we can avoid using
    // RunUntilIdle() by using HidConnectionTracker::DecrementConnectionCount()
    // as it will be called in the disconnect path.
    if (supports_hid_connection_tracker_) {
      base::RunLoop decrement_connection_count_loop;
      EXPECT_CALL(hid_connection_tracker(), DecrementConnectionCount(origin))
          .WillOnce(RunClosure(decrement_connection_count_loop.QuitClosure()));
      connection.reset();
      decrement_connection_count_loop.Run();
    } else {
      connection.reset();
      base::RunLoop().RunUntilIdle();
    }

    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToHidDevice());
    }
  }

  void TestConnectAndRemove(content::WebContents* web_contents) {
    const auto origin = url::Origin::Create(origin_url_);

    // Create the `HidService`.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());
    hid_service->RegisterClient(
        hid_manager_client().BindReceiverAndPassRemote());

    // Connect a device.
    auto device = CreateFakeDevice();
    AddDevice(device);

    // Grant permission to access `device` from `origin`.
    GetChooserContext()->GrantDevicePermission(origin, *device);

    // Call `GetDevices` and expect the device to be returned.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_THAT(devices_future.Take(), ElementsAre(HasGuid(device->guid)));

    if (web_contents) {
      // The `WebContents` should not indicate we are connected to a device.
      EXPECT_FALSE(web_contents->IsConnectedToHidDevice());
    }

    // Open a connection to `device`.
    FakeHidConnectionClient connection_client;
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    if (supports_hid_connection_tracker_) {
      EXPECT_CALL(hid_connection_tracker(), IncrementConnectionCount(origin));
    }
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    ASSERT_TRUE(connection);

    if (web_contents) {
      // Now the `WebContents` should indicate we are connected to a device.
      EXPECT_TRUE(web_contents->IsConnectedToHidDevice());
    }

    // Remove `device` and check that the `WebContents` no longer indicates we
    // are connected.
    // If the scenario supports HidConnectionTracker, we can avoid using
    // RunUntilIdle() by using HidConnectionTracker::DecrementConnectionCount()
    // as it will be called in the remove device path.
    if (supports_hid_connection_tracker_) {
      base::RunLoop decrement_connection_count_loop;
      EXPECT_CALL(hid_connection_tracker(), DecrementConnectionCount(origin))
          .WillOnce(RunClosure(decrement_connection_count_loop.QuitClosure()));
      RemoveDevice(device);
      decrement_connection_count_loop.Run();
    } else {
      RemoveDevice(device);
      base::RunLoop().RunUntilIdle();
    }

    if (web_contents) {
      EXPECT_FALSE(web_contents->IsConnectedToHidDevice());
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void TestFidoDeviceAllowedWithPrivilegedOrigin(
      content::WebContents* web_contents) {
    auto origin = url::Origin::Create(origin_url_);
    // Connect a FIDO device.
    auto device = CreateFakeFidoDevice();
    AddDevice(device);

    // Grant permission to access `device` from `privileged_origin`.
    GetChooserContext()->GrantDevicePermission(origin, *device);

    // Create the `HidService`.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());

    // Call `GetDevices` and expect the device to be returned.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_THAT(devices_future.Take(), ElementsAre(HasGuid(device->guid)));

    if (web_contents) {
      // The `WebContents` should not indicate we are connected to a device.
      EXPECT_FALSE(web_contents->IsConnectedToHidDevice());
    }

    // Open a connection to `device`.
    FakeHidConnectionClient connection_client;
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    ASSERT_TRUE(connection);

    if (web_contents) {
      // Now the `WebContents` should indicate we are connected to a device.
      EXPECT_TRUE(web_contents->IsConnectedToHidDevice());
    }
  }
#endif

  void TestConnectionTrackerOpenDeviceNoConnectionCountUpdate() {
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());
    auto origin = url::Origin::Create(origin_url_);
    auto device = CreateFakeDevice();
    AddDevice(device);
    GetChooserContext()->GrantDevicePermission(origin, *device);

    // Call |GetDevices| and expect the device to be returned.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_THAT(devices_future.Take(), ElementsAre(HasGuid(device->guid)));

    EXPECT_CALL(hid_connection_tracker(), IncrementConnectionCount(origin))
        .Times(0);
    // Open a connection to `device`.
    FakeHidConnectionClient connection_client;
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    ASSERT_TRUE(connection);
  }

 protected:
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
  GURL origin_url_;
  raw_ptr<MockHidConnectionTracker, DanglingUntriaged> hid_connection_tracker_ =
      nullptr;
  // This flag is expected to be set to true only for the scenario of extension
  // origin and kEnableWebHidOnExtensionServiceWorker enabled.
  bool supports_hid_connection_tracker_ = false;

 private:
  std::unique_ptr<device::FakeHidManager> hid_manager_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
  scoped_refptr<const extensions::Extension> extension_;
  MockHidManagerClient hid_manager_client_;
};

class ChromeHidDelegateRenderFrameTestBase
    : public ChromeRenderViewHostTestHarness,
      public ChromeHidTestHelper {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const user_manager::User* user = SetUpUserManager();
    TestingProfile::Builder builder;
    testing_profile_ = builder.Build();
    profile_ = testing_profile_.get();
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());
#else
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    // TODO(crbug.com/40249783): Pass testing factory when creating profile.
    // Ideally, we should be able to pass testing factory when calling profile
    // manager's CreateTestingProfile. However, due to the fact that:
    // 1) TestingProfile::TestingProfile(...) will call BrowserContextShutdown
    // as part of setting testing factory.
    // 2) HidConnectionTrackerFactory::BrowserContextShutdown() at some point
    // need valid profile_metrics::GetBrowserProfileType() as part of
    // HidConnectionTrackerFactory::GetForProfile().
    // It will hit failure in profile_metrics::GetBrowserProfileType() because
    // the profile is not initialized properly before setting testing factory.
    // As a result, here create a profile then call SetTestingFactory to inject
    // MockHidConnectionTracker.
    profile_ = profile_manager_->CreateTestingProfile(kTestUserEmail);
#endif
    HidConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile_, GetHidConnectionTrackerTestingFactory());

    ASSERT_TRUE(profile_);
    SetUpHidConnectionTracker();
    // Create a new web contents for `profile_`.
    SetContents(
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr));
    BindHidManager();
    SetUpOriginUrl();
    NavigateAndCommit(origin_url_);
  }

  void TearDown() override {
    DeleteContents();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    testing_profile_.reset();
    TearDownUserManager();
#else
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
#endif
    profile_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
    hid_connection_tracker_ = nullptr;
  }

  // ChromeHidTestHelper
  void ConnectToService(
      mojo::PendingReceiver<blink::mojom::HidService> receiver) override {
    content::RenderFrameHostTester::For(main_rfh())
        ->CreateHidServiceForTesting(std::move(receiver));
  }

  void TestConnectAndNavigateCrossDocument(content::WebContents* web_contents) {
    // The test assumes the previous page gets deleted after navigation,
    // disconnecting the device. Disable back/forward cache to ensure that it
    // doesn't get preserved in the cache.
    // TODO(crbug.com/40232335): Integrate WebHID with bfcache and remove this.
    content::DisableBackForwardCacheForTesting(
        web_contents, content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

    const auto origin = url::Origin::Create(origin_url_);

    // Create the `HidService`.
    mojo::Remote<blink::mojom::HidService> hid_service;
    ConnectToService(hid_service.BindNewPipeAndPassReceiver());

    // Connect a device.
    auto device = CreateFakeDevice();
    AddDevice(device);

    // Grant permission to access `device` from `origin`.
    GetChooserContext()->GrantDevicePermission(origin, *device);

    // Call `GetDevices` and expect the device to be returned.
    TestFuture<std::vector<device::mojom::HidDeviceInfoPtr>> devices_future;
    hid_service->GetDevices(devices_future.GetCallback());
    EXPECT_THAT(devices_future.Take(), ElementsAre(HasGuid(device->guid)));

    // The `WebContents` should not indicate we are connected to a device.
    EXPECT_FALSE(web_contents->IsConnectedToHidDevice());

    // Open a connection to `device`.
    FakeHidConnectionClient connection_client;
    mojo::PendingRemote<device::mojom::HidConnectionClient>
        hid_connection_client;
    connection_client.Bind(
        hid_connection_client.InitWithNewPipeAndPassReceiver());
    TestFuture<mojo::PendingRemote<device::mojom::HidConnection>>
        pending_remote_future;
    hid_service->Connect(device->guid, std::move(hid_connection_client),
                         pending_remote_future.GetCallback());
    mojo::Remote<device::mojom::HidConnection> connection;
    connection.Bind(pending_remote_future.Take());
    ASSERT_TRUE(connection);

    // Now the `WebContents` should indicate we are connected to a device.
    EXPECT_TRUE(web_contents->IsConnectedToHidDevice());

    // Perform a cross-document navigation. The `WebContents` should no longer
    // indicate we are connected.
    NavigateAndCommit(GURL(kCrossOriginTestUrl));
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(web_contents->IsConnectedToHidDevice());
  }

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> testing_profile_;
#else
  std::unique_ptr<TestingProfileManager> profile_manager_;
#endif
};

class ChromeHidDelegateRenderFrameTest
    : public ChromeHidDelegateRenderFrameTestBase {
  // ChromeHidTestHelper
  void SetUpOriginUrl() override { SetUpWebPageOriginUrl(); }
};

class ChromeHidDelegateServiceWorkerTestBase
    : public content::EmbeddedWorkerInstanceTestHarness,
      public ChromeHidTestHelper {
 public:
  void SetUp() override {
    content::EmbeddedWorkerInstanceTestHarness::SetUp();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SetUpUserManager();
#endif
    SetUpHidConnectionTracker();
    BindHidManager();
    SetUpOriginUrl();
    StartWorker();
  }

  void TearDown() override {
    StopWorker();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    TearDownUserManager();
#endif
    content::EmbeddedWorkerInstanceTestHarness::TearDown();
  }

  // ChromeHidTestHelper
  void ConnectToService(
      mojo::PendingReceiver<blink::mojom::HidService> receiver) override {
    BindHidServiceToWorker(origin_url_, std::move(receiver));
  }

  void StartWorker() {
    auto worker_url =
        GURL(base::StringPrintf("%s/worker.js", origin_url_.spec().c_str()));
    CreateAndStartWorker(origin_url_, worker_url);

    // Wait until tasks triggered by ServiceWorkerHidDelegateObserver settle.
    base::RunLoop().RunUntilIdle();
  }

  void StopWorker() { StopAndResetWorker(); }

  // content::EmbeddedWorkerInstanceTestHarness
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    auto builder = TestingProfile::Builder();
    auto testing_profile = builder.Build();
    profile_ = testing_profile.get();
    // TODO(crbug.com/40249783): Pass testing factory when creating profile.
    // Ideally, we should use TestingProfile::Builder::AddTestingFactory to
    // inject MockHidConnectionTracker. However, due to the fact that:
    // 1) TestingProfile::TestingProfile(...) will call BrowserContextShutdown
    //    as part of setting testing factory.
    // 2) HidConnectionTrackerFactory::BrowserContextShutdown() at some point
    //    need valid profile_metrics::GetBrowserProfileType() as part of
    //    HidConnectionTrackerFactory::GetForProfile().
    // It will hit failure in profile_metrics::GetBrowserProfileType() due to
    // profile is not initialized properly before setting testing factory. As a
    // result, here create a profile then call SetTestingFactory to inject
    // MockHidConnectionTracker.
    HidConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile_, GetHidConnectionTrackerTestingFactory());
    return testing_profile;
  }

 private:
  ScopedTestingLocalState testing_local_state_{
      TestingBrowserProcess::GetGlobal()};
};

class ChromeHidDelegateServiceWorkerTest
    : public ChromeHidDelegateServiceWorkerTestBase {
 public:
  // ChromeHidTestHelper
  void SetUpOriginUrl() override { SetUpWebPageOriginUrl(); }
};

#if BUILDFLAG(ENABLE_EXTENSIONS)

class ChromeHidDelegateExtensionServiceWorkerTest
    : public ChromeHidDelegateServiceWorkerTestBase {
 public:
  ChromeHidDelegateExtensionServiceWorkerTest() {
    supports_hid_connection_tracker_ = true;
  }
  // ChromeHidTestHelper
  void SetUpOriginUrl() override { SetUpExtensionOriginUrl(); }
};

class ChromeHidDelegateExtensionRenderFrameTest
    : public ChromeHidDelegateRenderFrameTestBase {
 public:
  ChromeHidDelegateExtensionRenderFrameTest() {
    supports_hid_connection_tracker_ = true;
  }
  // ChromeHidTestHelper
  void SetUpOriginUrl() override { SetUpExtensionOriginUrl(); }
};
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

TEST_F(ChromeHidDelegateRenderFrameTest, AddChangeRemoveDevice) {
  TestAddChangeRemoveDevice();
}

TEST_F(ChromeHidDelegateRenderFrameTest, NoPermissionDevice) {
  TestNoPermissionDevice();
}

TEST_F(ChromeHidDelegateRenderFrameTest, ReconnectHidService) {
  TestReconnectHidService();
}

TEST_F(ChromeHidDelegateRenderFrameTest, RevokeDevicePermission) {
  TestRevokeDevicePermission();
}

TEST_F(ChromeHidDelegateRenderFrameTest, RevokeDevicePermissionEphemeral) {
  TestRevokeDevicePermissionEphemeral();
  ;
}

TEST_F(ChromeHidDelegateRenderFrameTest, ConnectAndDisconnect) {
  TestConnectAndDisconnect(web_contents());
}

TEST_F(ChromeHidDelegateRenderFrameTest, ConnectAndRemove) {
  TestConnectAndRemove(web_contents());
}

TEST_F(ChromeHidDelegateRenderFrameTest, ConnectAndNavigateCrossDocument) {
  TestConnectAndNavigateCrossDocument(web_contents());
}

TEST_F(ChromeHidDelegateServiceWorkerTest, HidServiceNotConnected) {
  TestHidServiceNotConnected();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeHidDelegateExtensionRenderFrameTest,
       FidoDeviceAllowedWithPrivilegedOrigin) {
  TestFidoDeviceAllowedWithPrivilegedOrigin(web_contents());
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest, AddChangeRemoveDevice) {
  TestAddChangeRemoveDevice();
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest, NoPermissionDevice) {
  TestNoPermissionDevice();
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest, ReconnectHidService) {
  TestReconnectHidService();
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest, RevokeDevicePermission) {
  TestRevokeDevicePermission();
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest,
       RevokeDevicePermissionEphemeral) {
  TestRevokeDevicePermissionEphemeral();
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest, ConnectAndDisconnect) {
  TestConnectAndDisconnect(web_contents());
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest, ConnectAndRemove) {
  TestConnectAndRemove(web_contents());
}

TEST_F(ChromeHidDelegateExtensionRenderFrameTest,
       ConnectAndNavigateCrossDocument) {
  TestConnectAndNavigateCrossDocument(web_contents());
}

TEST_F(ChromeHidDelegateExtensionServiceWorkerTest, AddChangeRemoveDevice) {
  TestAddChangeRemoveDevice();
}

TEST_F(ChromeHidDelegateExtensionServiceWorkerTest, NoPermissionDevice) {
  TestNoPermissionDevice();
}

TEST_F(ChromeHidDelegateExtensionServiceWorkerTest, ReconnectHidService) {
  TestReconnectHidService();
}

TEST_F(ChromeHidDelegateExtensionServiceWorkerTest, RevokeDevicePermission) {
  TestRevokeDevicePermission();
}

TEST_F(ChromeHidDelegateExtensionServiceWorkerTest,
       RevokeDevicePermissionEphemeral) {
  TestRevokeDevicePermissionEphemeral();
}

TEST_F(ChromeHidDelegateExtensionServiceWorkerTest, ConnectAndDisconnect) {
  TestConnectAndDisconnect(/*web_contents=*/nullptr);
}

TEST_F(ChromeHidDelegateExtensionServiceWorkerTest, ConnectAndRemove) {
  TestConnectAndRemove(/*web_contents=*/nullptr);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

TEST(ChromeHidDelegateBrowserContextTest, BrowserContextIsNull) {
  ChromeHidDelegate chrome_hid_delegate;
  url::Origin origin = url::Origin::Create(GURL(kDefaultTestUrl));
  EXPECT_FALSE(chrome_hid_delegate.CanRequestDevicePermission(
      /*browser_context=*/nullptr, origin));
  EXPECT_FALSE(chrome_hid_delegate.HasDevicePermission(
      /*browser_context=*/nullptr, /*render_frame_host=*/nullptr, origin,
      device::mojom::HidDeviceInfo()));
  EXPECT_EQ(nullptr,
            chrome_hid_delegate.GetHidManager(/*browser_context=*/nullptr));
  EXPECT_EQ(nullptr, chrome_hid_delegate.GetDeviceInfo(
                         /*browser_context=*/nullptr,
                         base::Uuid::GenerateRandomV4().AsLowercaseString()));
  EXPECT_FALSE(chrome_hid_delegate.IsFidoAllowedForOrigin(
      /*browser_context=*/nullptr, origin));
}
