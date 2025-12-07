// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/strings/stringprintf.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate.h"
#include "chrome/browser/bluetooth/chrome_bluetooth_delegate_impl_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

// Espruino devices like Pixl.js present a Nordic UART service that
// provides serial port-like access to the Espruino REPL.
// https://www.espruino.com/BLE+UART
constexpr std::string_view kNordicUartServiceUuid =
    "6e400001-b5a3-f393-e0a9-e50e24dcca9e";

// The chooser that will select the first device shown in the chooser.
class BluetoothRealTargetTestChooser : public content::BluetoothChooser {
 public:
  explicit BluetoothRealTargetTestChooser(
      content::BluetoothChooser::EventHandler event_handler)
      : event_handler_(std::move(event_handler)) {}

  BluetoothRealTargetTestChooser(const BluetoothRealTargetTestChooser&) =
      delete;
  BluetoothRealTargetTestChooser& operator=(
      const BluetoothRealTargetTestChooser&) = delete;
  ~BluetoothRealTargetTestChooser() override = default;

  // content::BluetoothChooser implementation:
  void AddOrUpdateDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name,
                         bool is_gatt_connected,
                         bool is_paired,
                         int signal_strength_level) override {
    event_handler_.Run(content::BluetoothChooserEvent::SELECTED, device_id);
  }

 private:
  content::BluetoothChooser::EventHandler event_handler_;
};

class BluetoothRealTargetTestDelegate : public ChromeBluetoothDelegate {
 public:
  BluetoothRealTargetTestDelegate()
      : ChromeBluetoothDelegate(
            std::make_unique<ChromeBluetoothDelegateImplClient>()) {}
  ~BluetoothRealTargetTestDelegate() override = default;

  std::unique_ptr<content::BluetoothChooser> RunBluetoothChooser(
      content::RenderFrameHost* frame,
      const content::BluetoothChooser::EventHandler& event_handler) override {
    return std::make_unique<BluetoothRealTargetTestChooser>(event_handler);
  }
};

class TestBluetoothContentBrowserClient : public content::ContentBrowserClient {
 public:
  TestBluetoothContentBrowserClient()
      : delegate_(std::make_unique<BluetoothRealTargetTestDelegate>()) {}
  ~TestBluetoothContentBrowserClient() override = default;

  content::BluetoothDelegate* GetBluetoothDelegate() override {
    return delegate_.get();
  }

  void SetAsBrowserClient() {
    original_content_browser_client_ =
        content::SetBrowserClientForTesting(this);
  }

  void UnsetAsBrowserClient() {
    content::SetBrowserClientForTesting(original_content_browser_client_);
    delegate_.reset();
  }

 private:
  std::unique_ptr<BluetoothRealTargetTestDelegate> delegate_;
  raw_ptr<content::ContentBrowserClient> original_content_browser_client_ =
      nullptr;
};

class BluetoothRealTargetTest : public InProcessBrowserTest {
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

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_LINUX)
    // TODO(41229108): Remove this switch once Web Bluetooth is
    // supported on Linux.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
#endif
  }

 private:
  TestBluetoothContentBrowserClient test_content_browser_client_;
};

IN_PROC_BROWSER_TEST_F(BluetoothRealTargetTest, ConnectAndDisconnectDevice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto test_script = base::StringPrintf(
      R"((async () => {
              let device = await navigator.bluetooth.requestDevice({
                filters: [
                  {namePrefix: "Pixl.js"},
                ],
                optionalServices: ["%s"]
              });
              let server = await device.gatt.connect();
              await server.getPrimaryServices();
              let result = server.connected;
              await server.disconnect();
              return result;
        })())",
      kNordicUartServiceUuid);
  EXPECT_EQ(true, EvalJs(web_contents, test_script));
}

}  // namespace
