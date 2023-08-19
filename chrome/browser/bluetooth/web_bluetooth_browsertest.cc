// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains browsertests for Web Bluetooth that depend on behavior
// defined in chrome/, not just in content/.

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate_impl_client.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chooser_bubble_testapi.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/bluetooth_delegate_impl.h"
#include "components/permissions/contexts/bluetooth_chooser_context.h"
#include "components/permissions/permission_context_base.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/bluetooth_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothGattCharacteristic;
using ::device::BluetoothGattNotifySession;
using ::device::BluetoothGattService;
using ::device::BluetoothRemoteGattCharacteristic;
using ::device::BluetoothRemoteGattService;
using ::device::BluetoothUUID;
using ::device::MockBluetoothGattCharacteristic;
using ::device::MockBluetoothGattNotifySession;
using ::device::MockBluetoothGattService;

constexpr char kDeviceAddress[] = "00:00:00:00:00:00";
constexpr char kDeviceAddress2[] = "00:00:00:00:00:01";
constexpr char kHeartRateUUIDString[] = "0000180d-0000-1000-8000-00805f9b34fb";
constexpr char kHeartRateMeasurementUUIDString[] =
    "00001234-0000-1000-8000-00805f9b34fb";

const device::BluetoothUUID kHeartRateUUID(kHeartRateUUIDString);
const device::BluetoothUUID kHeartRateMeasurementUUID(
    kHeartRateMeasurementUUIDString);

constexpr char kExampleUrl[] = "https://example.com";

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  void SetIsPresent(bool is_present) { is_present_ = is_present; }

  void SimulateDeviceAdvertisementReceived(
      const std::string& device_address,
      const absl::optional<std::string>& advertisement_name =
          absl::nullopt) const {
    for (auto& observer : observers_) {
      observer.DeviceAdvertisementReceived(
          device_address, /*device_name=*/absl::nullopt, advertisement_name,
          /*rssi=*/absl::nullopt, /*tx_power=*/absl::nullopt,
          /*appearance=*/absl::nullopt,
          /*advertised_uuids=*/{}, /*service_data_map=*/{},
          /*manufacturer_data_map=*/{});
    }
  }

  // device::BluetoothAdapter implementation:
  void AddObserver(device::BluetoothAdapter::Observer* observer) override {
    device::BluetoothAdapter::AddObserver(observer);
  }

  bool IsPresent() const override { return is_present_; }

  bool IsPowered() const override { return true; }

  device::BluetoothAdapter::ConstDeviceList GetDevices() const override {
    device::BluetoothAdapter::ConstDeviceList devices;
    for (const auto& it : mock_devices_)
      devices.push_back(it.get());
    return devices;
  }

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void StartScanWithFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> filter,
      base::OnceCallback<void(/*is_error*/ bool,
                              device::UMABluetoothDiscoverySessionOutcome)>
          callback) override {
    std::move(callback).Run(
        /*is_error=*/false,
        device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
  }

 protected:
  ~FakeBluetoothAdapter() override = default;

  bool is_present_ = true;
};

class FakeBluetoothGattCharacteristic
    : public testing::NiceMock<MockBluetoothGattCharacteristic> {
 public:
  FakeBluetoothGattCharacteristic(MockBluetoothGattService* service,
                                  const std::string& identifier,
                                  const BluetoothUUID& uuid,
                                  Properties properties,
                                  Permissions permissions)
      : testing::NiceMock<MockBluetoothGattCharacteristic>(service,
                                                           identifier,
                                                           uuid,
                                                           properties,
                                                           permissions),
        value_({1}) {}

  // Move-only class
  FakeBluetoothGattCharacteristic(const FakeBluetoothGattCharacteristic&) =
      delete;
  FakeBluetoothGattCharacteristic operator=(
      const FakeBluetoothGattCharacteristic&) = delete;

  void ReadRemoteCharacteristic(ValueCallback callback) override {
    if (!(GetProperties() & BluetoothGattCharacteristic::PROPERTY_READ)) {
      std::move(callback).Run(
          BluetoothGattService::GattErrorCode::kNotPermitted,
          std::vector<uint8_t>());
      return;
    }
    if (defer_read_until_notification_start_) {
      DCHECK(!deferred_read_callback_);
      deferred_read_callback_ = std::move(callback);
      return;
    }
    std::move(callback).Run(/*error_code=*/absl::nullopt, value_);
  }

  void StartNotifySession(NotifySessionCallback callback,
                          ErrorCallback error_callback) override {
    if (!(GetProperties() & BluetoothGattCharacteristic::PROPERTY_NOTIFY)) {
      std::move(error_callback)
          .Run(BluetoothGattService::GattErrorCode::kNotPermitted);
      return;
    }
    auto fake_notify_session =
        std::make_unique<testing::NiceMock<MockBluetoothGattNotifySession>>(
            GetWeakPtr());
    active_notify_sessions_.insert(fake_notify_session->unique_id());

    if (deferred_read_callback_) {
      // A new value as a result of calling readValue().
      std::move(deferred_read_callback_)
          .Run(/*error_code=*/absl::nullopt, value_);
    }

    if (emit_value_change_at_notification_start_) {
      BluetoothAdapter* adapter = GetService()->GetDevice()->GetAdapter();
      adapter->NotifyGattCharacteristicValueChanged(this, value_);

      // NotifyGattCharacteristicValueChanged(...) posts a task to notify the
      // renderer of the change. Do the same for |callback| to ensure
      // StartNotifySession completes after the value change notification is
      // received.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), std::move(fake_notify_session)));
    } else {
      // Complete StartNotifySession normally.
      std::move(callback).Run(std::move(fake_notify_session));
    }
    EXPECT_TRUE(IsNotifying());
  }

  void StopNotifySession(BluetoothGattNotifySession::Id session,
                         base::OnceClosure callback) override {
    EXPECT_TRUE(base::Contains(active_notify_sessions_, session));
    std::move(callback).Run();
  }

  bool IsNotifying() const override { return !active_notify_sessions_.empty(); }

  // Do not call the readValue callback until midway through the completion
  // of the startNotification callback registration.
  // https://crbug.com/1153426
  void DeferReadUntilNotificationStart() {
    defer_read_until_notification_start_ = true;
  }

  // Possibly trigger value characteristicvaluechanged events on the page
  // during the setup of startNotifications.
  // https://crbug.com/1153426.
  void EmitChangeNotificationAtNotificationStart() {
    emit_value_change_at_notification_start_ = true;
  }

 private:
  std::vector<uint8_t> value_;
  ValueCallback deferred_read_callback_;
  bool defer_read_until_notification_start_ = false;
  bool emit_value_change_at_notification_start_ = false;
  std::set<BluetoothGattNotifySession::Id> active_notify_sessions_;
};

class FakeBluetoothGattConnection
    : public testing::NiceMock<device::MockBluetoothGattConnection> {
 public:
  FakeBluetoothGattConnection(scoped_refptr<device::BluetoothAdapter> adapter,
                              const std::string& device_address)
      : testing::NiceMock<device::MockBluetoothGattConnection>(adapter,
                                                               device_address) {
  }

  // Move-only class
  FakeBluetoothGattConnection(const FakeBluetoothGattConnection&) = delete;
  FakeBluetoothGattConnection operator=(const FakeBluetoothGattConnection&) =
      delete;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(device::MockBluetoothAdapter* adapter,
                      const std::string& address)
      : testing::NiceMock<device::MockBluetoothDevice>(adapter,
                                                       /*bluetooth_class=*/0u,
                                                       /*name=*/"Test Device",
                                                       address,
                                                       /*paired=*/true,
                                                       /*connected=*/true) {}

  void CreateGattConnection(
      device::BluetoothDevice::GattConnectionCallback callback,
      absl::optional<device::BluetoothUUID> service_uuid =
          absl::nullopt) override {
    SetConnected(true);
    gatt_services_discovery_complete_ = true;
    std::move(callback).Run(
        std::make_unique<FakeBluetoothGattConnection>(adapter_, GetAddress()),
        /*error_code=*/absl::nullopt);
  }

  bool IsGattServicesDiscoveryComplete() const override {
    return gatt_services_discovery_complete_;
  }

  BluetoothRemoteGattService* GetGattService(
      const std::string& identifier) const override {
    return GetMockService(identifier);
  }

  std::vector<device::BluetoothRemoteGattService*> GetGattServices()
      const override {
    return GetMockServices();
  }

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

 protected:
  bool gatt_services_discovery_complete_ = false;
};

class FakeBluetoothChooser : public content::BluetoothChooser {
 public:
  FakeBluetoothChooser(content::BluetoothChooser::EventHandler event_handler,
                       const absl::optional<std::string>& device_to_select)
      : event_handler_(event_handler), device_to_select_(device_to_select) {}
  ~FakeBluetoothChooser() override = default;

  // content::BluetoothChooser implementation:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override {
    // Select the first device that is added if |device_to_select_| is not
    // populated.
    if (!device_to_select_) {
      event_handler_.Run(content::BluetoothChooserEvent::SELECTED, device_id);
      return;
    }

    // Otherwise, select the added device if its device ID matches
    // |device_to_select_|.
    if (device_to_select_.value() == device_id) {
      event_handler_.Run(content::BluetoothChooserEvent::SELECTED, device_id);
    }
  }

  // Move-only class
  FakeBluetoothChooser(const FakeBluetoothChooser&) = delete;
  FakeBluetoothChooser& operator=(const FakeBluetoothChooser&) = delete;

 private:
  content::BluetoothChooser::EventHandler event_handler_;
  absl::optional<std::string> device_to_select_;
};

class TestBluetoothDelegate : public permissions::BluetoothDelegateImpl {
 public:
  TestBluetoothDelegate()
      : permissions::BluetoothDelegateImpl(
            std::make_unique<ChromeBluetoothDelegateImplClient>()) {}
  ~TestBluetoothDelegate() override = default;
  TestBluetoothDelegate(const TestBluetoothDelegate&) = delete;
  TestBluetoothDelegate& operator=(const TestBluetoothDelegate&) = delete;

  void UseRealChooser() {
    EXPECT_FALSE(device_to_select_.has_value());
    use_real_chooser_ = true;
  }

  void SetDeviceToSelect(const std::string& device_address) {
    EXPECT_FALSE(use_real_chooser_);
    device_to_select_ = device_address;
  }

 protected:
  // content::BluetoothDelegate implementation:
  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override {
    if (use_real_chooser_) {
      return permissions::BluetoothDelegateImpl::RunBluetoothChooser(
          frame, event_handler);
    }
    return std::make_unique<FakeBluetoothChooser>(event_handler,
                                                  device_to_select_);
  }

  std::unique_ptr<content::BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      content::RenderFrameHost* frame,
      const content::BluetoothScanningPrompt::EventHandler& event_handler)
      override {
    // Simulate that a prompt was accepted; no actual prompt is needed here.
    event_handler.Run(content::BluetoothScanningPrompt::Event::kAllow);
    return nullptr;
  }

 private:
  absl::optional<std::string> device_to_select_;
  bool use_real_chooser_ = false;
};

class TestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;
  TestContentBrowserClient(const TestContentBrowserClient&) = delete;
  TestContentBrowserClient& operator=(const TestContentBrowserClient&) = delete;

  TestBluetoothDelegate* bluetooth_delegate() { return &bluetooth_delegate_; }

 protected:
  // ChromeContentBrowserClient:
  content::BluetoothDelegate* GetBluetoothDelegate() override {
    return &bluetooth_delegate_;
  }

 private:
  TestBluetoothDelegate bluetooth_delegate_;
};

class WebBluetoothTest : public InProcessBrowserTest {
 public:
  WebBluetoothTest() = default;
  ~WebBluetoothTest() override = default;

  // Move-only class
  WebBluetoothTest(const WebBluetoothTest&) = delete;
  WebBluetoothTest& operator=(const WebBluetoothTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(crbug.com/570344): Remove this switch once Web Bluetooth is
    // supported on Linux.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    // Necessary to make content/test/data/cross_site_iframe_factory.html work.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Web Bluetooth permissions are granted for an origin. The tests for Web
    // Bluetooth permissions run code across a browser restart by splitting the
    // tests into separate test cases where the test prefixed with PRE_ runs
    // first. EmbeddedTestServer is not capable of maintaining a consistent
    // origin across the separate tests, so URLLoaderInterceptor is used instead
    // to intercept navigation requests and serve the test page. This enables
    // the separate test cases to grant and check permissions for the same
    // origin.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.host() == "example.com") {
                content::URLLoaderInterceptor::WriteResponse(
                    "content/test/data/simple_page.html", params->client.get());
                return true;
              }
              return false;
            }));

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();
    global_values_ =
        device::BluetoothAdapterFactory::Get()->InitGlobalValuesForTesting();
    global_values_->SetLESupported(true);
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    old_browser_client_ = content::SetBrowserClientForTesting(&browser_client_);
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(old_browser_client_);
    url_loader_interceptor_.reset();
  }

  net::EmbeddedTestServer* CreateHttpsServer() {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory("content/test/data");
    https_server_->AddDefaultHandlers();
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    return https_server();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  void AddFakeDevice(const std::string& device_address) {
    constexpr int kProperties = BluetoothGattCharacteristic::PROPERTY_READ |
                                BluetoothGattCharacteristic::PROPERTY_NOTIFY;
    constexpr int kPermissions = BluetoothGattCharacteristic::PERMISSION_READ;

    auto fake_device =
        std::make_unique<FakeBluetoothDevice>(adapter_.get(), device_address);
    fake_device->AddUUID(kHeartRateUUID);
    auto fake_service =
        std::make_unique<testing::NiceMock<device::MockBluetoothGattService>>(
            fake_device.get(), kHeartRateUUIDString, kHeartRateUUID,
            /*is_primary=*/true);
    auto fake_characteristic =
        std::make_unique<FakeBluetoothGattCharacteristic>(
            fake_service.get(), kHeartRateMeasurementUUIDString,
            kHeartRateMeasurementUUID, kProperties, kPermissions);
    characteristic_ = fake_characteristic.get();
    fake_service->AddMockCharacteristic(std::move(fake_characteristic));
    fake_device->AddMockService(std::move(fake_service));
    adapter_->AddMockDevice(std::move(fake_device));
  }

  void RemoveFakeDevice(const std::string& device_address) {
    adapter_->RemoveMockDevice(device_address);
  }

  void SimulateDeviceAdvertisement(const std::string& device_address) {
    adapter_->SimulateDeviceAdvertisementReceived(device_address);
  }

  void SetDeviceToSelect(const std::string& device_address) {
    browser_client_.bluetooth_delegate()->SetDeviceToSelect(device_address);
  }

  void UseRealChooser() {
    browser_client_.bluetooth_delegate()->UseRealChooser();
  }

  void CheckLastCommitedOrigin(const std::string& pattern) {
    EXPECT_THAT(web_contents_->GetPrimaryMainFrame()
                    ->GetLastCommittedOrigin()
                    .Serialize(),
                testing::StartsWith(pattern));
  }

  std::unique_ptr<device::BluetoothAdapterFactory::GlobalValuesForTesting>
      global_values_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  TestContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient, AcrossTasksDanglingUntriaged>
      old_browser_client_ = nullptr;
  raw_ptr<FakeBluetoothGattCharacteristic, AcrossTasksDanglingUntriaged>
      characteristic_ = nullptr;

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_ =
      nullptr;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;

  // Web Bluetooth needs HTTPS to work (a secure context). Moreover,
  // ContentMockCertVerifier is used to avoid HTTPS certificate errors.
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, WebBluetoothAfterCrash) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  // Make sure we can use Web Bluetooth after the tab crashes.
  // Set up adapter with one device.
  adapter_->SetIsPresent(false);
  EXPECT_EQ(
      "NotFoundError: Bluetooth adapter not available.",
      content::EvalJs(
          web_contents_.get(),
          "navigator.bluetooth.requestDevice({filters: [{services: [0x180d]}]})"
          "  .catch(e => e.toString());"));

  // Crash the renderer process.
  content::RenderProcessHost* process =
      web_contents_->GetPrimaryMainFrame()->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0);
  crash_observer.Wait();

  // Reload tab.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));

  // Use Web Bluetooth again.
  EXPECT_EQ(
      "NotFoundError: Bluetooth adapter not available.",
      content::EvalJs(
          web_contents_.get(),
          "navigator.bluetooth.requestDevice({filters: [{services: [0x180d]}]})"
          "  .catch(e => e.toString());"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, KillSwitchShouldBlock) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  // Turn on the global kill switch.
  std::map<std::string, std::string> params;
  params["Bluetooth"] =
      permissions::PermissionContextBase::kPermissionsKillSwitchBlockedValue;
  base::AssociateFieldTrialParams(
      permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup", params);
  base::FieldTrialList::CreateFieldTrial(
      permissions::PermissionContextBase::kPermissionsKillSwitchFieldStudy,
      "TestGroup");

  std::string rejection =
      content::EvalJs(
          web_contents_.get(),
          "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})"
          "  .then(() => 'Success',"
          "        reason => reason.name + ': ' + reason.message"
          "  );")
          .ExtractString();
  EXPECT_THAT(rejection,
              testing::MatchesRegex("NotFoundError: .*globally disabled.*"));
}

// Tests that using Finch field trial parameters for blocklist additions has
// the effect of rejecting requestDevice calls.
IN_PROC_BROWSER_TEST_F(WebBluetoothTest, BlocklistShouldBlock) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  if (base::FieldTrialList::TrialExists("WebBluetoothBlocklist")) {
    LOG(INFO) << "WebBluetoothBlocklist field trial already configured.";
    ASSERT_NE(base::GetFieldTrialParamValue("WebBluetoothBlocklist",
                                            "blocklist_additions")
                  .find("ed5f25a4"),
              std::string::npos)
        << "ERROR: WebBluetoothBlocklist field trial being tested in\n"
           "testing/variations/fieldtrial_testing_config_*.json must\n"
           "include this test's random UUID 'ed5f25a4' in\n"
           "blocklist_additions.\n";
  } else {
    LOG(INFO) << "Creating WebBluetoothBlocklist field trial for test.";
    // Create a field trial with test parameter.
    std::map<std::string, std::string> params;
    params["blocklist_additions"] = "ed5f25a4:e";
    base::AssociateFieldTrialParams("WebBluetoothBlocklist", "TestGroup",
                                    params);
    base::FieldTrialList::CreateFieldTrial("WebBluetoothBlocklist",
                                           "TestGroup");
  }

  std::string rejection =
      content::EvalJs(web_contents_.get(),
                      "navigator.bluetooth.requestDevice({filters: [{services: "
                      "[0xed5f25a4]}]})"
                      "  .then(() => 'Success',"
                      "        reason => reason.name + ': ' + reason.message"
                      "  );")
          .ExtractString();
  EXPECT_THAT(rejection,
              testing::MatchesRegex("SecurityError: .*blocklisted UUID.*"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, NavigateWithChooserCrossOrigin) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  UseRealChooser();
  content::TestNavigationObserver observer(
      web_contents_, 1 /* number_of_navigations */,
      content::MessageLoopRunner::QuitMode::DEFERRED);

  auto waiter = test::ChooserBubbleUiWaiter::Create();

  EXPECT_TRUE(content::ExecJs(
      web_contents_.get(),
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})",
      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the chooser to be displayed before navigating to avoid a race
  // between the two IPCs.
  waiter->WaitForChange();
  EXPECT_TRUE(waiter->has_shown());

  EXPECT_TRUE(content::ExecJs(web_contents_.get(),
                              "document.location.href = 'https://google.com'"));

  observer.Wait();
  waiter->WaitForChange();
  EXPECT_TRUE(waiter->has_closed());
  EXPECT_EQ(GURL("https://google.com"), web_contents_->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, ShowChooserInBackgroundTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  UseRealChooser();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Create a new foreground tab that covers |web_contents|.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kExampleUrl), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Try to show the chooser in the background tab.
  EXPECT_EQ("NotFoundError: User cancelled the requestDevice() chooser.",
            content::EvalJs(web_contents,
                            R"((async () => {
      try {
        await navigator.bluetooth.requestDevice({ filters: [{name: 'Hello'}] });
        return "Expected error, got success.";
      } catch (e) {
        return `${e.name}: ${e.message}`;
      }
    })())"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, NotificationStartValueChangeRead) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  ASSERT_TRUE(characteristic_);
  characteristic_->DeferReadUntilNotificationStart();
  SetDeviceToSelect(kDeviceAddress);

  auto js_values = content::EvalJs(web_contents_.get(), R"((async () => {
      const kHeartRateMeasurementUUID = '00001234-0000-1000-8000-00805f9b34fb';
      const device = await navigator.bluetooth.requestDevice(
          {filters: [{name: 'Test Device', services: ['heart_rate']}]});
      const gatt = await device.gatt.connect();
      const service = await gatt.getPrimaryService('heart_rate');
      const characteristic =
          await service.getCharacteristic(kHeartRateMeasurementUUID);

      const readPromise = (async () => {
        const dataview = await characteristic.readValue();
        return dataview.getUint8(0);
      })();

      const notifyCharacteristic = await characteristic.startNotifications();
      const notifyPromise = new Promise(resolve => {
        notifyCharacteristic.addEventListener(
            'characteristicvaluechanged', event => {
          resolve(event.target.value.getUint8(0));
        });
      });

      return Promise.all([readPromise, notifyPromise]);
    })())");

  const base::Value promise_values = js_values.ExtractList();
  EXPECT_EQ(2U, promise_values.GetList().size());
  EXPECT_EQ(content::ListValueOf(1, 1), js_values);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTest, NotificationStartValueChangeNotify) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  ASSERT_TRUE(characteristic_);
  characteristic_->EmitChangeNotificationAtNotificationStart();
  SetDeviceToSelect(kDeviceAddress);

  EXPECT_EQ(1, content::EvalJs(web_contents_.get(), R"((async () => {
      const kHeartRateMeasurementUUID = '00001234-0000-1000-8000-00805f9b34fb';
      const device = await navigator.bluetooth.requestDevice(
          {filters: [{name: 'Test Device', services: ['heart_rate']}]});
      const gatt = await device.gatt.connect();
      const service = await gatt.getPrimaryService('heart_rate');
      const characteristic =
          await service.getCharacteristic(kHeartRateMeasurementUUID);
      const notifyCharacteristic = await characteristic.startNotifications();
      return new Promise((resolve) => {
        notifyCharacteristic.addEventListener(
            'characteristicvaluechanged', event => {
          const value = event.target.value.getUint8(0);
          resolve(value);
        });
      });
    })())"));
}

// The Web Bluetooth Permissions Policy tests should work with the
// WebBluetoothNewPermissionsBackend feature flag being either enabled or
// disabled.
class WebBluetoothPermissionsPolicyTest
    : public base::test::WithFeatureOverride,
      public WebBluetoothTest {
 public:
  WebBluetoothPermissionsPolicyTest()
      : base::test::WithFeatureOverride(
            features::kWebBluetoothNewPermissionsBackend) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebBluetoothTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "WebBluetoothGetDevices");
  }

  content::EvalJsResult InvokeRequestDevice(
      const content::ToRenderFrameHost& adapter) {
    return content::EvalJs(adapter, R"((async () => {
      try {
        const device = await navigator.bluetooth.requestDevice(
          {filters: [{name: 'Test Device', services: ['heart_rate']}]});
        return [ device.id ];
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })())");
  }

  content::EvalJsResult InvokeGetDevices(
      const content::ToRenderFrameHost& adapter) {
    return content::EvalJs(adapter, R"((async () => {
      try {
        const devices = await navigator.bluetooth.getDevices(
          {filters: [{name: 'Test Device', services: ['heart_rate']}]});
        return devices.map(device => device.id);
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })())");
  }
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(WebBluetoothPermissionsPolicyTest);

IN_PROC_BROWSER_TEST_P(WebBluetoothPermissionsPolicyTest,
                       ThrowSecurityWhenIFrameIsDisallowed) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  GURL outer_url = https_server()->GetURL(
      "outer.com", "/cross_site_iframe_factory.html?outer(inner())");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), outer_url));

  // Attempting to access bluetooth devices from a disallowed iframe should
  // throw a SecurityError.
  content::EvalJsResult inner_device_error = InvokeGetDevices(
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0));
  EXPECT_EQ(
      "SecurityError: Failed to execute 'getDevices' on 'Bluetooth': Access to "
      "the feature \"bluetooth\" is disallowed by permissions policy.",
      inner_device_error);
}

IN_PROC_BROWSER_TEST_P(WebBluetoothPermissionsPolicyTest,
                       AllowedChildFrameShouldHaveAccessToParentDevices) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  GURL outer_url = https_server()->GetURL(
      "outer.com",
      "/cross_site_iframe_factory.html?outer(inner{allow-bluetooth}())");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), outer_url));

  // The inner frame should have access to the same devices that the main page
  // has requested.
  content::EvalJsResult outer_device_id =
      InvokeRequestDevice(web_contents_.get());
  content::EvalJsResult inner_device_id = InvokeGetDevices(
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0));
  ASSERT_TRUE(outer_device_id.value.is_list()) << outer_device_id.value;
  ASSERT_TRUE(inner_device_id.value.is_list()) << inner_device_id.value;
  EXPECT_EQ(outer_device_id.ExtractList(), inner_device_id.ExtractList());

  // If we navigate the main frame to inner.com, it should lose access to the
  // outer.com devices.
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  GURL inner_url = https_server()->GetURL("inner.com", "/simple_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), inner_url));
  content::EvalJsResult inner_device_id_after_navigation =
      InvokeGetDevices(web_contents_.get());
  // Expect an empty list.
  EXPECT_EQ(base::Value(base::Value::List()), inner_device_id_after_navigation);
}

IN_PROC_BROWSER_TEST_P(WebBluetoothPermissionsPolicyTest,
                       ParentShouldHaveAccessToAllowedChildFrameDevices) {
  ASSERT_TRUE(CreateHttpsServer()->Start());
  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  GURL outer_url = https_server()->GetURL(
      "outer.com",
      "/cross_site_iframe_factory.html?outer(inner{allow-bluetooth}())");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), outer_url));

  // The main page should have access to the same devices that the inner frame
  // has requested.
  content::EvalJsResult inner_device_id = InvokeRequestDevice(
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0));
  content::EvalJsResult outer_device_id = InvokeGetDevices(web_contents_.get());
  ASSERT_TRUE(outer_device_id.value.is_list()) << outer_device_id.value;
  ASSERT_TRUE(inner_device_id.value.is_list()) << inner_device_id.value;
  EXPECT_EQ(outer_device_id.ExtractList(), inner_device_id.ExtractList());
}

// The new Web Bluetooth permissions backend is currently implemented behind a
// feature flag.
// TODO(https://crbug.com/589228): Delete this class and convert all the tests
// using it to use WebBluetoothTest instead.
class WebBluetoothTestWithNewPermissionsBackendEnabled
    : public WebBluetoothTest {
 public:
  WebBluetoothTestWithNewPermissionsBackendEnabled() {
    feature_list_.InitAndEnableFeature(
        features::kWebBluetoothNewPermissionsBackend);
  }

  // Move-only class
  WebBluetoothTestWithNewPermissionsBackendEnabled(
      const WebBluetoothTestWithNewPermissionsBackendEnabled&) = delete;
  WebBluetoothTestWithNewPermissionsBackendEnabled& operator=(
      const WebBluetoothTestWithNewPermissionsBackendEnabled&) = delete;

  void SetUp() override {
    // Called to prevent flakiness that may arise from changes in the window
    // visibility. WebBluetoothServiceImpl may clear up its WatchAdvertisement
    // client lists before being able to simulate the device advertisement when
    // the window visibility changes or lose focus, resulting in timeouts from
    // promises that never resolve. This function prevents this clean up from
    // happening.
    content::IgnoreBluetoothVisibilityRequirementsForTesting();
    WebBluetoothTest::SetUp();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothPersistentIds) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // Grant permission for the device with address |kDeviceAddress| and store its
  // WebBluetoothDeviceId in localStorage to retrieve it after the browser
  // restarts.
  auto request_device_result =
      content::EvalJs(web_contents_.get(), R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            localStorage.setItem('requestDeviceId', device.id);
            return device.id;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())");
  const std::string& granted_id = request_device_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(granted_id));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothPersistentIds) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // At the moment, there is not a way for Web Bluetooth to return a list of the
  // previously granted Bluetooth devices, so use requestDevice here.
  // TODO(https://crbug.com/577953): Once there is an API that can return the
  // permitted Web Bluetooth devices, use that API instead.
  auto request_device_result =
      content::EvalJs(web_contents_.get(), R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            return device.id;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())");
  const std::string& granted_id = request_device_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(granted_id));

  auto local_storage_get_item_result = content::EvalJs(web_contents_.get(), R"(
        (async() => {
          return localStorage.getItem('requestDeviceId');
        })())");
  const std::string& prev_granted_id =
      local_storage_get_item_result.ExtractString();
  EXPECT_EQ(granted_id, prev_granted_id);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothScanningIdsNotPersistent) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  // The request to scan should be automatically accepted. Store the detected
  // device's WebBluetoothDeviceId in localStorage to retrieve it after the
  // browser restarts.
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), "requestLEScanPromise"));

  ASSERT_TRUE(content::ExecJs(web_contents_.get(), R"(
        var advertisementreceivedPromise = new Promise(resolve => {
          navigator.bluetooth.addEventListener('advertisementreceived',
              event => {
                localStorage.setItem('requestLEScanId', event.device.id);
                resolve(event.device.id);
              });
        });
      )"));

  SimulateDeviceAdvertisement(kDeviceAddress);

  auto advertisementreceived_promise_result =
      content::EvalJs(web_contents_.get(), "advertisementreceivedPromise ");
  const std::string& scan_id =
      advertisementreceived_promise_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(scan_id));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothScanningIdsNotPersistent) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  // The request to scan should be automatically accepted. Store the detected
  // assigned to the scanned device against the one that was stored previously.
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), "requestLEScanPromise"));

  ASSERT_TRUE(content::ExecJs(web_contents_.get(), R"(
        var advertisementreceivedPromise = new Promise(resolve => {
          navigator.bluetooth.addEventListener('advertisementreceived',
              event => {
                resolve(event.device.id);
              });
        });
      )"));

  SimulateDeviceAdvertisement(kDeviceAddress);

  auto advertisementreceived_promise_result =
      content::EvalJs(web_contents_.get(), "advertisementreceivedPromise ");
  const std::string& scan_id =
      advertisementreceived_promise_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(scan_id));

  auto local_storage_get_item_result = content::EvalJs(
      web_contents_.get(), "localStorage.getItem('requestLEScanId')");
  const std::string& prev_scan_id =
      local_storage_get_item_result.ExtractString();
  EXPECT_NE(scan_id, prev_scan_id);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothIdsUsedInWebBluetoothScanning) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // Grant permission for the device with address |kDeviceAddress| and store its
  // WebBluetoothDeviceId in localStorage to retrieve it after the browser
  // restarts.
  auto request_device_result =
      content::EvalJs(web_contents_.get(), R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            localStorage.setItem('requestDeviceId', device.id);
            return device.id;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())");
  const std::string& granted_id = request_device_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(granted_id));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothIdsUsedInWebBluetoothScanning) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  // The request to scan should be automatically accepted. Store the detected
  // assigned to the scanned device against the one that was stored previously.
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), R"(
      var requestLEScanPromise = navigator.bluetooth.requestLEScan({
        acceptAllAdvertisements: true});
  )"));
  ASSERT_TRUE(content::ExecJs(web_contents_.get(), "requestLEScanPromise"));

  ASSERT_TRUE(content::ExecJs(web_contents_.get(), R"(
        var advertisementreceivedPromise = new Promise(resolve => {
          navigator.bluetooth.addEventListener('advertisementreceived',
              event => {
                resolve(event.device.id);
              });
        });
      )"));

  SimulateDeviceAdvertisement(kDeviceAddress);

  auto advertisementreceived_promise_result =
      content::EvalJs(web_contents_.get(), "advertisementreceivedPromise ");
  const std::string& scan_id =
      advertisementreceived_promise_result.ExtractString();
  EXPECT_TRUE(blink::WebBluetoothDeviceId::IsValid(scan_id));

  auto local_storage_get_item_result = content::EvalJs(
      web_contents_.get(), "localStorage.getItem('requestDeviceId')");
  const std::string& granted_id = local_storage_get_item_result.ExtractString();
  EXPECT_EQ(scan_id, granted_id);
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       PRE_WebBluetoothPersistentServices) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // Grant permission for the device with address |kDeviceAddress| and store its
  // WebBluetoothDeviceId in localStorage to retrieve it after the browser
  // restarts.
  EXPECT_EQ(kHeartRateUUIDString,
            content::EvalJs(web_contents_.get(), R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device', services: ['heart_rate']}]});
            let gatt = await device.gatt.connect();
            let service = await gatt.getPrimaryService('heart_rate');
            return service.uuid;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       WebBluetoothPersistentServices) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // At the moment, there is not a way for Web Bluetooth to return a list of the
  // previously granted Bluetooth devices, so use requestDevice here without
  // specifying a filter for services. The site should still be able to GATT
  // connect and get the primary 'heart_rate' GATT service.
  // TODO(https://crbug.com/577953): Once there is an API that can return the
  // permitted Web Bluetooth devices, use that API instead.
  EXPECT_EQ(kHeartRateUUIDString,
            content::EvalJs(web_contents_.get(), R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            let gatt = await device.gatt.connect();
            let service = await gatt.getPrimaryService('heart_rate');
            return service.uuid;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())"));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       RevokingPermissionDisconnectsTheDevice) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // Connect to heart rate device and ensure the GATT service is connected.
  EXPECT_EQ(kHeartRateUUIDString, content::EvalJs(web_contents_.get(), R"(
    var gatt;
    var gattserverdisconnectedPromise;

    (async() => {
      try {
        let device = await navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]});
        gatt = await device.gatt.connect();
        gattserverdisconnectedPromise = new Promise(resolve => {
          device.addEventListener('gattserverdisconnected', _ => {
            resolve("event fired");
          });
        });
        let service = await gatt.getPrimaryService('heart_rate');
        return service.uuid;
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })()
  )"));

  permissions::BluetoothChooserContext* context =
      BluetoothChooserContextFactory::GetForProfile(browser()->profile());
  url::Origin origin =
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  // Revoke the permission.
  const auto objects = context->GetGrantedObjects(origin);
  EXPECT_EQ(1ul, objects.size());
  context->RevokeObjectPermission(origin, objects.at(0)->value);

  // Wait for gattserverdisconnect event.
  EXPECT_EQ("event fired", content::EvalJs(web_contents_.get(),
                                           "gattserverdisconnectedPromise "));

  // Ensure the service is disconnected.
  EXPECT_THAT(content::EvalJs(web_contents_.get(), R"((async() => {
      try {
        let service = await gatt.getPrimaryService('heart_rate');
        return service.uuid;
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })())")
                  .ExtractString(),
              ::testing::HasSubstr("GATT Server is disconnected."));
}

IN_PROC_BROWSER_TEST_F(WebBluetoothTestWithNewPermissionsBackendEnabled,
                       RevokingPermissionStopsAdvertisements) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kExampleUrl)));
  web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  CheckLastCommitedOrigin(kExampleUrl);

  // Setup the fake device.
  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  // Request device and watch for advertisements. Record the last seen
  // advertisement's name.
  EXPECT_EQ("", content::EvalJs(web_contents_.get(), R"(
    var events_seen = "";
    var first_device_promise;
    (async() => {
      try {
        let device = await navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]});
        device.watchAdvertisements();
        first_device_promise = new Promise(resolve => {
          device.addEventListener('advertisementreceived', event => {
            events_seen += event.name + "|";
            resolve(events_seen);
          });
        });
        return "";
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })()
  )"));

  url::Origin origin =
      web_contents_->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  permissions::BluetoothChooserContext* context =
      BluetoothChooserContextFactory::GetForProfile(browser()->profile());
  auto objects = context->GetGrantedObjects(origin);
  ASSERT_EQ(1u, objects.size());
  const auto first_object_key = context->GetKeyForObject(objects.at(0)->value);

  // Add a second listener on a different device which is used purely as an
  // indicator of how much to wait until we can be reasonably sure that the
  // second advertisement will not arrive.
  AddFakeDevice(kDeviceAddress2);
  SetDeviceToSelect(kDeviceAddress2);

  EXPECT_EQ("", content::EvalJs(web_contents_.get(), R"(
    var second_device_promise;
    (async() => {
      try {
        let device = await navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]});
        device.watchAdvertisements();
        second_device_promise = new Promise(resolve => {
          device.addEventListener('advertisementreceived', event => {
            events_seen += 'second_device_' + event.name;
            resolve(events_seen);
          });
        });
        return "";
      } catch(e) {
        return `${e.name}: ${e.message}`;
      }
    })()
  )"));

  // Number of granted objects should be 2.
  objects = context->GetGrantedObjects(origin);
  EXPECT_EQ(2u, objects.size());

  // Send first advertisement and wait for the event to be resolved.
  adapter_->SimulateDeviceAdvertisementReceived(kDeviceAddress,
                                                "advertisement_name1");
  EXPECT_EQ("advertisement_name1|",
            content::EvalJs(web_contents_.get(), "first_device_promise"));

  // Revoke the permission.
  context->RevokeObjectPermission(origin, first_object_key);
  EXPECT_EQ(1ul, context->GetGrantedObjects(origin).size());

  // Send another advertisement after the permission was revoked, this
  // advertisement event should not be received. Also send an advertisement
  // to the second device which, when received, will indicate that we have
  // waited enough.

  adapter_->SimulateDeviceAdvertisementReceived(kDeviceAddress,
                                                "advertisement_name2");
  adapter_->SimulateDeviceAdvertisementReceived(kDeviceAddress2,
                                                "advertisement_name2");

  EXPECT_EQ("advertisement_name1|second_device_advertisement_name2",
            content::EvalJs(web_contents_.get(), "second_device_promise"));
}

class WebBluetoothTestWithNewPermissionsBackendEnabledInPrerendering
    : public WebBluetoothTestWithNewPermissionsBackendEnabled {
 public:
  WebBluetoothTestWithNewPermissionsBackendEnabledInPrerendering()
      : prerender_helper_(base::BindRepeating(
            &WebBluetoothTestWithNewPermissionsBackendEnabledInPrerendering::
                GetWebContents,
            base::Unretained(this))) {}
  ~WebBluetoothTestWithNewPermissionsBackendEnabledInPrerendering() override =
      default;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    WebBluetoothTestWithNewPermissionsBackendEnabled::SetUp();
  }

  void SetUpOnMainThread() override {
    WebBluetoothTestWithNewPermissionsBackendEnabled::SetUpOnMainThread();
    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());

    auto url = embedded_test_server()->GetURL("/empty.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* GetWebContents() { return web_contents_; }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
  net::test_server::EmbeddedTestServerHandle test_server_handle_;
};

class TestWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit TestWebContentsObserver(content::WebContents* contents)
      : WebContentsObserver(contents) {}
  TestWebContentsObserver(const TestWebContentsObserver&) = delete;
  TestWebContentsObserver& operator=(const TestWebContentsObserver&) = delete;
  ~TestWebContentsObserver() override = default;

  void OnIsConnectedToBluetoothDeviceChanged(
      bool is_connected_to_bluetooth_device) override {
    ++num_is_connected_to_bluetooth_device_changed_;
    last_is_connected_to_bluetooth_device_ = is_connected_to_bluetooth_device;
    if (quit_closure_ && expected_updating_count_ ==
                             num_is_connected_to_bluetooth_device_changed_) {
      std::move(quit_closure_).Run();
    }
  }

  int num_is_connected_to_bluetooth_device_changed() {
    return num_is_connected_to_bluetooth_device_changed_;
  }

  const absl::optional<bool>& last_is_connected_to_bluetooth_device() {
    return last_is_connected_to_bluetooth_device_;
  }

  void clear_last_is_connected_to_bluetooth_device() {
    last_is_connected_to_bluetooth_device_.reset();
  }

  void WaitUntilConnectionIsUpdated(int expected_count) {
    if (num_is_connected_to_bluetooth_device_changed_ == expected_count)
      return;
    expected_updating_count_ = expected_count;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  int num_is_connected_to_bluetooth_device_changed_ = 0;
  absl::optional<bool> last_is_connected_to_bluetooth_device_;
  int expected_updating_count_;
  base::OnceClosure quit_closure_;
};

// Tests that the connection of Web Bluetooth is deferred in the prerendering.
IN_PROC_BROWSER_TEST_F(
    WebBluetoothTestWithNewPermissionsBackendEnabledInPrerendering,
    WebBluetoothDeviceConnectInPrerendering) {
  TestWebContentsObserver observer(GetWebContents());

  AddFakeDevice(kDeviceAddress);
  SetDeviceToSelect(kDeviceAddress);

  ASSERT_TRUE(content::ExecJs(GetWebContents(), R"((async() => {
          try {
            let device = await navigator.bluetooth.requestDevice({
              filters: [{name: 'Test Device'}]});
            let gatt = await device.gatt.connect();
            let service = await gatt.getPrimaryService('heart_rate');
            return service.uuid;
          } catch(e) {
            return `${e.name}: ${e.message}`;
          }
        })())"));

  observer.WaitUntilConnectionIsUpdated(1);
  // In the active main frame, the connection of Web Bluetooth works.
  EXPECT_EQ(observer.num_is_connected_to_bluetooth_device_changed(), 1);
  EXPECT_TRUE(observer.last_is_connected_to_bluetooth_device().has_value());
  EXPECT_TRUE(observer.last_is_connected_to_bluetooth_device().value());
  observer.clear_last_is_connected_to_bluetooth_device();

  // Loads a page in the prerender.
  auto prerender_url = embedded_test_server()->GetURL("/simple.html");
  // The prerendering doesn't affect the current scanning.
  int host_id = prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  content::RenderFrameHost* prerendered_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);

  constexpr char kUserGestureError[] =
      "Must be handling a user gesture to show a permission request.";
  auto result =
      content::EvalJs(prerendered_frame_host, R"(
      navigator.bluetooth.requestDevice({
          filters: [{name: 'Test Device', services: ['heart_rate']}]}))",
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_THAT(result.error, ::testing::HasSubstr(kUserGestureError));

  // In the prerendering, the connection of Web Bluetooth is deferred and
  // `observer` doesn't have any update.
  EXPECT_EQ(observer.num_is_connected_to_bluetooth_device_changed(), 1);
  EXPECT_FALSE(observer.last_is_connected_to_bluetooth_device().has_value());

  content::RenderFrameDeletedObserver rfh_observer(
      GetWebContents()->GetPrimaryMainFrame());

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());

  // Wait until the previous RFH to be disposed of.
  rfh_observer.WaitUntilDeleted();

  // During prerendering activation, the connection from the previous
  // RenderFrameHost to Web Bluetooth is closed, while the connection attempt
  // from the prerendering RenderFrameHost was refused.
  EXPECT_EQ(observer.num_is_connected_to_bluetooth_device_changed(), 2);
  EXPECT_TRUE(observer.last_is_connected_to_bluetooth_device().has_value());
  EXPECT_FALSE(observer.last_is_connected_to_bluetooth_device().value());
}

}  // namespace
