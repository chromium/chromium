// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"

// This API is not supported on Android.
static_assert(!BUILDFLAG(IS_ANDROID));

namespace extensions {
namespace {

const int kPort = 8888;

using SocketsTcpServerApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest, SocketTCPCreateGood) {
  scoped_refptr<api::SocketsTcpServerCreateFunction> socket_create_function(
      new api::SocketsTcpServerCreateFunction());
  scoped_refptr<const Extension> empty_extension(
      ExtensionBuilder("Test").Build());

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

IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest, SocketTCPServerExtension) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(profile());
  ExtensionTestMessageListener listener("info_please",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("sockets_tcp_server/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(base::StringPrintf("tcp_server:127.0.0.1:%d", kPort));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Flaky. http://crbug.com/41222743
IN_PROC_BROWSER_TEST_F(SocketsTcpServerApiTest,
                       DISABLED_SocketTCPServerUnbindOnUnload) {
  std::string path("sockets_tcp_server/unload");
  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(path));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();

  UnloadExtension(extension->id());

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(path))) << message_;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace
}  // namespace extensions
