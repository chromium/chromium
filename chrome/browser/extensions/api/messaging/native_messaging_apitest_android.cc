// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"

static_assert(BUILDFLAG(IS_ANDROID));
static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// A fixed public key for an extension not in EchoService's allowlist.
// Resulting extension ID: aapocclcgogkmnckokdopfmhonfmgoek
constexpr char kUnauthorizedExtensionKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8xv6iO+j4kzj1HiBL93+XVJH/CRyAQMUHS/"
    "Z0l8nCAzaAFkW/JsNwxJqQhrZspnxLqbQxNncXs6g6bsXAwKHiEs+LSs+bIv0Gc/"
    "2ycZdhXJ8Gh"
    "EsSMakog5dpQd1681c2gLK/8CrAoewE/0GIKhaFcp7a2iZlGh4Am6fgMKy0iQIDAQAB";

}  // namespace

class NativeMessagingAndroidApiTest : public ExtensionApiTest {
 public:
  NativeMessagingAndroidApiTest()
      : scoped_feature_list_{
            extensions_features::kApiDesktopAndroidNativeMessaging} {}
  ~NativeMessagingAndroidApiTest() override = default;
  NativeMessagingAndroidApiTest(const NativeMessagingAndroidApiTest&) = delete;
  NativeMessagingAndroidApiTest& operator=(
      const NativeMessagingAndroidApiTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest, NativeMessagingBasic) {
  ASSERT_TRUE(RunExtensionTest("native_messaging")) << message_;
}

IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest, SendNativeMessage) {
  ASSERT_TRUE(RunExtensionTest("native_messaging_send_native_message"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest, ConnectNative) {
  ASSERT_TRUE(RunExtensionTest("native_messaging_connect")) << message_;
}

// Test that the remote app can reject connections based on the extension ID.
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest,
                       UnauthorizedExtensionRejected) {
  constexpr char kManifest[] = R"(
      {
        "name": "UnauthorizedExtension",
        "version": "1.0",
        "manifest_version": 3,
        "key": "%s",
        "permissions": ["nativeMessaging"],
        "background": {
          "service_worker": "background.js"
        }
      })";

  // Attempt to connect to the test Android app. The connection should be
  // rejected with an error message.
  constexpr char kBackground[] = R"(
    chrome.test.runTests([
      function testUnauthorized() {
        const APP_NAME = 'org.chromium.chrome.tests.support';
        const expectedError =
            'Unable to connect to org.chromium.chrome.tests.support.';

        chrome.runtime.sendNativeMessage(APP_NAME, {text: 'hello'},
            chrome.test.callbackFail(expectedError, function(response) {
              chrome.test.assertEq(undefined, response);
            }));
      }
    ]);
  )";

  TestExtensionDir dir;
  dir.WriteManifest(base::StringPrintf(kManifest, kUnauthorizedExtensionKey));
  dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  ASSERT_TRUE(RunExtensionTest(dir.UnpackedPath(), {}, {}));
}

}  // namespace extensions
