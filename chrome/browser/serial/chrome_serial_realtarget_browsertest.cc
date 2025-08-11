// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/serial/chrome_serial_delegate.h"
#include "chrome/browser/serial/web_serial_chooser.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace {

constexpr int kMicroBitVendorId = 0x0d28;
constexpr int kMicroBitProductId = 0x0204;
constexpr std::string_view kPixelBudsProSerialPortUuid =
    "25e97ff7-24ce-4c4c-8951-f764a708f7b5";

// The chooser view that will select the first device in the list upon the list
// initialization or the first device added.
class SerialRealTargetTestChooserView
    : public permissions::ChooserController::View {
 public:
  explicit SerialRealTargetTestChooserView(
      std::unique_ptr<permissions::ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }
  SerialRealTargetTestChooserView(const SerialRealTargetTestChooserView&) =
      delete;
  SerialRealTargetTestChooserView& operator=(
      const SerialRealTargetTestChooserView&) = delete;

  ~SerialRealTargetTestChooserView() override {
    controller_->set_view(nullptr);
  }

  void OnOptionsInitialized() override {
    if (!selected_ && controller_->NumOptions() > 0) {
      controller_->Select({0});
      selected_ = true;
    }
  }

  void OnOptionAdded(size_t index) override {
    if (!selected_ && controller_->NumOptions() > 0) {
      controller_->Select({0});
      selected_ = true;
    }
  }
  void OnOptionRemoved(size_t index) override {}
  void OnOptionUpdated(size_t index) override {}
  void OnAdapterEnabledChanged(bool enabled) override {}
  void OnRefreshStateChanged(bool refreshing) override {}

 private:
  std::unique_ptr<permissions::ChooserController> controller_;
  bool selected_ = false;
};

class SerialRealTargetTestChooser : public WebSerialChooser {
 public:
  SerialRealTargetTestChooser() = default;
  SerialRealTargetTestChooser(const SerialRealTargetTestChooser&) = delete;
  SerialRealTargetTestChooser& operator=(const SerialRealTargetTestChooser&) =
      delete;
  ~SerialRealTargetTestChooser() override = default;

  void ShowChooser(
      content::RenderFrameHost* frame,
      std::unique_ptr<SerialChooserController> controller) override {
    view_ = std::make_unique<SerialRealTargetTestChooserView>(
        std::move(controller));
  }

 private:
  std::unique_ptr<SerialRealTargetTestChooserView> view_;
};

class SerialRealTargetTestDelegate : public ChromeSerialDelegate {
 public:
  SerialRealTargetTestDelegate() = default;
  ~SerialRealTargetTestDelegate() override = default;

  std::unique_ptr<content::SerialChooser> RunChooser(
      content::RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids,
      content::SerialChooser::Callback callback) override {
    auto chooser = std::make_unique<SerialRealTargetTestChooser>();
    chooser->ShowChooser(frame,
                         std::make_unique<SerialChooserController>(
                             frame, std::move(filters),
                             std::move(allowed_bluetooth_service_class_ids),
                             std::move(callback)));
    return chooser;
  }
};

class TestContentBrowserClient : public content::ContentBrowserClient {
 public:
  TestContentBrowserClient()
      : serial_delegate_(std::make_unique<SerialRealTargetTestDelegate>()) {}
  ~TestContentBrowserClient() override = default;

  content::SerialDelegate* GetSerialDelegate() override {
    return serial_delegate_.get();
  }

  void SetAsBrowserClient() {
    original_content_browser_client_ =
        content::SetBrowserClientForTesting(this);
  }

  void UnsetAsBrowserClient() {
    content::SetBrowserClientForTesting(original_content_browser_client_);
    serial_delegate_.reset();
  }

 private:
  std::unique_ptr<SerialRealTargetTestDelegate> serial_delegate_;
  raw_ptr<content::ContentBrowserClient> original_content_browser_client_ =
      nullptr;
};

class SerialRealTargetTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    test_content_browser_client_.SetAsBrowserClient();

    GURL url = embedded_test_server()->GetURL("localhost", "/simple_page.html");
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::RenderFrameHost* render_frame_host = browser()
                                                      ->tab_strip_model()
                                                      ->GetActiveWebContents()
                                                      ->GetPrimaryMainFrame();
    EXPECT_EQ(url.DeprecatedGetOriginAsURL(),
              render_frame_host->GetLastCommittedOrigin().GetURL());
  }

  void TearDownOnMainThread() override {
    test_content_browser_client_.UnsetAsBrowserClient();
  }

  // Attempt socket connection with `uuid_string` to all bluetooth devices known
  // to the adapter.
  void AttemptSocketConnectionToBluetoothDevices(std::string uuid_string) {
    base::test::TestFuture<scoped_refptr<device::BluetoothAdapter>>
        adapter_future;
    device::BluetoothAdapterFactory::Get()->GetAdapter(
        adapter_future.GetCallback());

    scoped_refptr<device::BluetoothAdapter> adapter = adapter_future.Get();
    ASSERT_TRUE(adapter);
    for (auto* device : adapter->GetDevices()) {
      base::test::TestFuture<scoped_refptr<device::BluetoothSocket>>
          socket_future;
      auto split_callback =
          base::SplitOnceCallback(socket_future.GetCallback());

      LOG(INFO) << "Attempt socket connection with " << uuid_string
                << " to device " << device->GetNameForDisplay();
      // The purpose of this function is to make the system to talk to bluetooth
      // devices so UUIDs of the devices will be known to the system, which
      // avoid issue like empty UUIDs in BluetoothDevice when testing after
      // system reboot.
      device->ConnectToService(
          device::BluetoothUUID(uuid_string), std::move(split_callback.first),
          base::BindLambdaForTesting([&](const std::string& message) {
            std::move(split_callback.second).Run(nullptr);
          }));
      scoped_refptr<device::BluetoothSocket> socket = socket_future.Get();
      if (socket) {
        base::test::TestFuture<void> disconnect_future;
        socket->Disconnect(disconnect_future.GetCallback());
        ASSERT_TRUE(disconnect_future.Wait());
      }
    }
  }

 private:
  TestContentBrowserClient test_content_browser_client_;
};

IN_PROC_BROWSER_TEST_F(SerialRealTargetTest, SerialOpenAndClosePort) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto test_script = base::StringPrintf(
      R"((async () => {
            let port = await navigator.serial.requestPort(
              {filters: [{ usbVendorId: %d, usbProductId: %d }]}
            );
            await port.open({ baudRate: 115200 });
            let result = port.connected;
            await port.close();
            return result;
      })())",
      kMicroBitVendorId, kMicroBitProductId);
  EXPECT_EQ(true, EvalJs(web_contents, test_script));
}

IN_PROC_BROWSER_TEST_F(SerialRealTargetTest, SetSignals) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto test_script = base::StringPrintf(
      R"((async () => {
        const port = await navigator.serial.requestPort(
          {filters: [{ usbVendorId: %d, usbProductId: %d }]}
        );
        await port.open({ baudRate: 115200 });
        for (let i = 3; i >= 0; --i) {
          const expectedDataTerminalReady = !!(i & 0x1);
          const expectedRequestToSend = !!(i & 0x2);
          await port.setSignals({
            dataTerminalReady: expectedDataTerminalReady,
            requestToSend: expectedRequestToSend,
          });
        }
        await port.close();
        return true;
      })())",
      kMicroBitVendorId, kMicroBitProductId);
  EXPECT_EQ(true, EvalJs(web_contents, test_script));
}

IN_PROC_BROWSER_TEST_F(SerialRealTargetTest, BluetoothSerialOpenAndClosePort) {
  // This step is to make the target bluetooth device connected to the system,
  // to reduce flaky when running Serial Bluetooth test.
  AttemptSocketConnectionToBluetoothDevices(
      std::string(kPixelBudsProSerialPortUuid));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto test_script = base::StringPrintf(
      R"((async () => {
            const uuids=['%s'];
            let port = await navigator.serial.requestPort({
              allowedBluetoothServiceClassIds:uuids, filters:[
                {bluetoothServiceClassId:uuids[0]}
              ]
            });
            // The retry here is because the pixel bud in the lab is in the case
            // with lid opened, where the first open attempt would fail.
            async function openPortWithRetry(port, maxRetries = 2) {
              for (let retry = 0; retry < maxRetries; retry++) {
                try {
                  await port.open({ baudRate: 115200 });
                  return;
                } catch (error) {
                  await new Promise(resolve => setTimeout(resolve, 1000));
                }
              }
              throw new Error('Failed to open port after multiple attempts.');
            }
            await openPortWithRetry(port);
            let result = port.connected;
            await port.close();
            return result;
      })())",
      kPixelBudsProSerialPortUuid);
  EXPECT_EQ(true, EvalJs(web_contents, test_script));
}

}  // namespace
