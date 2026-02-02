// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

// Verifies that the command result does not contain an "error" field.
testing::Matcher<const base::DictValue*> IsSuccess() {
  return testing::Pointee(testing::ResultOf(
      "FindDict('error')",
      [](const base::DictValue& dict) { return dict.FindDict("error"); },
      testing::IsNull()));
}

// A simplified struct to make test assertions readable.
// Note: This does not cover all Mojo reader state flags,
// only the specific flags relevant to these tests.
struct TestReaderStateOut {
  std::string name;
  int event_count = 0;
  std::vector<uint8_t> atr;
  // Selected flags
  bool changed = false;
  bool present = false;
  bool empty = false;
};

}  // namespace

class SmartCardEmulationBrowserTest : public IsolatedWebAppBrowserTestHarness {
 public:
  SmartCardEmulationBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSmartCard);
  }
  ~SmartCardEmulationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    InstallAndLaunchIWA();
    AttachDevTools();
    SetAutoAcceptPermissions();
  }

  void TearDownOnMainThread() override {
    if (devtools_client_) {
      devtools_client_->DetachProtocolClient();
      devtools_client_.reset();
    }
    web_contents_ = nullptr;
    IsolatedWebAppBrowserTestHarness::TearDownOnMainThread();
  }

  const base::DictValue* SendCommand(const std::string& method,
                                     base::DictValue params = {}) {
    return devtools_client_->SendCommandSync(method, std::move(params));
  }

  base::DictValue WaitForNotificationParams(const std::string& event_name) {
    return devtools_client_->WaitForNotification(event_name, true);
  }

  void SendResponse(const std::string& response_method,
                    const base::DictValue& event_params,
                    base::DictValue result_fields = {}) {
    const std::string* req_id = event_params.FindString("requestId");
    ASSERT_TRUE(req_id) << "Event missing 'requestId'";

    result_fields.Set("requestId", *req_id);
    ASSERT_THAT(SendCommand(response_method, std::move(result_fields)),
                IsSuccess());
  }

  content::TestDevToolsProtocolClient* client() {
    return devtools_client_.get();
  }

  content::RenderFrameHost* app_frame() {
    return web_contents_->GetPrimaryMainFrame();
  }

  content::WebContents* web_contents() { return web_contents_; }

  // Helper: Waits for establishContext and reports success.
  void HandleEstablishContext(
      int context_id = 123,
      std::optional<std::string> error_code = std::nullopt) {
    auto params = WaitForNotificationParams(
        "SmartCardEmulation.establishContextRequested");
    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
      return;
    }
    SendResponse("SmartCardEmulation.reportEstablishContextResult", params,
                 base::DictValue().Set("contextId", context_id));
  }

  // Helper: Waits for listReaders and reports either success or error.
  void HandleListReaders(const std::vector<std::string>& readers = {"Reader A"},
                         int context_id = 123,
                         std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.listReadersRequested");
    EXPECT_EQ(params.FindInt("contextId"), context_id);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
      return;
    }
    base::ListValue readers_list;
    for (const auto& reader : readers) {
      readers_list.Append(reader);
    }
    SendResponse("SmartCardEmulation.reportListReadersResult", params,
                 base::DictValue().Set("readers", std::move(readers_list)));
  }

  // Helper: Waits for connect and reports either success or error.
  void HandleConnect(const std::string& expected_reader = "Reader A",
                     const int handle = 123,
                     std::optional<std::string> active_protocol = std::nullopt,
                     std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.connectRequested");

    // Verify the request is for the correct reader.
    const std::string* reader = params.FindString("reader");
    ASSERT_THAT(reader, testing::Pointee(testing::StrEq(expected_reader)))
        << "Connect request was for the wrong reader!";

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
      return;
    }
    base::DictValue response;
    response.Set("handle", handle);
    if (active_protocol.has_value()) {
      response.Set("activeProtocol", *active_protocol);
    }
    SendResponse("SmartCardEmulation.reportConnectResult", params,
                 std::move(response));
  }

  // Helper: Waits for getStatusChangeRequested and sends a simulated response.
  void HandleGetStatusChange(
      const std::vector<TestReaderStateOut>& states,
      int context_id = 123,
      std::optional<std::string> error_code = std::nullopt) {
    auto params = WaitForNotificationParams(
        "SmartCardEmulation.getStatusChangeRequested");

    EXPECT_EQ(params.FindInt("contextId"), context_id);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
      return;
    }
    base::ListValue response_list;
    for (const auto& state : states) {
      base::DictValue dict;

      dict.Set("reader", state.name);
      dict.Set("eventCount", state.event_count);
      dict.Set("atr", base::Base64Encode(state.atr));

      base::DictValue flags;
      flags.Set("changed", state.changed);
      flags.Set("present", state.present);
      flags.Set("empty", state.empty);
      dict.Set("eventState", std::move(flags));

      response_list.Append(std::move(dict));
    }

    SendResponse(
        "SmartCardEmulation.reportGetStatusChangeResult", params,
        base::DictValue().Set("readerStates", std::move(response_list)));
  }

  // Helper: Waits for statusRequested and sends a response.
  void HandleStatus(const std::string& reader_name,
                    const std::string& state,
                    const std::string& protocol,
                    const std::vector<uint8_t>& atr,
                    int handle = 123,
                    std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.statusRequested");

    EXPECT_EQ(params.FindInt("handle"), handle);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
      return;
    }
    base::DictValue response;
    response.Set("readerName", reader_name);
    response.Set("state", state);
    response.Set("atr", base::Base64Encode(atr));
    if (!protocol.empty()) {
      response.Set("protocol", protocol);
    }
    SendResponse("SmartCardEmulation.reportStatusResult", params,
                 std::move(response));
  }

  // Helper: Waits for transmitRequested and sends a response.
  void HandleTransmit(
      const std::vector<uint8_t>& expected_command,
      const base::expected<std::vector<uint8_t>, std::string>& result,
      int handle = 123) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.transmitRequested");

    EXPECT_EQ(params.FindInt("handle"), handle);

    const std::string* command_b64 = params.FindString("data");
    ASSERT_TRUE(command_b64) << "Transmit event missing 'data' field";

    std::string command_str;
    base::Base64Decode(*command_b64, &command_str);
    std::vector<uint8_t> actual_command(command_str.begin(), command_str.end());
    EXPECT_EQ(actual_command, expected_command)
        << "JS sent wrong command bytes";

    if (result.has_value()) {
      SendResponse(
          "SmartCardEmulation.reportDataResult", params,
          base::DictValue().Set("data", base::Base64Encode(result.value())));
    } else {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", result.error()));
    }
  }

  // Helper: Waits for controlRequested and sends a response.
  void HandleControl(
      int expected_control_code,
      const std::vector<uint8_t>& expected_data,
      const base::expected<std::vector<uint8_t>, std::string>& result,
      int handle = 123) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.controlRequested");

    EXPECT_EQ(params.FindInt("handle"), handle);
    EXPECT_EQ(params.FindInt("controlCode"), expected_control_code);

    const std::string* data_b64 = params.FindString("data");
    ASSERT_TRUE(data_b64) << "Control event missing 'data' field";

    std::string data_str;
    base::Base64Decode(*data_b64, &data_str);
    std::vector<uint8_t> actual_data(data_str.begin(), data_str.end());
    EXPECT_EQ(actual_data, expected_data) << "JS sent wrong control data bytes";

    if (result.has_value()) {
      SendResponse(
          "SmartCardEmulation.reportDataResult", params,
          base::DictValue().Set("data", base::Base64Encode(result.value())));
    } else {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", result.error()));
    }
  }

  // Helper: Waits for getAttributeRequested and sends a response.
  void HandleGetAttribute(
      int attrib_id,
      const base::expected<std::vector<uint8_t>, std::string>& result,
      int handle = 123) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.getAttribRequested");

    EXPECT_EQ(params.FindInt("handle"), handle);
    EXPECT_EQ(params.FindInt("attribId"), attrib_id);

    if (result.has_value()) {
      SendResponse(
          "SmartCardEmulation.reportDataResult", params,
          base::DictValue().Set("data", base::Base64Encode(result.value())));
    } else {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", result.error()));
    }
  }

  // Helper: Waits for disconnectRequested and sends a response.
  void HandleDisconnect(const std::string& expected_disposition = "leave-card",
                        int handle = 123,
                        std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.disconnectRequested");

    EXPECT_EQ(params.FindInt("handle"), handle);
    EXPECT_EQ(*params.FindString("disposition"), expected_disposition);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
    } else {
      SendResponse("SmartCardEmulation.reportPlainResult", params, {});
    }
  }

  // Helper: Waits for beginTransactionRequested and sends a response.
  void HandleBeginTransaction(
      int handle = 123,
      std::optional<std::string> error_code = std::nullopt) {
    auto params = WaitForNotificationParams(
        "SmartCardEmulation.beginTransactionRequested");
    EXPECT_EQ(params.FindInt("handle"), handle);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
    } else {
      SendResponse("SmartCardEmulation.reportBeginTransactionResult", params,
                   base::DictValue().Set("handle", handle));
    }
  }

  // Helper: Waits for endTransactionRequested and sends a response.
  void HandleEndTransaction(
      const std::string& expected_disposition = "leave-card",
      int handle = 123,
      std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.endTransactionRequested");

    EXPECT_EQ(params.FindInt("handle"), handle);
    EXPECT_EQ(*params.FindString("disposition"), expected_disposition);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
    } else {
      SendResponse("SmartCardEmulation.reportPlainResult", params, {});
    }
  }

  // Helper: Waits for cancelRequested and sends a response.
  void HandleCancel(int context_id = 123,
                    std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.cancelRequested");
    EXPECT_EQ(params.FindInt("contextId"), context_id);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
    } else {
      SendResponse("SmartCardEmulation.reportPlainResult", params, {});
    }
  }

  // Helper: Waits for setAttribRequested (SCardSetAttrib)
  void HandleSetAttribute(
      int attrib_id,
      const std::vector<uint8_t>& expected_data,
      int handle = 123,
      std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.setAttribRequested");
    EXPECT_EQ(params.FindInt("handle"), handle);
    EXPECT_EQ(params.FindInt("attribId"), attrib_id);

    const std::string* data_b64 = params.FindString("data");
    ASSERT_TRUE(data_b64) << "Event is missing the 'data' field";

    std::string data_str;
    base::Base64Decode(*data_b64, &data_str);
    std::vector<uint8_t> actual_data(data_str.begin(), data_str.end());
    EXPECT_EQ(actual_data, expected_data);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
    } else {
      SendResponse("SmartCardEmulation.reportPlainResult", params, {});
    }
  }

  void HandleReleaseContext(
      int context_id = 123,
      std::optional<std::string> error_code = std::nullopt) {
    auto params =
        WaitForNotificationParams("SmartCardEmulation.releaseContextRequested");

    EXPECT_EQ(params.FindInt("contextId"), context_id);

    if (error_code.has_value()) {
      SendResponse("SmartCardEmulation.reportError", params,
                   base::DictValue().Set("resultCode", *error_code));
    } else {
      SendResponse("SmartCardEmulation.reportReleaseContextResult", params, {});
    }
  }

  void ReloadPage() {
    content::TestNavigationObserver observer(web_contents_);
    web_contents_->GetController().Reload(content::ReloadType::NORMAL, false);
    observer.Wait();
  }

  void NavigateToSubPage() {
    GURL current_url = web_contents_->GetLastCommittedURL();
    GURL new_url = current_url.Resolve("/target.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents_, new_url));
  }

 private:
  void InstallAndLaunchIWA() {
    auto app =
        IsolatedWebAppBuilder(
            ManifestBuilder().AddPermissionsPolicyWildcard(
                network::mojom::PermissionsPolicyFeature::kSmartCard))
            // Add a valid destination for navigation.
            .AddHtml("/target.html", "<html><body>Target Page</body></html>")
            .BuildBundle();

    app->TrustSigningKey();
    auto install_result = app->Install(profile());
    ASSERT_TRUE(install_result.has_value());

    auto url_info = install_result.value();
    raw_ptr<content::RenderFrameHost> app_frame = OpenApp(url_info.app_id());
    ASSERT_TRUE(app_frame);
    web_contents_ = content::WebContents::FromRenderFrameHost(app_frame);
    ASSERT_TRUE(web_contents_);
  }

  void AttachDevTools() {
    devtools_client_ = std::make_unique<content::TestDevToolsProtocolClient>();
    devtools_client_->AttachToWebContents(web_contents_);
  }

  void SetAutoAcceptPermissions() {
    permissions::PermissionRequestManager::FromWebContents(
        content::WebContents::FromRenderFrameHost(app_frame()))
        ->set_auto_response_for_test(
            permissions::PermissionRequestManager::ACCEPT_ALL);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::TestDevToolsProtocolClient> devtools_client_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, EnableDisableEmulation) {
  // Disable (Should be successful even if already disabled - Idempotency).
  EXPECT_THAT(SendCommand("SmartCardEmulation.disable"), IsSuccess());

  // Enable (Should activate the override).
  EXPECT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  // Re-Enable (Should be successful - Idempotency).
  EXPECT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  // Disable (Should clean up).
  EXPECT_THAT(SendCommand("SmartCardEmulation.disable"), IsSuccess());
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       ListReadersAndReportSuccess) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();

        if (readers.length === 2 &&
            readers.includes("Reader A") &&
            readers.includes("Reader B")) {
           window.domAutomationController.send("Success");
        } else {
           window.domAutomationController.send(
               "Failure: Unexpected list: " + JSON.stringify(readers));
        }
      } catch (e) {
        window.domAutomationController.send(
          "Error: " + e.name + " - " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders({"Reader A", "Reader B"});

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       ListReadersAndReportError) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();

        // This is expected to fail
        await context.listReaders();

        window.domAutomationController.send(
          "Failure: Should have thrown error");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.name);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders({}, 123, "no-service");

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Error: SmartCardError\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, ConnectAndReportSuccess) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const readerName = readers[0];
        const result = await context.connect(readerName, "shared",
            {preferredProtocols: ["t1"]});
        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send(
          "Error: " + e.name + " - " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect();

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, ConnectAndReportError) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const readerName = readers[0];

        // This connect attempt is expected to fail.
        await context.connect(readerName, "shared",
            {preferredProtocols: ["t1"]});

        if (result.activeProtocol === "t1") {
           window.domAutomationController.send("Success");
        } else {
           window.domAutomationController.send("Failure: Wrong protocol");
        }

        window.domAutomationController.send(
          "Failure: Should have thrown error");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.name);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1", "reader-unavailable");

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Error: SmartCardError\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       EmulationPersistsAcrossReload) {
  EXPECT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(web_contents());
  ReloadPage();

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send(
          "Error: " + e.name + " - " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       EmulationPersistsAcrossNavigation) {
  EXPECT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(web_contents());

  NavigateToSubPage();

  // Execute JS on the new page.
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.name);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       IframeCanAccessEmulatedReaders) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  // Inject Iframe
  const std::string kCreateIframeScript = R"(
      const iframe = document.createElement('iframe');
      iframe.src = 'about:blank';
      iframe.allow = 'smart-card';
      document.body.appendChild(iframe);
  )";
  EXPECT_TRUE(content::ExecJs(app_frame(), kCreateIframeScript));

  content::RenderFrameHost* iframe_host = content::ChildFrameAt(app_frame(), 0);
  ASSERT_TRUE(iframe_host);

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.name);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, GetStatusChangePnP) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const oldReaders = await context.listReaders();

        let allReaders = await context.getStatusChange(
            [{readerName: "\\\\?PnP?\\Notification", currentState: {}}],
            {timeout: 10000})
            .then((statesOut) => {
                return context.listReaders();
            });

        let newReaders = allReaders.filter(x => !oldReaders.includes(x));
        if (newReaders.length === 1 && newReaders[0] === "New Reader") {
           window.domAutomationController.send("Success");
        } else {
           window.domAutomationController.send(
            "Failure: " + JSON.stringify(newReaders));
        }
      } catch (e) {
        window.domAutomationController.send(
          "Error: " + e.name + " - " + e.message);
      }
    })();
  )";

  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders({});
  HandleGetStatusChange(
      {{.name = "\\\\?PnP?\\Notification", .event_count = 1, .changed = true}});
  HandleListReaders({"New Reader"});

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       GetStatusChangeReturnsError) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();

        await context.getStatusChange(
            [{readerName: "Simulated Reader", currentState: {}}],
            {timeout: 10000}
        );

        window.domAutomationController.send(
          "Failure: Promise resolved but should have rejected.");

      } catch (e) {
        if (e.name === "SmartCardError") {
           window.domAutomationController.send("Success");
        } else {
           window.domAutomationController.send(
            "Failure: Wrong error type - " + e.name + ": " + e.message);
        }
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders({"Simulated Reader"});
  HandleGetStatusChange({}, 123, "no-service");

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       GetStatusChangeMultiReader) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        if (readers.length !== 2 || !readers.includes("Reader A") ||
          !readers.includes("Reader B")) {
            window.domAutomationController.send(
              "Failure: Readers missing. Got: " + readers);
            return;
        }

        const states = await context.getStatusChange([
            {readerName: "Reader A", currentState: {empty: true}},
            {readerName: "Reader B", currentState: {empty: true}}
        ], {timeout: 10000});

        const stateA = states.find(s => s.readerName === "Reader A");
        const stateB = states.find(s => s.readerName === "Reader B");

        if (!stateA || !stateB) {
            throw new Error("Missing state for one or more readers.");
        }

        if (!stateA.eventState.changed || !stateA.eventState.present) {
             throw new Error(
              "Reader A failed: Expected changed/present. Got: " +
                JSON.stringify(stateA));
        }

        if (stateB.eventState.changed || !stateB.eventState.empty) {
             throw new Error(
              "Reader B failed: Expected !changed/empty. Got: " +
                JSON.stringify(stateB));
        }
        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send(
          "Error: " + e.name + " - " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders({"Reader A", "Reader B"});
  HandleGetStatusChange({{.name = "Reader A",

                          .event_count = 1,
                          .atr = {0x3B, 0x11},
                          .changed = true,
                          .present = true},
                         {.name = "Reader B",
                          .event_count = 0,
                          .atr = {},
                          .changed = false,
                          .empty = true}});

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, ConnectionStatus) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0], "shared",
            {preferredProtocols: ["t1"]});

        const status = await connectResult.connection.status();

        if (status.readerName !== "Reader A") {
            throw new Error("Wrong readerName: " + status.readerName);
        }

        if (status.state !== "t1") {
            throw new Error("Wrong state: " + status.state);
        }

        const atrBytes = new Uint8Array(status.answerToReset);
        if (atrBytes.length !== 4 || atrBytes[0] !== 0x3B) {
            throw new Error("Wrong ATR bytes: " + atrBytes);
        }

        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Error: " +
          e.name + " - " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect();

  HandleStatus("Reader A", "specific", "t1", {0x3B, 0x01, 0x02, 0x03});

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, ConnectionStatusError) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());
  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0], "shared",
            {preferredProtocols: ["t1"]});
            await connectResult.connection.status();

            window.domAutomationController.send(
              "Failure: Promise resolved but should have rejected.");
        } catch (e) {
            if (e.name === "SmartCardError") {
                 window.domAutomationController.send("Success");
            } else {
                 window.domAutomationController.send(
                  "Failure: Wrong error type. Got: " +
                    e.name + ", " + e.message);
            }
        }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect();

  HandleStatus("", "", "", {}, 123, "reader-unavailable");

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, DataOperationsSuccess) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0],
          "shared", {preferredProtocols: ["t1"]});
        const connection = connectResult.connection;

        // --- Transmit ---
        const txResponse = await connection.transmit(
          new Uint8Array([0x00, 0xA4, 0x04, 0x00]));
        const txBytes = new Uint8Array(txResponse);
        if (txBytes.length !== 2 || txBytes[0] !== 0x90 ||
          txBytes[1] !== 0x00) {
            throw new Error("Transmit failed. Got: " + txBytes.join(','));
        }

        // --- Control ---
        const ctrlResponse = await connection.control(42,
          new Uint8Array([1, 2, 3]));
        const ctrlBytes = new Uint8Array(ctrlResponse);
        if (ctrlBytes.length !== 2 || ctrlBytes[0] !== 10 ||
          ctrlBytes[1] !== 20) {
             throw new Error("Control failed. Got: " + ctrlBytes.join(','));
        }

        // --- GetAttribute ---
        // Tag 0x00090101 (SCARD_ATTR_VENDOR_NAME)
        const attrResponse = await connection.getAttribute(0x00090101);
        const vendorName = new TextDecoder().decode(attrResponse);
        if (vendorName !== "Chrome Reader") {
             throw new Error("GetAttribute failed. Got: " + vendorName);
        }

        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1");

  HandleTransmit({0x00, 0xA4, 0x04, 0x00}, std::vector<uint8_t>{0x90, 0x00});
  HandleControl(42, {1, 2, 3}, std::vector<uint8_t>{10, 20});

  std::string kName = "Chrome Reader";
  HandleGetAttribute(0x00090101,
                     std::vector<uint8_t>(kName.begin(), kName.end()));

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, DataOperationsErrors) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(
          readers[0], "shared", {preferredProtocols: ["t1"]});
        const connection = connectResult.connection;

        // --- Transmit Error ---
        try {
            await connection.transmit(new Uint8Array([0x00, 0x00]));
            throw new Error("Transmit should have failed but succeeded.");
        } catch (e) {
            if (e.name !== "SmartCardError") {
              throw new Error("Transmit wrong error: " + e.name);
            }
        }

        // --- Control Error ---
        try {
            await connection.control(42, new Uint8Array([1]));
            throw new Error("Control should have failed but succeeded.");
        } catch (e) {
            if (e.name !== "SmartCardError") {
              throw new Error("Control wrong error: " + e.name);
            }
        }

        // --- GetAttribute Error ---
        try {
            await connection.getAttribute(1234);
            throw new Error("GetAttribute should have failed but succeeded.");
        } catch (e) {
            if (e.name !== "SmartCardError") {
              throw new Error("GetAttrib wrong error: " + e.name);
            }
        }

        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Failure: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1");

  HandleTransmit({0x00, 0x00}, base::unexpected("sharing-violation"));
  HandleControl(42, {1}, base::unexpected("unsupported-feature"));
  HandleGetAttribute(1234, base::unexpected("reader-unavailable"));

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, TransactionSuccess) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0], "shared",
            {preferredProtocols: ["t1"]});
        const connection = connectResult.connection;

        await connection.startTransaction(async function() {
            return "leave";
        });
        await connection.disconnect();
        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1");
  HandleBeginTransaction();
  HandleEndTransaction();
  HandleDisconnect();

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, BeginTransactionFailure) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0], "shared",
            {preferredProtocols: ["t1"]});
        const connection = connectResult.connection;

        try {
            // Attempt to start a transaction - this should fail immediately.
            await connection.startTransaction(async function() {
                throw new Error(
                  "Transaction callback should not run on failure!");
            });
            throw new Error("Transaction succeeded but was expected to fail.");
        } catch (txError) {
            if (txError.name === "SmartCardError") {
                window.domAutomationController.send("Success");
            } else {
                throw new Error("Caught unexpected error type: " +
                  txError.name + " - " + txError.message);
            }
        }
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1");

  HandleBeginTransaction(123, "sharing-violation");

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, EndTransactionFailure) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0], "shared",
            {preferredProtocols: ["t1"]});
        const connection = connectResult.connection;

        try {
            await connection.startTransaction(async function() {
                return "leave";
            });
            throw new Error("Transaction succeeded but was expected to fail.");
        } catch (e) {
             if (e.message.includes("communications error")) {
                 window.domAutomationController.send("Success");
             } else {
                 throw new Error("Unexpected error: " +
                  e.name + " - " + e.message);
             }
        }
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1");

  HandleBeginTransaction();
  HandleEndTransaction("leave-card", 123, "comm-error");

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, SetAttributeSuccess) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0],
          "shared", {preferredProtocols: ["t1"]});
        const connection = connectResult.connection;
        await connection.setAttribute(1234, new Uint8Array([0x96, 0x00]));

        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1");

  HandleSetAttribute(1234, {0x96, 0x00});

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, SetAttributeFailure) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const readers = await context.listReaders();
        const connectResult = await context.connect(readers[0],
          "shared", {preferredProtocols: ["t1"]});
        const connection = connectResult.connection;

        try {
          await connection.setAttribute(1234, new Uint8Array([0x96, 0x00]));
          throw new Error("setAttribute succeeded but was expected to fail.");
        } catch (e) {
           if (e.name === "SmartCardError" ) {
             window.domAutomationController.send("Success");
           } else {
             throw new Error(
              "Caught unexpected error: " + e.name + " - " + e.message);
           }
        }
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();
  HandleListReaders();
  HandleConnect("Reader A", 123, "t1");

  HandleSetAttribute(1234, {0x96, 0x00}, 123, "sharing-violation");

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, CancelGetStatusChange) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const controller = new AbortController();
        const signal = controller.signal;
        const statusPromise = context.getStatusChange(
            [{
               readerName: "Simulated Reader",
               currentState: { empty: true }
            }],
            {signal: signal}
        );
        controller.abort();

        try {
            await statusPromise;
            throw new Error(
              "GetStatusChange should have been aborted but succeeded.");
        } catch (e) {
            if (e.name === 'AbortError') {
                 window.domAutomationController.send("Success");
            } else {
                 throw e;
            }
        }
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();

  // Intercept GetStatusChange but do not answer yet.
  auto status_params =
      WaitForNotificationParams("SmartCardEmulation.getStatusChangeRequested");

  HandleCancel();

  base::DictValue response;
  response.Set("resultCode", "cancelled");
  SendResponse("SmartCardEmulation.reportError", status_params,
               std::move(response));

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       CancelFailureOperationContinues) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        const controller = new AbortController();

        const statusPromise = context.getStatusChange(
            [{readerName: "Simulated Reader", currentState: {empty: true}}],
            {signal: controller.signal}
        );

        controller.abort();
        const result = await statusPromise;
        console.log(result);
        if (result.length === 1 && result[0].readerName ===  "Reader A") {
            window.domAutomationController.send("Success");
        } else {
            throw new Error("Operation finished but returned wrong data.");
        }
      } catch (e) {
        window.domAutomationController.send(
          "Error: " + e.name + " - " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();

  auto status_params =
      WaitForNotificationParams("SmartCardEmulation.getStatusChangeRequested");

  HandleCancel(123, "comm-error");

  base::DictValue response;
  base::DictValue state;
  base::ListValue response_list;

  state.Set("reader", "Reader A");
  state.Set("eventCount", 1);
  state.Set("atr", base::Base64Encode({0x3B, 0x11}));

  base::DictValue flags;
  flags.Set("changed", true);

  state.Set("eventState", std::move(flags));

  response_list.Append(std::move(state));

  response.Set("readerStates", std::move(response_list));
  SendResponse("SmartCardEmulation.reportGetStatusChangeResult", status_params,
               std::move(response));

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, ReleaseContextOnGC) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  ASSERT_THAT(SendCommand("HeapProfiler.enable"), IsSuccess());

  content::DOMMessageQueue message_queue(app_frame());

  const std::string kScript = R"(
    (async () => {
      try {
        const context = await navigator.smartCard.establishContext();
        window.domAutomationController.send("Success");
      } catch (e) {
        window.domAutomationController.send("Error: " + e.message);
      }
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kScript);

  HandleEstablishContext();

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);

  ASSERT_THAT(SendCommand("HeapProfiler.collectGarbage"), IsSuccess());
  HandleReleaseContext();
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest,
                       ReleaseContextOnNavigation) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kSetupScript = R"(
    (async () => {
      window.smartCardContext = await navigator.smartCard.establishContext();
      window.domAutomationController.send("Success");
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kSetupScript);

  HandleEstablishContext(123);

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);

  content::ExecuteScriptAsync(app_frame(), "window.location.reload();");
  HandleReleaseContext();
}

IN_PROC_BROWSER_TEST_F(SmartCardEmulationBrowserTest, ReleaseContextFailure) {
  ASSERT_THAT(SendCommand("SmartCardEmulation.enable"), IsSuccess());
  content::DOMMessageQueue message_queue(app_frame());

  const std::string kSetupScript = R"(
    (async () => {
      window.smartCardContext = await navigator.smartCard.establishContext();
      window.domAutomationController.send("Success");
    })();
  )";
  content::ExecuteScriptAsync(app_frame(), kSetupScript);

  HandleEstablishContext(123);

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("\"Success\"", message);

  content::ExecuteScriptAsync(app_frame(), "window.location.reload();");

  HandleReleaseContext(123, "internal-error");
}

}  // namespace web_app
