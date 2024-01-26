// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_browser_process.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace api {

static std::unique_ptr<KeyedService> ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  return std::make_unique<ApiResourceManager<ResumableTCPSocket>>(context);
}

static std::unique_ptr<KeyedService> ApiResourceManagerTestServerFactory(
    content::BrowserContext* context) {
  return std::make_unique<ApiResourceManager<ResumableTCPServerSocket>>(
      context);
}

class SocketsTcpServerUnitTest : public ExtensionApiUnittest {
 public:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    ApiResourceManager<ResumableTCPSocket>::GetFactoryInstance()
        ->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&ApiResourceManagerTestFactory));

    ApiResourceManager<ResumableTCPServerSocket>::GetFactoryInstance()
        ->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(&ApiResourceManagerTestServerFactory));
  }
};

TEST_F(SocketsTcpServerUnitTest, Create) {
  // Create SocketCreateFunction and put it on BrowserThread
  SocketsTcpServerCreateFunction* function =
      new SocketsTcpServerCreateFunction();

  // Run tests
  std::optional<base::Value::Dict> result(RunFunctionAndReturnDictionary(
      function, "[{\"persistent\": true, \"name\": \"foo\"}]"));
  ASSERT_TRUE(result);
}

}  // namespace api
}  // namespace extensions
