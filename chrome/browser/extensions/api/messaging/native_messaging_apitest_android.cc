// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

static_assert(BUILDFLAG(IS_ANDROID));
static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// A fixed public key for an extension not in EchoService's allowlist.
// Resulting extension ID: ddchlicdkolnonkihahngkmmmjnjlkkf
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

// A sub-test which tests that the browser sends information on whether the
// extension is verified (i.e. content verified) to the test Android app.
// Unlike the extensions used for NativeMessagingAndroidApiTest which the
// external test app will always allow or disallow based on their ID, extensions
// used for this test class are only accepted by the test app if they are
// verified.
class NativeMessagingAndroidVerifiedExtensionsTest
    : public NativeMessagingAndroidApiTest {
 public:
  NativeMessagingAndroidVerifiedExtensionsTest() = default;
  ~NativeMessagingAndroidVerifiedExtensionsTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    NativeMessagingAndroidApiTest::SetUpOnMainThread();
    test_data_dir_ = test_data_dir_.DirName().AppendASCII("native_messaging");
  }

  void TearDownOnMainThread() override {
    ChromeContentVerifierDelegate::SetDefaultModeForTesting(std::nullopt);
    NativeMessagingAndroidApiTest::TearDownOnMainThread();
  }

 private:
  // Needed to simulate installs for webstore extensions.
  ScopedInstallVerifierBypassForTest ignore_install_verification_;
};

// An unpacked extension is unverified and rejected by the native app.
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidVerifiedExtensionsTest,
                       UnpackedExtensionRejected) {
  ChromeContentVerifierDelegate::SetDefaultModeForTesting(
      ChromeContentVerifierDelegate::VerifyInfo::Mode::ENFORCE);

  ExtensionTestMessageListener listener;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("send_native_message"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("app received isVerified: false", listener.message());
}

// Extension from web store is content verified and accepted.
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidVerifiedExtensionsTest,
                       WebstoreExtensionAccepted) {
  ChromeContentVerifierDelegate::SetDefaultModeForTesting(
      ChromeContentVerifierDelegate::VerifyInfo::Mode::ENFORCE);

  ExtensionTestMessageListener listener;
  base::FilePath crx_path =
      PackExtension(test_data_dir_.AppendASCII("send_native_message"));
  const Extension* extension = InstallExtensionFromWebstore(crx_path, 1);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("app received isVerified: true", listener.message());
}

// Extension installed via policy is verified and accepted.
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidVerifiedExtensionsTest,
                       PolicyExtensionAccepted) {
  ChromeContentVerifierDelegate::SetDefaultModeForTesting(
      ChromeContentVerifierDelegate::VerifyInfo::Mode::ENFORCE);

  ExtensionTestMessageListener listener;
  base::FilePath crx_path =
      PackExtension(test_data_dir_.AppendASCII("send_native_message"));
  const Extension* extension = InstallExtension(
      crx_path, 1, mojom::ManifestLocation::kExternalPolicyDownload);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("app received isVerified: true", listener.message());
}

// Webstore extension is unverified and rejected when content verification is
// inactive (Mode::NONE).
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidVerifiedExtensionsTest,
                       WebstoreExtensionWithoutVerificationRejected) {
  ChromeContentVerifierDelegate::SetDefaultModeForTesting(
      ChromeContentVerifierDelegate::VerifyInfo::Mode::NONE);

  ExtensionTestMessageListener listener;
  base::FilePath crx_path =
      PackExtension(test_data_dir_.AppendASCII("send_native_message"));
  const Extension* extension = InstallExtensionFromWebstore(crx_path, 1);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("app received isVerified: false", listener.message());
}

}  // namespace extensions
