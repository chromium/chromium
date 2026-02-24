// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "services/network/test/test_url_loader_factory.h"

namespace extensions {
namespace {

constexpr char kAffiliationId[] = "affiliation-id";

// Manifest key for the Endpoint Verification extension found at
// chrome.google.com/webstore/detail/callobklhcbilhphinckomhgkigmfocg
// This extension is authorized to use the chrome.proxyOverrideRulesPrivate API.
constexpr char kAuthorizedManifestKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAjXwWSZq5RLuM5ZbmRWn4gXwpMOb52a"
    "oOhtzIsmbXUWPQeA6/D2p1uaPxIHh6EusxAhXMrBgNaJv1QFxCxiU1aGDlmCR9mOsA7rK5kmVC"
    "i0TYLbQa+C38UDmyhRACrvHO26Jt8qC8oM8yiSuzgb+16rgCCcek9dP7IaHaoJMsBMAEf3VEno"
    "4xt+kCAAsFsyFCB4plWid54avqpgg6+OsR3ZtUAMWooVziJHVmBTiyl82QR5ZURYr+TjkiljkP"
    "EBLaMTKC2g7tUl2h0Q1UmMTMc2qxLIVVREhr4q9iOegNxfNy78BaxZxI1Hjp0EVYMZunIEI9r1"
    "k0vyyaH13TvdeqNwIDAQAB";

// Manifest key for the Google Translate extension found at
// chrome.google.com/webstore/detail/aapbdbdomjkkjkaonfhkkikfgjllcleb
// This extension is unauthorized to use the chrome.proxyOverrideRulesPrivate
// API.
constexpr char kUnauthorizedManifestKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCfHy1M+jghaHyaVAILzx/c/Dy+RXtcaP9/5p"
    "C7EY8JlNEI/G4DIIng9IzlrH8UWStpMWMyGUsdyusn2PkYFrqfVzhc2azVF3PX9D0KHG3FLN3m"
    "Noz1YTBHvO5QSXJf292qW0tTYuoGqeTfXtF9odLdg20Xd0YrLmtS4TQkpSYGDwIDAQAB";

constexpr char kManifestTemplate[] = R"(
    {
      "key": "%s",
      "name": "Proxy Override Rules Private API Test",
      "version": "0.1",
      "manifest_version": 3,
      "permissions": [
          "proxyOverrideRulesPrivate"
      ],
      "background": { "service_worker": "background.js" }
    })";

}  // namespace

class ProxyOverrideRulesPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  ProxyOverrideRulesPrivateApiTest() {
    browser_dm_token_storage_.SetClientId("client_id");
    browser_dm_token_storage_.SetEnrollmentToken("enrollment_token");
    browser_dm_token_storage_.SetDMToken("dm_token");
    policy::BrowserDMTokenStorage::SetForTesting(&browser_dm_token_storage_);
  }

  ~ProxyOverrideRulesPrivateApiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void RunTest(const std::string& background_js,
               bool authorized_manifest_key = true) {
    ResultCatcher result_catcher;
    TestExtensionDir test_dir;
    test_dir.WriteManifest(base::StringPrintf(
        kManifestTemplate, authorized_manifest_key ? kAuthorizedManifestKey
                                                   : kUnauthorizedManifestKey));

    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background_js);

    const Extension* extension = LoadExtension(
        test_dir.UnpackedPath(), {.ignore_manifest_warnings = true});
    ASSERT_TRUE(extension);
    ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  }

  void SetProxyOverrideRulesPolicy(const base::ListValue& rules) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kProxyOverrideRules,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(rules.Clone()),
                 nullptr);
    policies.SetUserAffiliationIds({kAffiliationId});
    provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return profile()->GetPrefs()->GetList(
                 proxy_config::prefs::kProxyOverrideRules) == rules;
    }));
  }

 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    extensions::ExtensionApiTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);

    // Set device org's affiliated IDs.
    auto* browser_policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    auto browser_policy_data =
        std::make_unique<enterprise_management::PolicyData>();
    browser_policy_data->add_device_affiliation_ids(kAffiliationId);
    browser_policy_manager->core()->store()->set_policy_data_for_testing(
        std::move(browser_policy_data));
  }

  void TearDownOnMainThread() override {
    extensions::ExtensionApiTest::TearDownOnMainThread();
    // Must be destroyed before the Profile.
    identity_test_env_profile_adaptor_.reset();
  }

  policy::ProfilePolicyConnector* profile_policy_connector() {
    return profile()->GetProfilePolicyConnector();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;

  network::TestURLLoaderFactory test_url_loader_factory_;

  policy::FakeBrowserDMTokenStorage browser_dm_token_storage_;

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest, ProxyOverrideRules) {
  // Test that the setting can be set and retrieved.
  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function setAndGet() {
        const rule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.example.com"],
          Conditions: [{
            DnsProbe: {
              Host: "corp.ads",
              Result: "resolved"
            }
          }]
        };

        chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
          chrome.test.assertNoLastError();
          chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(1, details.value.length);
            chrome.test.assertEq("google.com",
                                 details.value[0].DestinationMatchers[0]);
            chrome.test.assertEq("https://proxy.example.com",
                                 details.value[0].ProxyList[0]);
            chrome.test.assertEq("corp.ads",
                                 details.value[0].Conditions[0].DnsProbe.Host);
            chrome.test.assertEq("resolved",
                                 details.value[0].Conditions[0].DnsProbe.Result);
            chrome.test.assertEq("controlled_by_this_extension",
                                 details.levelOfControl);

            chrome.proxyOverrideRulesPrivate.rules.clear({}, () => {
              chrome.test.assertNoLastError();
              chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
                chrome.test.assertNoLastError();
                chrome.test.assertEq(0, details.value.length);
                chrome.test.assertEq("controllable_by_this_extension",
                                     details.levelOfControl);
                chrome.test.succeed();
              });
            });
          });
        });
      }
    ]);
  )";
  RunTest(kTest);
}

IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_OnChange) {
  // Test that the onChange event is fired.
  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function onChange() {
        const rule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.example.com"]
        };

        chrome.proxyOverrideRulesPrivate.rules.onChange.addListener((details) => {
          chrome.test.assertEq(1, details.value.length);
          chrome.test.assertEq("google.com",
                               details.value[0].DestinationMatchers[0]);
          chrome.test.assertEq("https://proxy.example.com",
                               details.value[0].ProxyList[0]);
          chrome.test.succeed();
        });

        chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
          chrome.test.assertNoLastError();
        });
      }
    ]);
  )";
  RunTest(kTest);
}

IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_Unauthorized) {
  // Test that an unauthorized extension cannot use the API.
  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function unauthorized() {
        chrome.test.assertEq(undefined, chrome.proxyOverrideRulesPrivate);
        chrome.test.succeed();
      }
    ]);
  )";
  RunTest(kTest, /*authorized_manifest_key=*/false);
}

IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_ManagedByPolicy) {
  base::DictValue rule;
  rule.Set("DestinationMatchers", base::ListValue().Append("managed.com"));
  rule.Set("ProxyList", base::ListValue().Append("https://managed-proxy.com"));
  SetProxyOverrideRulesPolicy(base::ListValue().Append(std::move(rule)));

  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function managedByPolicy() {
        chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq(1, details.value.length);
          chrome.test.assertEq("managed.com",
                               details.value[0].DestinationMatchers[0]);
          chrome.test.assertEq("not_controllable", details.levelOfControl);

          const extensionRule = {
            DestinationMatchers: ["google.com"],
            ProxyList: ["https://proxy.example.com"]
          };

          // Try to set value, should not overwrite policy.
          chrome.proxyOverrideRulesPrivate.rules.set({value: [extensionRule]},
                                                     () => {
            chrome.test.assertNoLastError();
            chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
              chrome.test.assertEq(1, details.value.length);
              chrome.test.assertEq("managed.com",
                                   details.value[0].DestinationMatchers[0]);
              chrome.test.assertEq("not_controllable", details.levelOfControl);

              // Try to clear value, should not overwrite policy.
              chrome.proxyOverrideRulesPrivate.rules.clear({}, () => {
                chrome.test.assertNoLastError();
                chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
                  chrome.test.assertEq(1, details.value.length);
                  chrome.test.assertEq("managed.com",
                                       details.value[0].DestinationMatchers[0]);
                  chrome.test.succeed();
                });
              });
            });
          });
        });
      }
    ]);
  )";
  RunTest(kTest);
}

// `ProxyOverrideRules` policy should be able to overwrite extension-set
// preference value.
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_PolicyOverwritesExtension) {
  ResultCatcher result_catcher;
  // Set an initial set of proxy override rules via extension, this should have
  // lower priority than Chrome policies.
  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function asyncAssertions() {
        const extensionRule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.example.com"]
        };
        chrome.proxyOverrideRulesPrivate.rules.set({value: [extensionRule]}, () => {
          chrome.test.assertNoLastError();
          chrome.test.sendMessage("ready_for_policy", (reply) => {
            chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
              chrome.test.assertNoLastError();
              chrome.test.assertEq(1, details.value.length);
              chrome.test.assertEq("managed.com", details.value[0].DestinationMatchers[0]);
              chrome.test.assertEq("not_controllable", details.levelOfControl);
              chrome.test.succeed();
            });
          });
        });
      }
    ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifestTemplate, kAuthorizedManifestKey));

  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kTest);

  ExtensionTestMessageListener listener("ready_for_policy",
                                        ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(
      test_dir.UnpackedPath(), {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Apply policy.
  base::DictValue rule;
  rule.Set("DestinationMatchers", base::ListValue().Append("managed.com"));
  rule.Set("ProxyList", base::ListValue().Append("https://managed-proxy.com"));
  SetProxyOverrideRulesPolicy(base::ListValue().Append(std::move(rule)));

  listener.Reply("proceed");

  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_InvalidInput) {
  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function invalidProxyString() {
        const rule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["invalid-proxy-string"]
        };
        chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
          chrome.test.assertLastError("Invalid proxy: invalid-proxy-string");
          chrome.test.succeed();
        });
      },
      function invalidDnsProbeHost() {
        const rule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.com"],
          Conditions: [{
            DnsProbe: {
              Host: "invalid host",
              Result: "resolved"
            }
          }]
        };
        chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
          chrome.test.assertLastError("Invalid DnsProbe Host: invalid host");
          chrome.test.succeed();
        });
      },
      function emptyCondition() {
        const rule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.com"],
          Conditions: [{}]
        };
        chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
          chrome.test.assertLastError(
              "Each condition must have exactly one probe type.");
          chrome.test.succeed();
        });
      },
      function multipleProxiesOneInvalid() {
        const rule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.com", "invalid-proxy"]
        };
        chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
          chrome.test.assertLastError("Invalid proxy: invalid-proxy");
          chrome.test.succeed();
        });
      },
      function multipleRulesOneInvalid() {
        const validRule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.com"]
        };
        const invalidRule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["invalid-proxy"]
        };
        chrome.proxyOverrideRulesPrivate.rules.set(
            {value: [validRule, invalidRule]}, () => {
          chrome.test.assertLastError("Invalid proxy: invalid-proxy");
          chrome.test.succeed();
        });
      }
    ]);
  )";
  RunTest(kTest);
}

}  // namespace extensions
