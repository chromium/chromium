// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/socket/write_quota_checker.h"
#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

// This API is not supported on Android.
static_assert(!BUILDFLAG(IS_ANDROID));

namespace extensions {
namespace {

const char kHostname[] = "www.foo.com";
const int kPort = 8888;

class SocketsUdpApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule(kHostname, "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, SocketsUdpCreateGood) {
  auto socket_create_function =
      base::MakeRefCounted<api::SocketsUdpCreateFunction>();
  scoped_refptr<const Extension> empty_extension =
      ExtensionBuilder("Test").Build();

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  std::optional<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          socket_create_function.get(), "[]", profile()));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  std::optional<int> socket_id = result->GetDict().FindInt("socketId");
  ASSERT_TRUE(socket_id);
  ASSERT_GT(*socket_id, 0);
}

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, SocketsUdpExtension) {
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

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("sockets_udp/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("udp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// See crbug.com/40342086.
IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, DISABLED_SocketsUdpMulticast) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("sockets_udp/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("multicast:%s:%d", kHostname, kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketsUdpApiTest, SocketsUdpSendWriteQuota) {
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

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("sockets_udp/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("udp_send_write_quota:%s:%d",
                                    host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace
}  // namespace extensions
