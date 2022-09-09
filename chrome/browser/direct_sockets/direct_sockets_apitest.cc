// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/crx_file/id_util.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)

constexpr char kHostname[] = "www.foo.com";

std::string GenerateManifest(
    absl::optional<base::Value::Dict> socket_permissions = {}) {
  base::Value::Dict manifest;
  manifest.Set(extensions::manifest_keys::kName,
               "Direct Sockets in Chrome Extensions");
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

class DirectSocketsApiTest : public extensions::ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        ::switches::kIsolatedAppOrigins,
        extensions::Extension::CreateOriginFromExtensionId(
            GuessFutureExtensionId())
            .Serialize());
  }

 protected:
  extensions::TestExtensionDir& dir() { return dir_; }

 private:
  std::string GuessFutureExtensionId() {
    return crx_file::id_util::GenerateIdForPath(dir().UnpackedPath());
  }

  extensions::TestExtensionDir dir_;
};

using DirectSocketsTcpApiTest = DirectSocketsApiTest;

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpApiTest, TcpReadWrite) {
  base::Value::Dict socket_permissions;
  socket_permissions.SetByDottedPath("tcp.connect", "*");

  dir().WriteManifest(GenerateManifest(std::move(socket_permissions)));

  dir().WriteFile(FILE_PATH_LITERAL("background.js"), R"(
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

            tcpResponse += (new TextDecoder()).decode(value);
            if (tcpResponse.length >= kTcpMinExpectedResponseLength) {
              chrome.test.assertTrue(
                !!(new TextDecoder()).decode(value).match(kTcpResponsePattern),
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

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTP);
  test_server.AddDefaultHandlers();
  EXPECT_TRUE(test_server.Start());

  net::HostPortPair host_port_pair = test_server.host_port_pair();
  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  host_port_pair.set_host(kHostname);

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(dir().UnpackedPath()));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("%s:%d", host_port_pair.host().c_str(), port));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsTcpApiTest,
                       TcpFailsWithoutSocketsPermission) {
  dir().WriteManifest(GenerateManifest());

  dir().WriteFile(FILE_PATH_LITERAL("background.js"), R"(
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

  ASSERT_TRUE(LoadExtension(dir().UnpackedPath()));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply(base::StringPrintf("%s:%d", kHostname, 0));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

using DirectSocketsUdpApiTest = DirectSocketsApiTest;

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpApiTest, UdpReadWrite) {
  base::Value::Dict socket_permissions;
  socket_permissions.SetByDottedPath("udp.send", "*");

  dir().WriteManifest(GenerateManifest(std::move(socket_permissions)));

  dir().WriteFile(FILE_PATH_LITERAL("background.js"), R"(
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

  extensions::TestUdpEchoServer udp_echo_server;
  net::HostPortPair host_port_pair;
  ASSERT_TRUE(udp_echo_server.Start(&host_port_pair));

  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  host_port_pair.set_host(kHostname);

  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(dir().UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("%s:%d", host_port_pair.host().c_str(), port));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(DirectSocketsUdpApiTest,
                       UdpFailsWithoutSocketsPermission) {
  dir().WriteManifest(GenerateManifest());

  dir().WriteFile(FILE_PATH_LITERAL("background.js"), R"(
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

  ASSERT_TRUE(LoadExtension(dir().UnpackedPath()));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply(base::StringPrintf("%s:%d", kHostname, 0));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#endif

}  // namespace
