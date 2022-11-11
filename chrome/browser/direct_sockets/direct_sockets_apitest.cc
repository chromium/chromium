// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

constexpr char kHostname[] = "direct-sockets.com";

#if BUILDFLAG(ENABLE_EXTENSIONS)

std::string GenerateManifest(
    absl::optional<base::Value::Dict> socket_permissions = {}) {
  base::Value::Dict manifest;
  manifest.Set(extensions::manifest_keys::kName,
               "Direct Sockets in Chrome Apps");
  manifest.Set(extensions::manifest_keys::kManifestVersion, 2);
  manifest.Set(extensions::manifest_keys::kVersion, "1.0");

  base::Value::List scripts;
  scripts.Append("background.js");
  manifest.SetByDottedPath(
      extensions::manifest_keys::kPlatformAppBackgroundScripts,
      std::move(scripts));

  if (socket_permissions) {
    manifest.Set(extensions::manifest_keys::kSockets,
                 std::move(*socket_permissions));
  }

  std::string out;
  base::JSONWriter::Write(manifest, &out);

  return out;
}

#endif

class TestServer {
 public:
  virtual ~TestServer() = default;

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual uint16_t port() const = 0;
};

class TcpHttpTestServer : public TestServer {
 public:
  void Start() override {
    DCHECK(!test_server_);
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    test_server_->AddDefaultHandlers();
    ASSERT_TRUE(test_server_->Start());
  }

  void Stop() override { test_server_.reset(); }

  uint16_t port() const override {
    DCHECK(test_server_);
    return test_server_->port();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

class UdpEchoTestServer : public TestServer {
 public:
  void Start() override {
    DCHECK(!udp_echo_server_);
    udp_echo_server_ = std::make_unique<extensions::TestUdpEchoServer>();
    net::HostPortPair host_port_pair;
    ASSERT_TRUE(udp_echo_server_->Start(&host_port_pair));

    port_ = host_port_pair.port();
    ASSERT_GT(*port_, 0);
  }

  void Stop() override { udp_echo_server_.reset(); }

  uint16_t port() const override {
    DCHECK(port_);
    return *port_;
  }

 private:
  std::unique_ptr<extensions::TestUdpEchoServer> udp_echo_server_;
  absl::optional<uint16_t> port_;
};

template <typename TestHarness,
          typename = std::enable_if_t<
              std::is_base_of_v<content::BrowserTestBase, TestHarness>>>
class ChromeDirectSocketsTest : public TestHarness {
 public:
  ChromeDirectSocketsTest() = delete;

  void SetUpOnMainThread() override {
    TestHarness::SetUpOnMainThread();
    TestHarness::host_resolver()->AddRule(kHostname, "127.0.0.1");
    test_server()->Start();
  }

  void TearDownOnMainThread() override {
    TestHarness::TearDownOnMainThread();
    test_server()->Stop();
  }

 protected:
  explicit ChromeDirectSocketsTest(std::unique_ptr<TestServer> test_server)
      : test_server_{std::move(test_server)} {}
  TestServer* test_server() const {
    DCHECK(test_server_);
    return test_server_.get();
  }

 private:
  std::unique_ptr<TestServer> test_server_;
};

template <typename TestHarness>
class ChromeDirectSocketsTcpTest : public ChromeDirectSocketsTest<TestHarness> {
 public:
  ChromeDirectSocketsTcpTest()
      : ChromeDirectSocketsTest<TestHarness>{
            std::make_unique<TcpHttpTestServer>()} {}
};

template <typename TestHarness>
class ChromeDirectSocketsUdpTest : public ChromeDirectSocketsTest<TestHarness> {
 public:
  ChromeDirectSocketsUdpTest()
      : ChromeDirectSocketsTest<TestHarness>{
            std::make_unique<UdpEchoTestServer>()} {}
};

#if BUILDFLAG(ENABLE_EXTENSIONS)

class ExtensionApiTestWithDirectSocketsEnabled
    : public extensions::ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "DirectSockets");
  }
};

using ChromeDirectSocketsTcpApiTest =
    ChromeDirectSocketsTcpTest<ExtensionApiTestWithDirectSocketsEnabled>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest, TcpReadWrite) {
  extensions::TestExtensionDir dir;

  base::Value::Dict socket_permissions;
  socket_permissions.SetByDottedPath("tcp.connect", "*");

  dir.WriteManifest(GenerateManifest(std::move(socket_permissions)));
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.test.sendMessage("ready", async (message) => {
      try {
        const [remoteAddress, remotePort] = message.split(':');
        const socket = new TCPSocket(remoteAddress, remotePort);

        const { readable, writable } = await socket.opened;

        const reader = readable.getReader();
        const writer = writable.getWriter();

        const kTcpPacket =
          "POST /echo HTTP/1.1\r\n" +
          "Content-Length: 19\r\n\r\n" +
          "0100000005320000005";

        // The echo server can send back the response in multiple chunks.
        // We must wait for at least `kTcpMinExpectedResponseLength` bytes to
        // be received before matching the response with `kTcpResponsePattern`.
        const kTcpMinExpectedResponseLength = 102;

        const kTcpResponsePattern = "0100000005320000005";

        let tcpResponse = "";
        const readUntil = async () => {
          reader.read().then(packet => {
            const { value, done } = packet;
            chrome.test.assertFalse(done,
                "ReadableStream must not be exhausted at this point.");

            const message = (new TextDecoder()).decode(value);
            tcpResponse += message;
            if (tcpResponse.length >= kTcpMinExpectedResponseLength) {
              chrome.test.assertTrue(
                !!tcpResponse.match(kTcpResponsePattern),
                "The data returned must match the data sent."
              );

              chrome.test.succeed();
            } else {
              readUntil();
            }
          });
        };

        readUntil();

        writer.write((new TextEncoder()).encode(kTcpPacket));
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )");

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply(base::StringPrintf("%s:%d", kHostname, test_server()->port()));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest,
                       TcpFailsWithoutSocketsPermission) {
  extensions::TestExtensionDir dir;

  dir.WriteManifest(GenerateManifest());
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.test.sendMessage("ready", async (message) => {
      try {
        const [remoteAddress, remotePort] = message.split(':');
        const socket = new TCPSocket(remoteAddress, remotePort);

        await chrome.test.assertPromiseRejects(
          socket.opened,
          "InvalidAccessError: Access to the requested host is blocked."
        );

        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )");

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(dir.UnpackedPath()));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply(base::StringPrintf("%s:%d", kHostname, 0));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

using ChromeDirectSocketsUdpApiTest =
    ChromeDirectSocketsUdpTest<ExtensionApiTestWithDirectSocketsEnabled>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest, UdpReadWrite) {
  extensions::TestExtensionDir dir;

  base::Value::Dict socket_permissions;
  socket_permissions.SetByDottedPath("udp.send", "*");

  dir.WriteManifest(GenerateManifest(std::move(socket_permissions)));
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.test.sendMessage("ready", async (message) => {
      try {
        const [remoteAddress, remotePort] = message.split(':');
        const socket = new UDPSocket({ remoteAddress, remotePort });

        const { readable, writable } = await socket.opened;

        const reader = readable.getReader();
        const writer = writable.getWriter();

        const kUdpMessage = "udp_message";

        reader.read().then(packet => {
          const { value, done } = packet;
          chrome.test.assertFalse(done,
              "ReadableStream must not be exhausted at this point.");

          const { data } = value;
          chrome.test.assertEq((new TextDecoder()).decode(data), kUdpMessage,
              "The data returned must exactly match the data sent.");

          chrome.test.succeed();
        });

        writer.write({
          data: (new TextEncoder()).encode(kUdpMessage)
        });
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )");

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply(base::StringPrintf("%s:%d", kHostname, test_server()->port()));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpFailsWithoutSocketsPermission) {
  extensions::TestExtensionDir dir;

  dir.WriteManifest(GenerateManifest());
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.test.sendMessage("ready", async (message) => {
      try {
        const [remoteAddress, remotePort] = message.split(':');
        const socket = new UDPSocket({ remoteAddress, remotePort });

        await chrome.test.assertPromiseRejects(
          socket.opened,
          "InvalidAccessError: Access to the requested host is blocked."
        );

        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e.name + ':' + e.message);
      }
    });
  )");

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(dir.UnpackedPath()));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply(base::StringPrintf("%s:%d", kHostname, 0));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#endif

class IsolatedWebAppTestHarnessWithDirectSocketsEnabled
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppTestHarnessWithDirectSocketsEnabled() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using ChromeDirectSocketsTcpIsolatedWebAppTest = ChromeDirectSocketsTcpTest<
    IsolatedWebAppTestHarnessWithDirectSocketsEnabled>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpIsolatedWebAppTest, TcpReadWrite) {
  // Install & open the IWA.
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server->GetOrigin());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  // Run the echo script.
  constexpr base::StringPiece kTcpSendReceiveHttpScript = R"(
    (async () => {
      try {
        const socket = new TCPSocket($1, $2);

        const { readable, writable } = await socket.opened;

        const reader = readable.getReader();
        const writer = writable.getWriter();

        const kTcpPacket =
          "POST /echo HTTP/1.1\r\n" +
          "Content-Length: 19\r\n\r\n" +
          "0100000005320000005";

        // The echo server can send back the response in multiple chunks.
        // We must wait for at least `kTcpMinExpectedResponseLength` bytes to
        // be received before matching the response with `kTcpResponsePattern`.
        const kTcpMinExpectedResponseLength = 102;

        const kTcpResponsePattern = "0100000005320000005";

        let tcpResponse = "";
        const readUntil = async () => {
          const { value, done } = await reader.read();
          if (done) {
            console.log("ReadableStream must not be exhausted at this point.");
            return false;
          }

          const message = (new TextDecoder()).decode(value);
          tcpResponse += message;
          if (tcpResponse.length >= kTcpMinExpectedResponseLength) {
            if (!tcpResponse.match(kTcpResponsePattern)) {
              console.log("The data returned must match the data sent.");
              return false;
            }

            return true;
          } else {
            return await readUntil();
          }
        };

        writer.write((new TextEncoder()).encode(kTcpPacket));

        return await readUntil();
      } catch (err) {
        console.log(err);
        return false;
      }
    })();
  )";

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kTcpSendReceiveHttpScript, kHostname,
                                           test_server()->port()))
          .ExtractBool());
}

using ChromeDirectSocketsUdpIsolatedWebAppTest = ChromeDirectSocketsUdpTest<
    IsolatedWebAppTestHarnessWithDirectSocketsEnabled>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest, UdpReadWrite) {
  // Install & open the IWA.
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server =
      CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server->GetOrigin());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  // Run the echo script.
  constexpr base::StringPiece kUdpSendReceiveEchoScript = R"(
    (async () => {
      try {
        const socket = new UDPSocket({ remoteAddress: $1, remotePort: $2 });
        const { readable, writable } = await socket.opened;

        const kUdpMessage = "udp_message";
        writable.getWriter().write({
          data: (new TextEncoder()).encode(kUdpMessage)
        });
        return await readable.getReader().read().then(packet => {
          const { value, done } = packet;
          if (done) {
            return false;
          }
          const { data } = value;
          if ((new TextDecoder()).decode(data) !== kUdpMessage) {
            return false;
          }
          return true;
        });
      } catch (err) {
        console.log(err);
        return false;
      }
    })();
  )";

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kUdpSendReceiveEchoScript, kHostname,
                                           test_server()->port()))
          .ExtractBool());
}

}  // namespace
