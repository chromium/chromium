// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>
#include <type_traits>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

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
constexpr char kPrivateAddress[] = "10.8.0.1";

constexpr std::string_view kTcpReadWriteScript = R"(
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

constexpr std::string_view kUdpConnectedReadWriteScript = R"(
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

constexpr std::string_view kUdpBoundReadWriteScript = R"(
  (async () => {
    try {
      const socket = new UDPSocket({ localAddress: "127.0.0.1" });
      const { readable, writable } = await socket.opened;

      const kUdpMessage = "udp_message";
      writable.getWriter().write({
        data: (new TextEncoder()).encode(kUdpMessage),
        remoteAddress: $1,
        remotePort: $2,
      });
      return await readable.getReader().read().then(packet => {
        const { value, done } = packet;
        if (done) {
          return false;
        }
        const { data, remoteAddress, remotePort } = value;
        if ((new TextDecoder()).decode(data) !== kUdpMessage) {
          return false;
        }
        if (remoteAddress !== "127.0.0.1") {
          return false;
        }
        if (remotePort !== $2) {
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

static constexpr std::string_view kTcpServerExchangePacketWithTcpScript = R"(
  (async () => {
    const assertEq = (actual, expected) => {
      const jf = e => JSON.stringify(e);
      if (actual !== expected) {
        throw `Expected ${jf(expected)}, got ${jf(actual)}`;
      }
    };

    const kPacket = "I'm a netcat. Meow-meow!";

    // |localPort| is intentionally omitted so that the OS will pick one itself.
    const serverSocket = new TCPServerSocket('127.0.0.1');
    const { localPort: serverSocketPort } = await serverSocket.opened;

    // Connect a client to the server.
    const clientSocket = new TCPSocket('127.0.0.1', serverSocketPort);

    async function acceptOnce() {
      const { readable } = await serverSocket.opened;
      const reader = readable.getReader();
      const { value: acceptedSocket, done } = await reader.read();
      assertEq(done, false);
      reader.releaseLock();
      return acceptedSocket;
    };

    const acceptedSocket = await acceptOnce();
    await clientSocket.opened;

    const encoder = new TextEncoder();
    const decoder = new TextDecoder();

    async function acceptedSocketSend() {
      const { writable } = await acceptedSocket.opened;
      const writer = writable.getWriter();

      await writer.ready;
      await writer.write(encoder.encode(kPacket));

      writer.releaseLock();
    }

    async function clientSocketReceive() {
      const { readable } = await clientSocket.opened;
      const reader = readable.getReader();
      let result = "";
      while (result.length < kPacket.length) {
        const { value, done } = await reader.read();
        assertEq(done, false);
        result += decoder.decode(value);
      }
      reader.releaseLock();
      assertEq(result, kPacket);
    }

    acceptedSocketSend();
    await clientSocketReceive();

    await clientSocket.close();
    await acceptedSocket.close();
    await serverSocket.close();
  })();
)";

#if BUILDFLAG(ENABLE_EXTENSIONS)

base::Value::Dict GenerateManifest(
    std::optional<base::Value::Dict> socket_permissions = {}) {
  auto manifest = base::Value::Dict()
                      .Set(extensions::manifest_keys::kName,
                           "Direct Sockets in Chrome Apps")
                      .Set(extensions::manifest_keys::kManifestVersion, 2)
                      .Set(extensions::manifest_keys::kVersion, "1.0");

  manifest.SetByDottedPath(
      extensions::manifest_keys::kPlatformAppBackgroundScripts,
      base::Value::List().Append("background.js"));

  if (socket_permissions) {
    manifest.Set(extensions::manifest_keys::kSockets,
                 std::move(*socket_permissions));
  }

  return manifest;
}

auto AccessBlocked() {
  return testing::HasSubstr("Access to the requested host or port is blocked");
}

auto PrivateNetworkAccessBlocked() {
  return testing::HasSubstr("Access to private network is blocked");
}

auto ErrorIs(const auto& matcher) {
  return testing::Field(&content::EvalJsResult::error, matcher);
}

#endif

class TestServer {
 public:
  virtual ~TestServer() = default;

  virtual void Start(network::mojom::NetworkContext* network_context) = 0;
  virtual void Stop() = 0;
  virtual uint16_t port() const = 0;
};

class TcpHttpTestServer : public TestServer {
 public:
  void Start(network::mojom::NetworkContext* network_context) override {
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
  void Start(network::mojom::NetworkContext* network_context) override {
    DCHECK(!udp_echo_server_);
    udp_echo_server_ = std::make_unique<extensions::TestUdpEchoServer>();
    net::HostPortPair host_port_pair;
    ASSERT_TRUE(udp_echo_server_->Start(network_context, &host_port_pair));

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
  std::optional<uint16_t> port_;
};

template <typename TestHarness>
  requires(std::is_base_of_v<InProcessBrowserTest, TestHarness>)
class ChromeDirectSocketsTest : public TestHarness {
 public:
  ChromeDirectSocketsTest() = delete;

  void SetUpOnMainThread() override {
    TestHarness::SetUpOnMainThread();
    TestHarness::host_resolver()->AddRule(kHostname, "127.0.0.1");
    test_server()->Start(InProcessBrowserTest::browser()
                             ->profile()
                             ->GetDefaultStoragePartition()
                             ->GetNetworkContext());
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

class ChromeAppApiTest : public extensions::ExtensionApiTest {
 public:
  content::RenderFrameHost* InstallAndOpenChromeApp(
      const base::Value::Dict& manifest) {
    dir.WriteManifest(manifest);
    dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");

    const extensions::Extension& extension =
        CHECK_DEREF(LoadExtension(dir.UnpackedPath()));
    return CHECK_DEREF(extensions::ProcessManager::Get(profile())
                           ->GetBackgroundHostForExtension(extension.id()))
        .main_frame_host();
  }

 private:
  extensions::TestExtensionDir dir;
};

using ChromeDirectSocketsTcpApiTest =
    ChromeDirectSocketsTcpTest<ChromeAppApiTest>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest, TcpReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/
                       base::Value::Dict().Set(
                           "tcp", base::Value::Dict().Set("connect", "*"))));

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kTcpReadWriteScript, kHostname,
                                           test_server()->port()))
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest,
                       TcpSocketUndefinedWithoutSocketsPermission) {
  // "sockets" key is not present in the manifest.
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest());

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new TCPSocket($1, $2);
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, content::JsReplace(kScript, kHostname, 0)),
              ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest,
                       TcpFailsWithoutSocketsTcpConnectPermission) {
  // "sockets" key is present in the manifest, but "sockets.tcp.connect" is not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::Value::Dict()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new TCPSocket($1, $2);
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, content::JsReplace(kScript, kHostname,
                                                   test_server()->port())),
              ErrorIs(AccessBlocked()));
}

using ChromeDirectSocketsUdpApiTest =
    ChromeDirectSocketsUdpTest<ChromeAppApiTest>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest, UdpReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::Value::Dict().Set(
          "udp", base::Value::Dict().Set("send", "*"))));

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kUdpConnectedReadWriteScript,
                                           kHostname, test_server()->port()))
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpSocketUndefinedWithoutSocketsPermission) {
  // "sockets" key is not present in the manifest.
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest());

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new UDPSocket({ remoteAddress: $1, remotePort: $2 });
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, content::JsReplace(kScript, kHostname, 0)),
              ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpConnectedFailsWithoutSocketsUdpSendPermission) {
  // "sockets" key is present in the manifest, but "sockets.udp.send" is
  // not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::Value::Dict()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new UDPSocket({ remoteAddress: $1, remotePort: $2 });
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, content::JsReplace(kScript, kHostname,
                                                   test_server()->port())),
              ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpBoundFailsWithoutSocketsUdpBindPermission) {
  // "sockets" key is present in the manifest as well as "sockets.udp.send",
  // but "sockets.udp.bind" is not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::Value::Dict()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "::" });
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, kScript), ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest, UdpServerReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::Value::Dict().Set(
          "udp", base::Value::Dict().Set("bind", "*").Set("send", "*"))));

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kUdpBoundReadWriteScript, kHostname,
                                           test_server()->port()))
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpServerNotAffectedByPNAContentSettingInChromeApps) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::Value::Dict().Set(
          "udp", base::Value::Dict().Set("bind", "*").Set("send", "*"))));

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(
          ContentSettingsType::DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS,
          ContentSetting::CONTENT_SETTING_BLOCK);

  constexpr std::string_view kUdpBoundPna = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "0.0.0.0" });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, kUdpBoundPna), content::EvalJsResult::IsOk());
}

using ChromeDirectSocketsTcpServerApiTest = ChromeAppApiTest;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerApiTest,
                       TcpServerSocketUndefinedWithoutSocketsPermission) {
  // "sockets" key is not present in the manifest.
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest());

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new TCPServerSocket("::");
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, kScript), ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerApiTest,
                       TcpServerFailsWithoutSocketsTcpServerListenPermission) {
  // "sockets" key is present in the manifest, but "sockets.tcpServer.listen" is
  // not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::Value::Dict()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new TCPServerSocket("::");
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, kScript), ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerApiTest,
                       TcpServerExchangePacketWithTcpSocket) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest(
          /*socket_permissions=*/base::Value::Dict()
              .Set("tcpServer", base::Value::Dict().Set("listen", "*"))
              .Set("tcp", base::Value::Dict().Set("connect", "*"))));

  EXPECT_THAT(EvalJs(app_frame, kTcpServerExchangePacketWithTcpScript),
              content::EvalJsResult::IsOk());
}

#endif

class IsolatedWebAppApiTest : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  content::RenderFrameHost* InstallAndOpenIsolatedWebApp(
      bool with_pna = false) {
    using PermissionsPolicyFeature = blink::mojom::PermissionsPolicyFeature;

    auto manifest_builder =
        web_app::ManifestBuilder().AddPermissionsPolicyWildcard(
            PermissionsPolicyFeature::kDirectSockets);
    if (with_pna) {
      manifest_builder.AddPermissionsPolicyWildcard(
          PermissionsPolicyFeature::kDirectSocketsPrivate);
    }
    auto app = web_app::IsolatedWebAppBuilder(std::move(manifest_builder))
                   .BuildBundle();
    app->TrustSigningKey();
    web_app::IsolatedWebAppUrlInfo url_info = app->Install(profile()).value();
    return OpenApp(url_info.app_id());
  }

 private:
  std::unique_ptr<web_app::ScopedBundledIsolatedWebApp> app_;
};

using ChromeDirectSocketsTcpIsolatedWebAppTest =
    ChromeDirectSocketsTcpTest<IsolatedWebAppApiTest>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpIsolatedWebAppTest, TcpReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenIsolatedWebApp();

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kTcpReadWriteScript, kHostname,
                                           test_server()->port()))
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpIsolatedWebAppTest,
                       TcpConnectionToPrivateFailsWithoutPNAPermission) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/false);

  constexpr std::string_view kTcpPna = R"(
    (async () => {
      const socket = new TCPSocket($1, 459);
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, content::JsReplace(kTcpPna, kPrivateAddress)),
              ErrorIs(PrivateNetworkAccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpIsolatedWebAppTest,
                       TcpConnectionToPrivateFailsWithoutPNAContentSetting) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/true);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(
          ContentSettingsType::DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS,
          ContentSetting::CONTENT_SETTING_BLOCK);

  constexpr std::string_view kTcpPna = R"(
    (async () => {
      const socket = new TCPSocket($1, 459);
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, content::JsReplace(kTcpPna, kPrivateAddress)),
              ErrorIs(PrivateNetworkAccessBlocked()));
}

using ChromeDirectSocketsUdpIsolatedWebAppTest =
    ChromeDirectSocketsUdpTest<IsolatedWebAppApiTest>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest, UdpReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenIsolatedWebApp();

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kUdpConnectedReadWriteScript,
                                           kHostname, test_server()->port()))
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest,
                       UdpConnectionToPrivateFailsWithoutPNAPermission) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/false);

  constexpr std::string_view kUdpPna = R"(
    (async () => {
      const socket = new UDPSocket({ remoteAddress: $1, remotePort: 459 });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, content::JsReplace(kUdpPna, kPrivateAddress)),
              ErrorIs(PrivateNetworkAccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest,
                       UdpConnectionToPrivateFailsWithoutPNAContentSetting) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/true);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(
          ContentSettingsType::DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS,
          ContentSetting::CONTENT_SETTING_BLOCK);

  constexpr std::string_view kUdpPna = R"(
    (async () => {
      const socket = new UDPSocket({ remoteAddress: $1, remotePort: 459 });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, content::JsReplace(kUdpPna, kPrivateAddress)),
              ErrorIs(PrivateNetworkAccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest,
                       UdpServerReadWrite) {
  // UDP Bound Mode requires direct-sockets-private permissions policy.
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/true);

  ASSERT_TRUE(
      EvalJs(app_frame, content::JsReplace(kUdpBoundReadWriteScript, kHostname,
                                           test_server()->port()))
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest,
                       UdpServerFailsWithoutPNAPermission) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/false);

  constexpr std::string_view kUdpBoundPna = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "0.0.0.0" });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, kUdpBoundPna),
              ErrorIs(PrivateNetworkAccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest,
                       UdpServerFailsWithoutPNAContentSetting) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/true);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(
          ContentSettingsType::DIRECT_SOCKETS_PRIVATE_NETWORK_ACCESS,
          ContentSetting::CONTENT_SETTING_BLOCK);

  constexpr std::string_view kUdpBoundPna = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "0.0.0.0" });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, kUdpBoundPna),
              ErrorIs(PrivateNetworkAccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpIsolatedWebAppTest,
                       UdpServerPortRestrictions) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/true);

  constexpr std::string_view kUdpBoundPortBelow1024 = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "0.0.0.0", localPort: 455 });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, kUdpBoundPortBelow1024),
              ErrorIs(AccessBlocked()));

  constexpr std::string_view kUdpBoundPortNumberHighEnough = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "0.0.0.0", localPort: 2558 });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, kUdpBoundPortNumberHighEnough),
              content::EvalJsResult::IsOk());
}

using ChromeDirectSocketsTcpServerIsolatedWebAppTest = IsolatedWebAppApiTest;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerIsolatedWebAppTest,
                       TcpServerExchangePacketWithTcpSocket) {
  content::RenderFrameHost* app_frame = InstallAndOpenIsolatedWebApp();

  EXPECT_THAT(EvalJs(app_frame, kTcpServerExchangePacketWithTcpScript),
              content::EvalJsResult::IsOk());
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerIsolatedWebAppTest,
                       TcpServerPortRestrictions) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenIsolatedWebApp(/*with_pna=*/true);

  constexpr std::string_view kTcpServerPortBelow32678 = R"(
    (async () => {
      const socket = new TCPServerSocket("0.0.0.0", { localPort: 7845 });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, kTcpServerPortBelow32678),
              ErrorIs(AccessBlocked()));

  constexpr std::string_view kTcpServerPortNumberHighEnough = R"(
    (async () => {
      const socket = new TCPServerSocket("0.0.0.0", { localPort: 35588 });
      await socket.opened;
    })();
  )";

  ASSERT_THAT(EvalJs(app_frame, kTcpServerPortNumberHighEnough),
              content::EvalJsResult::IsOk());
}

}  // namespace
