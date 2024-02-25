// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/test_protocol_handler_registry_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"

using custom_handlers::ProtocolHandler;
using custom_handlers::ProtocolHandlerRegistry;

class ChromeProtocolHandlerRegistryTest : public testing::Test {
 protected:
  ChromeProtocolHandlerRegistryTest() = default;

  bool ProtocolHandlerCanRegisterProtocol(
      const std::string& protocol,
      const GURL& handler_url,
      blink::ProtocolHandlerSecurityLevel security_level) {
    registry_->OnAcceptRegisterProtocolHandler(
        ProtocolHandler::CreateProtocolHandler(protocol, handler_url,
                                               security_level));
    return registry_->IsHandledProtocol(protocol);
  }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    CHECK(profile_->GetPrefs());
    auto delegate = std::make_unique<
        custom_handlers::TestProtocolHandlerRegistryDelegate>();
    registry_ = std::make_unique<ProtocolHandlerRegistry>(profile_->GetPrefs(),
                                                          std::move(delegate));
    registry_->InitProtocolSettings();
  }

  void TearDown() override {
    registry_->Shutdown();
    registry_.reset();
    // Registry owns the delegate_ it handles deletion of that object.
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ProtocolHandlerRegistry> registry_;
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ChromeProtocolHandlerRegistryTest, ExtensionHandler) {
  GURL chrome_extension_handler_url(
      "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/test.html");

  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news", chrome_extension_handler_url,
      blink::ProtocolHandlerSecurityLevel::kStrict));

  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news", chrome_extension_handler_url,
      blink::ProtocolHandlerSecurityLevel::kUntrustedOrigins));

  EXPECT_TRUE(ProtocolHandlerCanRegisterProtocol(
      "news", chrome_extension_handler_url,
      blink::ProtocolHandlerSecurityLevel::kExtensionFeatures));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Isolated Web Apps test
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ChromeProtocolHandlerRegistryTest, IsolatedWebApps) {
  GURL isolated_web_apps_handler_url(
      "isolated-app://aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic/"
      "test.html");

  EXPECT_FALSE(ProtocolHandlerCanRegisterProtocol(
      "news", isolated_web_apps_handler_url,
      blink::ProtocolHandlerSecurityLevel::kStrict));

  EXPECT_TRUE(ProtocolHandlerCanRegisterProtocol(
      "news", isolated_web_apps_handler_url,
      blink::ProtocolHandlerSecurityLevel::kSameOrigin));
}
#endif  // !BUILDFLAG(IS_ANDROID)
