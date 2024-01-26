// Copyright 2012 The Chromium Authors
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
#include "extensions/browser/api/socket/socket_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

std::unique_ptr<KeyedService> ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  return std::make_unique<ApiResourceManager<Socket>>(context);
}

class SocketUnitTest : public ExtensionApiUnittest {
 public:
  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    ApiResourceManager<Socket>::GetFactoryInstance()->SetTestingFactoryAndUse(
        browser()->profile(),
        base::BindRepeating(&ApiResourceManagerTestFactory));
  }
};

TEST_F(SocketUnitTest, Create) {
  // Create SocketCreateFunction and put it on BrowserThread
  SocketCreateFunction* function = new SocketCreateFunction();

  // Run tests
  std::optional<base::Value::Dict> result =
      RunFunctionAndReturnDictionary(function, "[\"tcp\"]");
  ASSERT_TRUE(result);
}

TEST_F(SocketUnitTest, InvalidPort) {
  const std::string kError = "Port must be a value between 0 and 65535.";

  SocketConnectFunction* connect_function = new SocketConnectFunction();
  EXPECT_EQ(kError,
            RunFunctionAndReturnError(connect_function, "[1, \"foo\", -1]"));
  SocketBindFunction* bind_function = new SocketBindFunction();
  EXPECT_EQ(kError,
            RunFunctionAndReturnError(bind_function, "[1, \"foo\", -1]"));
}

}  // namespace extensions
