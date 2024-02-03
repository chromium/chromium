// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom-shared.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/lacros/crosapi_pref_observer.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace extensions {

namespace {

const char kNoServer[] = "";
const char kNoBypass[] = "";
const char kNoPac[] = "";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool IsLacrosServiceSyncingProxyPref() {
  static constexpr int kMinVersionProxyPolicy = 4;
  const int version = chromeos::LacrosService::Get()
                          ->GetInterfaceVersion<crosapi::mojom::Prefs>();
  return version >= kMinVersionProxyPolicy;
}
#endif

}  // namespace

class ProxySettingsApiTest : public ExtensionApiTest {
 public:
  ProxySettingsApiTest() {}

  ProxySettingsApiTest(const ProxySettingsApiTest&) = delete;
  ProxySettingsApiTest& operator=(const ProxySettingsApiTest&) = delete;

 protected:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void TearDownOnMainThread() override {
    // Clear the proxy from the test_ash_chrome since the same instance Ash is
    // used for all tests in the target. Setting a proxy will prevent other
    // tests which require a direct connection to complete successfully.
    auto* lacros_service = chromeos::LacrosService::Get();
    if (!lacros_service) {
      ExtensionApiTest::TearDownOnMainThread();
      return;
    }
    if (IsLacrosServiceSyncingProxyPref()) {
      if (lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
        lacros_service->GetRemote<crosapi::mojom::Prefs>()
            ->ClearExtensionControlledPref(crosapi::mojom::PrefPath::kProxy,
                                           base::DoNothing());
      }
    } else {
      if (lacros_service
              ->IsAvailable<crosapi::mojom::NetworkSettingsService>()) {
        lacros_service->GetRemote<crosapi::mojom::NetworkSettingsService>()
            ->ClearExtensionProxy();
      }
    }
    ExtensionApiTest::TearDownOnMainThread();
  }
#endif

  void ValidateSettings(int expected_mode,
                        const std::string& expected_server,
                        const std::string& bypass,
                        const std::string& expected_pac_url,
                        PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(proxy_config::prefs::kProxy);
    ASSERT_TRUE(pref != nullptr);
    EXPECT_TRUE(pref->IsExtensionControlled());

    // TODO(https://crbug.com/1348219) This should call
    // `PrefService::GetDict`.
    ProxyConfigDictionary dict(
        pref_service->GetDict(proxy_config::prefs::kProxy).Clone());

    ProxyPrefs::ProxyMode mode;
    ASSERT_TRUE(dict.GetMode(&mode));
    EXPECT_EQ(expected_mode, mode);

    std::string value;
    if (!bypass.empty()) {
       ASSERT_TRUE(dict.GetBypassList(&value));
       EXPECT_EQ(bypass, value);
     } else {
       EXPECT_FALSE(dict.GetBypassList(&value));
     }

    if (!expected_pac_url.empty()) {
       ASSERT_TRUE(dict.GetPacUrl(&value));
       EXPECT_EQ(expected_pac_url, value);
     } else {
       EXPECT_FALSE(dict.GetPacUrl(&value));
     }

    if (!expected_server.empty()) {
      ASSERT_TRUE(dict.GetProxyServer(&value));
      EXPECT_EQ(expected_server, value);
    } else {
      EXPECT_FALSE(dict.GetProxyServer(&value));
    }
  }

  void ExpectNoSettings(PrefService* pref_service) {
    const PrefService::Preference* pref =
        pref_service->FindPreference(proxy_config::prefs::kProxy);
    ASSERT_TRUE(pref != nullptr);
    EXPECT_FALSE(pref->IsExtensionControlled());
  }

  bool SetIsIncognitoEnabled(bool enabled) {
    ResultCatcher catcher;
    extensions::util::SetIsIncognitoEnabled(
        GetSingleLoadedExtension()->id(), browser()->profile(), enabled);
    if (!catcher.GetNextResult()) {
      message_ = catcher.message();
      return false;
    }
    return true;
  }

  extensions::ManagementPolicy* GetManagementPolicy() {
    return ExtensionSystem::Get(profile())->management_policy();
  }
};

// Tests direct connection settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyDirectSettings) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/direct", {}, {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);

  // As the extension is executed with incognito permission, the settings
  // should propagate to incognito mode.
  pref_service = browser()
                     ->profile()
                     ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     ->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests that proxy settings are changed appropriately when the extension is
// disabled or enabled.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, SettingsChangeOnDisableEnable) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/direct", {}, {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);

  DisableExtension(extension->id());
  ExpectNoSettings(pref_service);

  EnableExtension(extension->id());
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests that proxy settings corresponding to an extension are removed when
// the extension is uninstalled.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, SettingsRemovedOnUninstall) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/direct", {}, {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);

  UninstallExtension(extension->id());
  ExpectNoSettings(pref_service);
}

// Tests that proxy settings corresponding to an extension are removed when
// the extension is blocklisted by management policy. Regression test for
// crbug.com/709264.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
                       PRE_SettingsRemovedOnPolicyBlocklist) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/direct", {}, {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);

  GetManagementPolicy()->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::PROHIBIT_LOAD);
  GetManagementPolicy()->RegisterProvider(&provider);

  // Run the policy check.
  extension_service()->CheckManagementPolicy();
  ExpectNoSettings(pref_service);

  // Remove the extension from policy blocklist. It should get enabled again.
  GetManagementPolicy()->UnregisterAllProviders();
  extension_service()->CheckManagementPolicy();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);

  // Block the extension again for the next test.
  GetManagementPolicy()->RegisterProvider(&provider);
  extension_service()->CheckManagementPolicy();
  ExpectNoSettings(pref_service);
}

// Tests that proxy settings corresponding to an extension take effect again
// on browser restart, when the extension is removed from the policy blocklist.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, SettingsRemovedOnPolicyBlocklist) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_DIRECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests auto-detect settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyAutoSettings) {
  ASSERT_TRUE(RunExtensionTest("proxy/auto", {}, {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_AUTO_DETECT, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyPacScript) {
  ASSERT_TRUE(RunExtensionTest("proxy/pac")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer, kNoBypass,
                   "http://wpad/windows.pac", pref_service);

  // As the extension is not executed with incognito permission, the settings
  // should not propagate to incognito mode.
  pref_service = browser()
                     ->profile()
                     ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     ->GetPrefs();
  ExpectNoSettings(pref_service);

  // Now we enable the extension in incognito mode and verify that settings
  // are applied.
  ASSERT_TRUE(SetIsIncognitoEnabled(true));
  ValidateSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer, kNoBypass,
                   "http://wpad/windows.pac", pref_service);

  // Disabling incognito permission should revoke the settings for incognito
  // mode.
  ASSERT_TRUE(SetIsIncognitoEnabled(false));
  ExpectNoSettings(pref_service);
}

// Tests PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyPacDataUrl) {
  ASSERT_TRUE(RunExtensionTest("proxy/pacdataurl")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  const char url[] =
       "data:;base64,ZnVuY3Rpb24gRmluZFByb3h5R"
       "m9yVVJMKHVybCwgaG9zdCkgewogIGlmIChob3N0ID09ICdmb29iYXIuY29tJykKICAgIHJl"
       "dHVybiAnUFJPWFkgYmxhY2tob2xlOjgwJzsKICByZXR1cm4gJ0RJUkVDVCc7Cn0=";
  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer, kNoBypass,
                   url, pref_service);
}

// Tests PAC proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyPacData) {
  ASSERT_TRUE(RunExtensionTest("proxy/pacdata")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);
  const char url[] =
      "data:application/x-ns-proxy-autoconfig;base64,ZnVuY3Rpb24gRmluZFByb3h5R"
      "m9yVVJMKHVybCwgaG9zdCkgewogIGlmIChob3N0ID09ICdmb29iYXIuY29tJykKICAgIHJl"
      "dHVybiAnUFJPWFkgYmxhY2tob2xlOjgwJzsKICByZXR1cm4gJ0RJUkVDVCc7Cn0=";
  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_PAC_SCRIPT, kNoServer, kNoBypass,
                   url, pref_service);
}

// Tests setting a single proxy to cover all schemes.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedSingle) {
  ASSERT_TRUE(RunExtensionTest("proxy/single")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                 "127.0.0.1:100",
                 kNoBypass,
                 kNoPac,
                 pref_service);
}

// Tests setting to use the system's proxy settings.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxySystem) {
  ASSERT_TRUE(RunExtensionTest("proxy/system")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_SYSTEM, kNoServer, kNoBypass, kNoPac,
                   pref_service);
}

// Tests setting separate proxies for each scheme.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedIndividual) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/individual", {}, {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"   // http:// is pruned.
                   "https=2.2.2.2:80;"  // http:// is pruned.
                   "ftp=3.3.3.3:9000;"  // http:// is pruned.
                   "socks=socks4://4.4.4.4:9090",
                   kNoBypass, kNoPac, pref_service);

  // Now check the incognito preferences.
  pref_service = browser()
                     ->profile()
                     ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     ->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"
                   "https=2.2.2.2:80;"
                   "ftp=3.3.3.3:9000;"
                   "socks=socks4://4.4.4.4:9090",
                   kNoBypass, kNoPac, pref_service);
}

// Tests setting values only for incognito mode
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
                       ProxyFixedIndividualIncognitoOnly) {
  ASSERT_TRUE(RunExtensionTest("proxy/individual_incognito_only", {},
                               {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ExpectNoSettings(pref_service);

  // Now check the incognito preferences.
  pref_service = browser()
                     ->profile()
                     ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     ->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"
                       "https=socks5://2.2.2.2:1080;"
                       "ftp=3.3.3.3:9000;"
                       "socks=socks4://4.4.4.4:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);
}

// Tests setting values also for incognito mode
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
                       ProxyFixedIndividualIncognitoAlso) {
  ASSERT_TRUE(RunExtensionTest("proxy/individual_incognito_also", {},
                               {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80;"
                       "https=socks5://2.2.2.2:1080;"
                       "ftp=3.3.3.3:9000;"
                       "socks=socks4://4.4.4.4:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);

  // Now check the incognito preferences.
  pref_service = browser()
                     ->profile()
                     ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     ->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=5.5.5.5:80;"
                       "https=socks5://6.6.6.6:1080;"
                       "ftp=7.7.7.7:9000;"
                       "socks=socks4://8.8.8.8:9090",
                   kNoBypass,
                   kNoPac,
                   pref_service);
}

// Tests setting and unsetting values
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyFixedIndividualRemove) {
  ASSERT_TRUE(RunExtensionTest("proxy/individual_remove")) << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ExpectNoSettings(pref_service);
}

IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest,
    ProxyBypass) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/bypass", {}, {.allow_in_incognito = true}))
      << message_;
  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(extension);

  PrefService* pref_service = browser()->profile()->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80",
                   "localhost,::1,foo.bar,<local>",
                   kNoPac,
                   pref_service);

  // Now check the incognito preferences.
  pref_service = browser()
                     ->profile()
                     ->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                     ->GetPrefs();
  ValidateSettings(ProxyPrefs::MODE_FIXED_SERVERS,
                   "http=1.1.1.1:80",
                   "localhost,::1,foo.bar,<local>",
                   kNoPac,
                   pref_service);
}

// This test sets the HTTP proxy to an unreachable host "does.not.exist" and
// then attempts to fetch "example.test", expecting the listeners of
// chrome.proxy.onProxyError to fire with ERR_PROXY_CONNECTION_FAILED.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyEventsInvalidProxy) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/events", {.extension_url = "invalid_proxy.html"}))
      << message_;
}

// Tests error events: PAC script parse error.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyEventsParseError) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/events", {.extension_url = "parse_error.html"}))
      << message_;
}

// Tests that chrome.proxy.onProxyError is NOT called in the case of a
// non-proxy error.
IN_PROC_BROWSER_TEST_F(ProxySettingsApiTest, ProxyEventsOtherError) {
  ASSERT_TRUE(
      RunExtensionTest("proxy/events", {.extension_url = "other_error.html"}))
      << message_;
}

}  // namespace extensions
