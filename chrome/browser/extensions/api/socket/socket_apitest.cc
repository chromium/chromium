// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/browser/api/socket/write_quota_checker.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

using extensions::Extension;
using extensions::ResultCatcher;

namespace {

const char kHostname[] = "www.foo.com";
const int kPort = 8888;

class SocketApiTest : public extensions::ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketUDPExtension) {
  extensions::TestUdpEchoServer udp_echo_server;
  net::HostPortPair host_port_pair;
  ASSERT_TRUE(udp_echo_server.Start(
      profile()->GetDefaultStoragePartition()->GetNetworkContext(),
      &host_port_pair));

  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  // Test that sendTo() is properly resolving hostnames.
  host_port_pair.set_host(kHostname);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("udp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Flaky on Windows. https://crbug.com/1319604.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SocketTCPExtension DISABLED_SocketTCPExtension
#else
#define MAYBE_SocketTCPExtension SocketTCPExtension
#endif
IN_PROC_BROWSER_TEST_F(SocketApiTest, MAYBE_SocketTCPExtension) {
  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTP);
  test_server.AddDefaultHandlers();
  EXPECT_TRUE(test_server.Start());

  net::HostPortPair host_port_pair = test_server.host_port_pair();
  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  // Test that connect() is properly resolving hostnames.
  host_port_pair.set_host("lOcAlHoSt");

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("tcp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketTCPServerExtension) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("tcp_server:127.0.0.1:%d", kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketTCPServerUnbindOnUnload) {
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("socket/unload"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  UnloadExtension(extension->id());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/unload")))
      << message_;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Fails on MacOS 11, crbug.com/1211141 .
#if BUILDFLAG(IS_MAC)
#define MAYBE_SocketMulticast DISABLED_SocketMulticast
#else
#define MAYBE_SocketMulticast SocketMulticast
#endif
IN_PROC_BROWSER_TEST_F(SocketApiTest, MAYBE_SocketMulticast) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("multicast:%s:%d", kHostname, kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, TCPSocketWriteQuota) {
  extensions::WriteQuotaChecker* write_quota_checker =
      extensions::WriteQuotaChecker::Get(profile());
  constexpr size_t kBytesLimit = 1;
  extensions::WriteQuotaChecker::ScopedBytesLimitForTest scoped_quota(
      write_quota_checker, kBytesLimit);

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTP);
  test_server.AddDefaultHandlers();
  EXPECT_TRUE(test_server.Start());

  net::HostPortPair host_port_pair = test_server.host_port_pair();
  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("tcp_write_quota:%s:%d",
                                    host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, UDPSocketWriteQuota) {
  extensions::WriteQuotaChecker* write_quota_checker =
      extensions::WriteQuotaChecker::Get(profile());
  constexpr size_t kBytesLimit = 1;
  extensions::WriteQuotaChecker::ScopedBytesLimitForTest scoped_quota(
      write_quota_checker, kBytesLimit);

  extensions::TestUdpEchoServer udp_echo_server;
  net::HostPortPair host_port_pair;
  ASSERT_TRUE(udp_echo_server.Start(
      profile()->GetDefaultStoragePartition()->GetNetworkContext(),
      &host_port_pair));

  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("udp_sendTo_quota:%s:%d",
                                    host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
