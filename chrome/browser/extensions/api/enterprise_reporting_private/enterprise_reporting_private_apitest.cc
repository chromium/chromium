// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif

namespace extensions {
namespace {

// Manifest key for the Endpoint Verification extension found at
// chrome.google.com/webstore/detail/callobklhcbilhphinckomhgkigmfocg
// This extension is authorized to use the enterprise.reportingPrivate API.
constexpr char kAuthorizedManifestKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjXwWSZq5RLuM5ZbmRWn4gXwpMOb52a"
    "oOhtzIsmbXUWPQeA6/D2p1uaPxIHh6EusxAhXMrBgNaJv1QFxCxiU1aGDlmCR9mOsA7rK5kmVC"
    "i0TYLbQa+C38UDmyhRACrvHO26Jt8qC8oM8yiSuzgb+16rgCCcek9dP7IaHaoJMsBMAEf3VEno"
    "4xt+kCAAsFsyFCB4plWid54avqpgg6+OsR3ZtUAMWooVziJHVmBTiyl82QR5ZURYr+TjkiljkP"
    "EBLaMTKC2g7tUl2h0Q1UmMTMc2qxLIVVREhr4q9iOegNxfNy78BaxZxI1Hjp0EVYMZunIEI9r1"
    "k0vyyaH13TvdeqNwIDAQAB";

// Manifest key for the Google Translate extension found at
// chrome.google.com/webstore/detail/aapbdbdomjkkjkaonfhkkikfgjllcleb
// This extension is unauthorized to use the enterprise.reportingPrivate API.
constexpr char kUnauthorizedManifestKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCfHy1M+jghaHyaVAILzx/c/Dy+RXtcaP9/5p"
    "C7EY8JlNEI/G4DIIng9IzlrH8UWStpMWMyGUsdyusn2PkYFrqfVzhc2azVF3PX9D0KHG3FLN3m"
    "Noz1YTBHvO5QSXJf292qW0tTYuoGqeTfXtF9odLdg20Xd0YrLmtS4TQkpSYGDwIDAQAB";

constexpr char kManifestTemplate[] = R"(
    {
      "key": "%s",
      "name": "Enterprise Private Reporting API Test",
      "version": "0.1",
      "manifest_version": 2,
      "permissions": [
          "enterprise.reportingPrivate"
      ],
      "background": { "scripts": ["background.js"] }
    })";

}  // namespace

// This test class is to validate that the API is correctly unavailable on
// unsupported extensions and unsupported platforms. It also does basic
// validation that fields are present in the values the API returns, but it
// doesn't make strong assumption on what those values are to minimize the kind
// of mocking that is already done in unit/browser tests covering this API.
class EnterpriseReportingPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  EnterpriseReportingPrivateApiTest() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    browser_dm_token_storage_.SetClientId("client_id");
#endif
  }

  ~EnterpriseReportingPrivateApiTest() override = default;

  void RunTest(const std::string& background_js,
               bool authorized_manifest_key = true) {
    ResultCatcher result_catcher;
    TestExtensionDir test_dir;
    test_dir.WriteManifest(base::StringPrintf(
        kManifestTemplate, authorized_manifest_key ? kAuthorizedManifestKey
                                                   : kUnauthorizedManifestKey));

    // Since the API functions use async callbacks, this wrapper code is
    // necessary for assertions to work properly.
    constexpr char kTestWrapper[] = R"(
        chrome.test.runTests([
          async function asyncAssertions() {
            %s
          }
        ]);)";
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                       base::StringPrintf(kTestWrapper, background_js.c_str()));

    const Extension* extension = LoadExtension(
        test_dir.UnpackedPath(), {.ignore_manifest_warnings = true});
    ASSERT_TRUE(extension);
    ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  }

 protected:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage browser_dm_token_storage_;
#endif
};

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest,
                       ExtensionAvailability) {
  constexpr char kBackgroundJs[] = R"(
    chrome.test.assertEq(undefined, chrome.enterprise);
    chrome.test.notifyPass();
  )";
  RunTest(kBackgroundJs, /*authorized_manifest_key*/ false);
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest, GetDeviceId) {
  constexpr char kAssertions[] =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      "chrome.test.assertNoLastError();"
      "chrome.test.assertEq(id, 'client_id');";
#else
      "chrome.test.assertLastError('Access to extension API denied.');";
#endif
  constexpr char kTest[] = R"(
      chrome.test.assertEq(
        'function',
        typeof chrome.enterprise.reportingPrivate.getDeviceId);
      chrome.enterprise.reportingPrivate.getDeviceId((id) => {
        %s
        chrome.test.notifyPass();
      });
  )";
  RunTest(base::StringPrintf(kTest, kAssertions));
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// https://crbug.com/1222670
#define MAYBE_GetPersistentSecret DISABLED_GetPersistentSecret
#else
#define MAYBE_GetPersistentSecret GetPersistentSecret
#endif
IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest,
                       MAYBE_GetPersistentSecret) {
  constexpr char kAssertions[] =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      "chrome.test.assertNoLastError();"
      "chrome.test.assertTrue(secret instanceof ArrayBuffer);";
#else
      "chrome.test.assertLastError('Access to extension API denied.');";
#endif
  constexpr char kTest[] = R"(
      chrome.test.assertEq(
        'function',
        typeof chrome.enterprise.reportingPrivate.getPersistentSecret);
      chrome.enterprise.reportingPrivate.getPersistentSecret((secret) => {
        %s
        chrome.test.notifyPass();
      });
  )";
  RunTest(base::StringPrintf(kTest, kAssertions));
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest, GetDeviceData) {
  constexpr char kAssertions[] =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      "chrome.test.assertNoLastError();"
      "chrome.test.assertTrue(data instanceof ArrayBuffer);";
#else
      "chrome.test.assertLastError('Access to extension API denied.');";
#endif
  constexpr char kTest[] = R"(
      chrome.test.assertEq(
        'function',
        typeof chrome.enterprise.reportingPrivate.getDeviceData);
      chrome.enterprise.reportingPrivate.getDeviceData('id', (data) => {
        %s
        chrome.test.notifyPass();
      });
  )";
  RunTest(base::StringPrintf(kTest, kAssertions));
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest, SetDeviceData) {
  constexpr char kAssertions[] =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      "chrome.test.assertNoLastError();"
      "chrome.enterprise.reportingPrivate.getDeviceData('id', (data) => {"
      "  let view = new Int8Array(data);"
      "  chrome.test.assertEq(3, view.length);"
      "  chrome.test.assertEq(2, view[0]);"
      "  chrome.test.assertEq(1, view[1]);"
      "  chrome.test.assertEq(0, view[2]);"
      "  chrome.test.notifyPass();"
      "});";
#else
      "chrome.test.assertLastError('Access to extension API denied.');"
      "chrome.test.notifyPass();";
#endif
  constexpr char kTest[] = R"(
      chrome.test.assertEq(
        'function',
        typeof chrome.enterprise.reportingPrivate.setDeviceData);
      let array = new Int8Array(3);
      array[0] = 2;
      array[1] = 1;
      array[2] = 0;
      chrome.enterprise.reportingPrivate.setDeviceData('id', array, () => {
        %s
      });
  )";
  RunTest(base::StringPrintf(kTest, kAssertions));
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest, GetDeviceInfo) {
#if BUILDFLAG(IS_WIN)
  constexpr char kOSName[] = "windows";
#elif BUILDFLAG(IS_MAC)
  constexpr char kOSName[] = "macOS";
#elif BUILDFLAG(IS_LINUX)
  constexpr char kOSName[] = "linux";
#endif

#if BUILDFLAG(IS_WIN)
  // The added conditions for windows are related to the fact that we don't know
  // if the machine running the test is managed or not
  constexpr char kTest[] = R"(
    chrome.test.assertEq(
      'function',
      typeof chrome.enterprise.reportingPrivate.getDeviceInfo);

    chrome.enterprise.reportingPrivate.getDeviceInfo((deviceInfo) => {
      chrome.test.assertNoLastError();
      let count = 9;
      if(deviceInfo.windowsUserDomain){
        count++;
        chrome.test.assertEq(typeof deviceInfo.windowsUserDomain, "string");
      } else {
        chrome.test.assertEq(typeof deviceInfo.windowsUserDomain, "undefined");
      }

      if(deviceInfo.windowsMachineDomain){
        count++;
        chrome.test.assertEq(typeof deviceInfo.windowsMachineDomain, "string");
      } else {
        chrome.test.assertEq(
          typeof deviceInfo.windowsMachineDomain,
          "undefined");
      }
      chrome.test.assertEq(count, Object.keys(deviceInfo).length);
      chrome.test.assertEq('%s', deviceInfo.osName);
      chrome.test.assertEq(typeof deviceInfo.osVersion, 'string');
      chrome.test.assertEq(typeof deviceInfo.securityPatchLevel, 'string');
      chrome.test.assertEq(typeof deviceInfo.deviceHostName, 'string');
      chrome.test.assertEq(typeof deviceInfo.deviceModel, 'string');
      chrome.test.assertEq(typeof deviceInfo.serialNumber, 'string');
      chrome.test.assertEq(typeof deviceInfo.screenLockSecured, 'string');
      chrome.test.assertEq(typeof deviceInfo.diskEncrypted, 'string');
      chrome.test.assertTrue(deviceInfo.macAddresses instanceof Array);

      chrome.test.notifyPass();
    });)";
  RunTest(base::StringPrintf(kTest, kOSName));
#elif BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  constexpr char kTest[] = R"(
    chrome.test.assertEq(
      'function',
      typeof chrome.enterprise.reportingPrivate.getDeviceInfo);

    chrome.enterprise.reportingPrivate.getDeviceInfo((deviceInfo) => {
      chrome.test.assertNoLastError();

      chrome.test.assertEq(9, Object.keys(deviceInfo).length);
      chrome.test.assertEq('%s', deviceInfo.osName);
      chrome.test.assertEq(typeof deviceInfo.osVersion, 'string');
      chrome.test.assertEq(typeof deviceInfo.securityPatchLevel, 'string');
      chrome.test.assertEq(typeof deviceInfo.deviceHostName, 'string');
      chrome.test.assertEq(typeof deviceInfo.deviceModel, 'string');
      chrome.test.assertEq(typeof deviceInfo.serialNumber, 'string');
      chrome.test.assertEq(typeof deviceInfo.screenLockSecured, 'string');
      chrome.test.assertEq(typeof deviceInfo.diskEncrypted, 'string');
      chrome.test.assertTrue(deviceInfo.macAddresses instanceof Array);
      chrome.test.assertEq(typeof deviceInfo.windowsMachineDomain, "undefined");
      chrome.test.assertEq(typeof deviceInfo.windowsUserDomain, "undefined");

      chrome.test.notifyPass();
    });)";
  RunTest(base::StringPrintf(kTest, kOSName));
#else
  RunTest(R"(
      chrome.test.assertEq(
        'function',
        typeof chrome.enterprise.reportingPrivate.getDeviceInfo);

      chrome.enterprise.reportingPrivate.getDeviceInfo((deviceInfo) => {
        chrome.test.assertLastError('Access to extension API denied.');
        chrome.test.notifyPass();
      });
  )");
#endif
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest, GetContextInfo) {
#if BUILDFLAG(IS_WIN)
  constexpr char kChromeCleanupEnabledType[] = "boolean";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  constexpr char kThirdPartyBlockingEnabledType[] = "boolean";
  constexpr char kCount[] = "17";
#else
  constexpr char kThirdPartyBlockingEnabledType[] = "undefined";
  constexpr char kCount[] = "16";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
  constexpr char kChromeCleanupEnabledType[] = "undefined";
  constexpr char kThirdPartyBlockingEnabledType[] = "undefined";
  constexpr char kCount[] = "15";
#endif  // BUILDFLAG(IS_WIN)

  constexpr char kTest[] = R"(
    chrome.test.assertEq(
      'function',
      typeof chrome.enterprise.reportingPrivate.getContextInfo);
    chrome.enterprise.reportingPrivate.getContextInfo((info) => {
      chrome.test.assertNoLastError();

      chrome.test.assertEq(%s, Object.keys(info).length);
      chrome.test.assertTrue(info.browserAffiliationIds instanceof Array);
      chrome.test.assertTrue(info.profileAffiliationIds instanceof Array);
      chrome.test.assertTrue(info.onFileAttachedProviders instanceof Array);
      chrome.test.assertTrue(info.onFileDownloadedProviders instanceof Array);
      chrome.test.assertTrue(info.onBulkDataEntryProviders instanceof Array);
      chrome.test.assertEq(typeof info.realtimeUrlCheckMode, 'string');
      chrome.test.assertTrue(info.onSecurityEventProviders instanceof Array);
      chrome.test.assertEq(typeof info.browserVersion, 'string');
      chrome.test.assertEq(typeof info.safeBrowsingProtectionLevel, 'string');
      chrome.test.assertEq(typeof info.siteIsolationEnabled, 'boolean');
      chrome.test.assertEq(typeof info.builtInDnsClientEnabled, 'boolean');
      chrome.test.assertEq
        (typeof info.passwordProtectionWarningTrigger, 'string');
      chrome.test.assertEq(typeof info.chromeCleanupEnabled, '%s');
      chrome.test.assertEq
        (typeof info.chromeRemoteDesktopAppBlocked, 'boolean');
      chrome.test.assertEq(typeof info.thirdPartyBlockingEnabled,'%s');
      chrome.test.assertEq(typeof info.osFirewall, 'string');
      chrome.test.assertTrue(info.systemDnsServers instanceof Array);

      chrome.test.notifyPass();
    });)";
  RunTest(base::StringPrintf(kTest, kCount, kChromeCleanupEnabledType,
                             kThirdPartyBlockingEnabledType));
}

IN_PROC_BROWSER_TEST_F(EnterpriseReportingPrivateApiTest, GetCertificate) {
  // The encodedCertificate attribute should always be empty when the
  // AutoSelectCertificateForUrls policy is unset.
  RunTest(R"(
    chrome.test.assertEq(
      'function',
      typeof chrome.enterprise.reportingPrivate.getCertificate);
    chrome.enterprise.reportingPrivate.getCertificate(
      'https://foo.com', (certificate) => {
        chrome.test.assertNoLastError();

        chrome.test.assertEq(1, Object.keys(certificate).length);
        chrome.test.assertEq(typeof certificate.status, 'string');
        chrome.test.assertEq(certificate.encodedCertificate, undefined);

        chrome.test.notifyPass();
    });)");
}

}  // namespace extensions
