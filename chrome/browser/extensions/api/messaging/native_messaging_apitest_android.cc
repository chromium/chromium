// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
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
  NativeMessagingAndroidApiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {extensions_features::kApiDesktopAndroidNativeMessaging,
         extensions_features::
             kApiDesktopAndroidNativeMessagingBypassExtensionAllowlist},
        /*disabled_features=*/{});
  }
  ~NativeMessagingAndroidApiTest() override = default;
  NativeMessagingAndroidApiTest(const NativeMessagingAndroidApiTest&) = delete;
  NativeMessagingAndroidApiTest& operator=(
      const NativeMessagingAndroidApiTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Needed to simulate installs for webstore extensions.
  ScopedInstallVerifierBypassForTest ignore_install_verification_;
};

// Test fixture explicitly disabling the bypass flag, verifying allowlist
// behavior.
class NativeMessagingAndroidUnauthorizedApiTest : public ExtensionApiTest {
 public:
  NativeMessagingAndroidUnauthorizedApiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::
                                  kApiDesktopAndroidNativeMessaging},
        /*disabled_features=*/
        {extensions_features::
             kApiDesktopAndroidNativeMessagingBypassExtensionAllowlist});
  }
  ~NativeMessagingAndroidUnauthorizedApiTest() override = default;
  NativeMessagingAndroidUnauthorizedApiTest(
      const NativeMessagingAndroidUnauthorizedApiTest&) = delete;
  NativeMessagingAndroidUnauthorizedApiTest& operator=(
      const NativeMessagingAndroidUnauthorizedApiTest&) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // Needed to simulate installs for webstore extensions.
  ScopedInstallVerifierBypassForTest ignore_install_verification_;
};

IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidUnauthorizedApiTest,
                       UnauthorizedExtensionRejected) {
  ASSERT_TRUE(RunExtensionTest("native_messaging_android_unauthorized"))
      << message_;
}

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

IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest, NativeMessagingCerts) {
  ASSERT_TRUE(RunExtensionTest("native_messaging_certs")) << message_;
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

        chrome.runtime.sendNativeMessage(
            APP_NAME, {text: 'hello'},
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

// Test that disabling an extension triggers closeConnection() on the external
// Android service.
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest,
                       ExtensionUnloadNotifiesApp) {
  // Load an extension that just calls connectNative to keep a port open.
  const Extension* extension =
      LoadExtension(test_data_dir_.DirName()
                        .AppendASCII("native_messaging")
                        .AppendASCII("connect_native"));
  ASSERT_TRUE(extension);
  const auto extension_id = extension->id();

  // Load an observer extension which sends a message to the app and waits for
  // when the app has connected or disconnected with another extension.
  constexpr char kObserverExtensionManifest[] = R"(
      {
        "name": "ObserverExtension",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": ["nativeMessaging"],
        "background": {
          "service_worker": "background.js"
        }
      })";

  TestExtensionDir observer_extension_dir;
  observer_extension_dir.WriteManifest(kObserverExtensionManifest);
  observer_extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                                   "// Empty background script");
  const Extension* observer_ext =
      LoadExtension(observer_extension_dir.UnpackedPath());
  ASSERT_TRUE(observer_ext);

  // Through `observer_ext`, wait for the app to connect to `extension`.
  {
    std::string load_wait_script = base::StringPrintf(
        R"(
          chrome.runtime.sendNativeMessage(
              'org.chromium.chrome.tests.support',
              {request: 'waitForExtensionConnected', extensionId: '%s'},
              (response) => {
                chrome.test.sendScriptResult(response);
              });
        )",
        extension_id);

    base::Value load_result = BackgroundScriptExecutor::ExecuteScript(
        profile(), observer_ext->id(), load_wait_script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    EXPECT_THAT(load_result, base::test::IsJson(base::StringPrintf(
                                 R"({"status": "loaded", "extensionId": "%s"})",
                                 extension_id)));
  }

  // `observer_ext` sends "waitForExtensionUnloaded" to wait for `extension`'s
  // unload notification.
  std::string unload_wait_script = base::StringPrintf(
      R"(
        chrome.runtime.sendNativeMessage(
            'org.chromium.chrome.tests.support',
            {request: 'waitForExtensionUnloaded', extensionId: '%s'},
            (response) => {
              chrome.test.sendScriptResult(response);
            });
      )",
      extension_id);

  BackgroundScriptExecutor unload_executor(profile());
  unload_executor.ExecuteScriptAsync(
      observer_ext->id(), unload_wait_script,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

  // Disable `extension`. This will be propagated to the app via an
  // IExtensionNativeMessageService.closeConnection() IPC.
  DisableExtension(extension_id);

  // Wait for the app to receive the IPC and reply to the sendNativeMessage call
  // in `unload_wait_script`.
  base::Value unload_result = unload_executor.WaitForResult();
  EXPECT_THAT(
      unload_result,
      base::test::IsJson(base::StringPrintf(
          R"({"status": "unloaded", "extensionId": "%s"})", extension_id)));
}

// An unpacked extension can send messages to the external Android app without
// specifying certificates.
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest,
                       EmptyCertificates_Unpacked) {
  ExtensionTestMessageListener listener;
  const Extension* extension =
      LoadExtension(test_data_dir_.DirName()
                        .AppendASCII("native_messaging")
                        .AppendASCII("send_native_message_android_no_certs"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("success", listener.message());
}

// A packed extension must specify certificates in order to send messages to the
// external Android app.
IN_PROC_BROWSER_TEST_F(NativeMessagingAndroidApiTest,
                       EmptyCertificates_Packed) {
  ExtensionTestMessageListener listener;
  base::FilePath crx_path =
      PackExtension(test_data_dir_.DirName()
                        .AppendASCII("native_messaging")
                        .AppendASCII("send_native_message_android_no_certs"));
  const Extension* extension = InstallExtensionFromWebstore(crx_path, 1);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ(
      "caught error: Error in invocation of runtime.sendNativeMessage("
      "[string|runtime.NativeMessageTarget] application, "
      "object message, optional function callback): Packed extensions on "
      "Android must specify at least one expected signing certificate in "
      "'androidCertificates'.",
      listener.message());
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
