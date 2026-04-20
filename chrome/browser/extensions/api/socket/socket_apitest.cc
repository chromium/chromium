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
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

// This API is not supported on Android.
static_assert(!BUILDFLAG(IS_ANDROID));

namespace extensions {
namespace {

using api_test_utils::RunFunctionAndReturnSingleResult;

const char kHostname[] = "www.foo.com";
const int kPort = 8888;

class SocketApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketUDPExtension) {
  TestUdpEchoServer udp_echo_server;
  net::HostPortPair host_port_pair;
  ASSERT_TRUE(udp_echo_server.Start(
      profile()->GetDefaultStoragePartition()->GetNetworkContext(),
      &host_port_pair));

  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  // Test that sendTo() is properly resolving hostnames.
  host_port_pair.set_host(kHostname);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());

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
  catcher.RestrictToBrowserContext(profile());

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
  catcher.RestrictToBrowserContext(profile());
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

// Failed on an old version of macOS, unclear if it still does.
// TODO(https://crbug.com/40182775): re-enable.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SocketMulticast DISABLED_SocketMulticast
#else
#define MAYBE_SocketMulticast SocketMulticast
#endif
IN_PROC_BROWSER_TEST_F(SocketApiTest, MAYBE_SocketMulticast) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("multicast:%s:%d", kHostname, kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, TCPSocketWriteQuota) {
  WriteQuotaChecker* write_quota_checker = WriteQuotaChecker::Get(profile());
  constexpr size_t kBytesLimit = 1;
  WriteQuotaChecker::ScopedBytesLimitForTest scoped_quota(write_quota_checker,
                                                          kBytesLimit);

  net::EmbeddedTestServer test_server(net::EmbeddedTestServer::TYPE_HTTP);
  test_server.AddDefaultHandlers();
  EXPECT_TRUE(test_server.Start());

  net::HostPortPair host_port_pair = test_server.host_port_pair();
  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("tcp_write_quota:%s:%d",
                                    host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, UDPSocketWriteQuota) {
  WriteQuotaChecker* write_quota_checker = WriteQuotaChecker::Get(profile());
  constexpr size_t kBytesLimit = 1;
  WriteQuotaChecker::ScopedBytesLimitForTest scoped_quota(write_quota_checker,
                                                          kBytesLimit);

  TestUdpEchoServer udp_echo_server;
  net::HostPortPair host_port_pair;
  ASSERT_TRUE(udp_echo_server.Start(
      profile()->GetDefaultStoragePartition()->GetNetworkContext(),
      &host_port_pair));

  int port = host_port_pair.port();
  ASSERT_GT(port, 0);

  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());

  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("udp_sendTo_quota:%s:%d",
                                    host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketUDPCreateGood) {
  auto socket_create_function = base::MakeRefCounted<SocketCreateFunction>();
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_create_function.get(), "[\"udp\"]", profile()));
  const base::DictValue& value = result->GetDict();
  std::optional<int> socket_id = value.FindInt("socketId");
  ASSERT_TRUE(socket_id);
  EXPECT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketTCPCreateGood) {
  auto socket_create_function = base::MakeRefCounted<SocketCreateFunction>();
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::optional<base::Value> result(RunFunctionAndReturnSingleResult(
      socket_create_function.get(), "[\"tcp\"]", profile()));
  const base::DictValue& value = result->GetDict();
  std::optional<int> socket_id = value.FindInt("socketId");
  ASSERT_TRUE(socket_id);
  ASSERT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, GetNetworkList) {
  auto socket_function = base::MakeRefCounted<SocketGetNetworkListFunction>();
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_function->set_extension(empty_extension.get());
  socket_function->set_has_callback(true);

  std::optional<base::Value> result(
      RunFunctionAndReturnSingleResult(socket_function.get(), "[]", profile()));

  // If we're invoking socket tests, all we can confirm is that we have at
  // least one address, but not what it is.
  ASSERT_TRUE(result->is_list());
  ASSERT_FALSE(result->GetList().empty());
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, WriteQuotaChecker) {
  WriteQuotaChecker* checker = WriteQuotaChecker::Get(profile());

  constexpr size_t kBytesLimit = 100;
  WriteQuotaChecker::ScopedBytesLimitForTest scoped_limit(checker, kBytesLimit);

  const ExtensionId extension_id = "test_extension_id";
  const ExtensionId another_extension_id = "another_test_extension_id";

  // Fails if a single request is too large.
  EXPECT_FALSE(checker->TakeBytes(extension_id, kBytesLimit + 1));

  // Fails if combined multiple requests are larger than limit.
  EXPECT_TRUE(checker->TakeBytes(extension_id, kBytesLimit));
  EXPECT_FALSE(checker->TakeBytes(extension_id, 1));

  // Different extension is not affected.
  EXPECT_TRUE(checker->TakeBytes(another_extension_id, kBytesLimit));

  // Simulate a request is done and return bytes to the pool.
  checker->ReturnBytes(extension_id, kBytesLimit);

  // Writes are allowed again.
  EXPECT_TRUE(checker->TakeBytes(extension_id, 1));
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, ShutdownWithLingeringWriteQuota) {
  // An arbitrary SocketApiFunction.
  auto socket_function = base::MakeRefCounted<SocketWriteFunction>();
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_function->set_extension(empty_extension.get());

  auto dispatcher = std::make_unique<ExtensionFunctionDispatcher>(profile());
  socket_function->SetDispatcher(dispatcher->AsWeakPtr());

  // Uses some write quota.
  ASSERT_TRUE(socket_function->TakeWriteQuota(100));

  // Ensures the function has a null BrowserContext to simulate shutdown.
  socket_function->SetDispatcher(nullptr);
  ASSERT_FALSE(socket_function->browser_context());

  // Resets write quota and it should not crash.
  socket_function->ReturnWriteQuota();
}

}  // namespace extensions
