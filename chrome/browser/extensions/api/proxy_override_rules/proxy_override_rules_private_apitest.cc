// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
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
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "services/network/test/test_url_loader_factory.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
      "version": "0.%d",
      "manifest_version": 3,
      "permissions": [
          "proxyOverrideRulesPrivate",
          "privacy"
      ],
      "background": { "service_worker": "background.js" }
    })";

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kVerifyControllableAndSetRuleJs[] = R"(
    chrome.test.runTests([
      function canSetAndControl() {
        chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq(0, details.value.length);
          chrome.test.assertEq("controllable_by_this_extension",
                               details.levelOfControl);

          const rule = {
            DestinationMatchers: ["google.com"],
            ProxyList: ["https://proxy.example.com"]
          };

          chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
            chrome.test.assertNoLastError();
            chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
              chrome.test.assertNoLastError();
              chrome.test.assertEq(1, details.value.length);
              chrome.test.assertEq("google.com",
                                   details.value[0].DestinationMatchers[0]);
              chrome.test.assertEq("controlled_by_this_extension",
                                   details.levelOfControl);
              chrome.test.succeed();
            });
          });
        });
      }
    ]);
  )";
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

class ProxyOverrideRulesPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  ProxyOverrideRulesPrivateApiTest() {
#if !BUILDFLAG(IS_CHROMEOS)
    browser_dm_token_storage_.SetClientId("client_id");
    browser_dm_token_storage_.SetEnrollmentToken("enrollment_token");
    browser_dm_token_storage_.SetDMToken("dm_token");
    policy::BrowserDMTokenStorage::SetForTesting(&browser_dm_token_storage_);
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

  ~ProxyOverrideRulesPrivateApiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  const base::ListValue& GetProxyOverrideRulesPref() {
    return profile()->GetPrefs()->GetList(
        proxy_config::prefs::kProxyOverrideRules);
  }

  const Extension* LoadTestExtension(std::string_view background_js,
                                     bool authorized_manifest_key = true,
                                     bool allow_in_incognito = false) {
    auto test_dir = std::make_unique<TestExtensionDir>();
    test_dir->WriteManifest(base::StringPrintf(kManifestTemplate,
                                               authorized_manifest_key
                                                   ? kAuthorizedManifestKey
                                                   : kUnauthorizedManifestKey,
                                               1));
    test_dir->WriteFile(FILE_PATH_LITERAL("background.js"), background_js);
    const Extension* extension = LoadExtension(
        test_dir->UnpackedPath(), {.allow_in_incognito = allow_in_incognito,
                                   .ignore_manifest_warnings = true});
    test_dirs_.push_back(std::move(test_dir));
    return extension;
  }

  void RunTest(std::string_view background_js,
               bool authorized_manifest_key = true) {
    ResultCatcher result_catcher;
    ASSERT_TRUE(LoadTestExtension(background_js, authorized_manifest_key));
    ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  }

  void SetProxyOverrideRulesPolicy(const base::ListValue& rules) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kProxyOverrideRules,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(rules.Clone()),
                 nullptr);
    policies.SetDeviceAffiliationIds({kAffiliationId});
    policies.SetUserAffiliationIds({kAffiliationId});
    provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return GetProxyOverrideRulesPref() == rules; }));
  }

  void SetManagedProxyOverrideRulePolicy() {
    base::DictValue rule;
    rule.Set("DestinationMatchers", base::ListValue().Append("managed.com"));
    rule.Set("ProxyList",
             base::ListValue().Append("https://managed-proxy.com"));
    SetProxyOverrideRulesPolicy(base::ListValue().Append(std::move(rule)));
  }

  void SetPolicyState(bool affiliated,
                      std::optional<int> enable_for_all_users = std::nullopt) {
    policy::PolicyMap policies;
    policies.SetDeviceAffiliationIds({kAffiliationId});
    if (affiliated) {
      policies.SetUserAffiliationIds({kAffiliationId});
    } else {
      policies.SetUserAffiliationIds({"unaffiliated-id"});
    }
#if !BUILDFLAG(IS_CHROMEOS)
    if (enable_for_all_users.has_value()) {
      policies.Set(policy::key::kEnableProxyOverrideRulesForAllUsers,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                   policy::POLICY_SOURCE_CLOUD,
                   base::Value(*enable_for_all_users), nullptr);
    }
#endif  // !BUILDFLAG(IS_CHROMEOS)
    provider_.UpdateChromePolicy(policies);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      const policy::PolicyMap& active_policies =
          profile_policy_connector()->policy_service()->GetPolicies(
              policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                      std::string()));
      if (active_policies.GetDeviceAffiliationIds().empty() ||
          active_policies.IsUserAffiliated() != affiliated) {
        return false;
      }
#if !BUILDFLAG(IS_CHROMEOS)
      if (profile()->GetPrefs()->GetInteger(
              proxy_config::prefs::kEnableProxyOverrideRulesForAllUsers) !=
          enable_for_all_users.value_or(0)) {
        return false;
      }
#endif  // !BUILDFLAG(IS_CHROMEOS)
      return true;
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

#if !BUILDFLAG(IS_CHROMEOS)
    // Set device org's affiliated IDs.
    auto* browser_policy_manager =
        g_browser_process->browser_policy_connector()
            ->machine_level_user_cloud_policy_manager();
    auto browser_policy_data =
        std::make_unique<enterprise_management::PolicyData>();
    browser_policy_data->add_device_affiliation_ids(kAffiliationId);
    browser_policy_manager->core()->store()->set_policy_data_for_testing(
        std::move(browser_policy_data));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

  void TearDownOnMainThread() override {
    test_dirs_.clear();
    extensions::ExtensionApiTest::TearDownOnMainThread();
    // Must be destroyed before the Profile.
    identity_test_env_profile_adaptor_.reset();
  }

#if !BUILDFLAG(IS_CHROMEOS)
  void VerifyDynamicModifiabilityClearsAndNotifies(
      base::FunctionRef<void()> revoke_access,
      base::FunctionRef<void()> restore_access) {
    ResultCatcher result_catcher;
    constexpr char kTest[] = R"(
      chrome.test.runTests([
        function dynamicModifiabilityTransitions() {
          const rule = {
            DestinationMatchers: ["google.com"],
            ProxyList: ["https://proxy.example.com"]
          };

          function verifyEmptyWithControl(details, expectedControl, next) {
            chrome.test.assertEq(0, details.value.length);
            chrome.test.assertEq(expectedControl, details.levelOfControl);
            chrome.proxyOverrideRulesPrivate.rules.get({}, (getDetails) => {
              chrome.test.assertNoLastError();
              chrome.test.assertEq(0, getDetails.value.length);
              chrome.test.assertEq(expectedControl, getDetails.levelOfControl);
              next();
            });
          }

          let step = 0;
          chrome.proxyOverrideRulesPrivate.rules.onChange.addListener(
              (details) => {
            step++;
            if (step === 1) {
              chrome.test.assertEq(1, details.value.length);
              chrome.test.assertEq("controlled_by_this_extension",
                                   details.levelOfControl);
              chrome.test.sendMessage("ready_for_revoke");
            } else if (step === 2) {
              verifyEmptyWithControl(details, "not_controllable", () => {
                chrome.test.sendMessage("ready_for_restore");
              });
            } else if (step === 3) {
              verifyEmptyWithControl(
                  details, "controllable_by_this_extension",
                  chrome.test.succeed);
            }
          });

          chrome.proxyOverrideRulesPrivate.rules.get({}, (initial) => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(0, initial.value.length);
            chrome.test.assertEq("controllable_by_this_extension",
                                 initial.levelOfControl);
            chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
              chrome.test.assertNoLastError();
            });
          });
        }
      ]);
    )";

    ExtensionTestMessageListener revoke_listener("ready_for_revoke");
    ExtensionTestMessageListener restore_listener("ready_for_restore");
    ASSERT_TRUE(LoadTestExtension(kTest));

    ASSERT_TRUE(revoke_listener.WaitUntilSatisfied());
    EXPECT_EQ(1u, GetProxyOverrideRulesPref().size());

    revoke_access();
    ASSERT_TRUE(restore_listener.WaitUntilSatisfied());
    EXPECT_TRUE(GetProxyOverrideRulesPref().empty());

    restore_access();
    ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  policy::ProfilePolicyConnector* profile_policy_connector() {
    return profile()->GetProfilePolicyConnector();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;

  network::TestURLLoaderFactory test_url_loader_factory_;

#if !BUILDFLAG(IS_CHROMEOS)
  policy::FakeBrowserDMTokenStorage browser_dm_token_storage_;
#endif  // !BUILDFLAG(IS_CHROMEOS)

  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override_{
          policy::PlatformManagementService::GetInstance(),
          policy::EnterpriseManagementAuthority::NONE};

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  std::vector<std::unique_ptr<TestExtensionDir>> test_dirs_;
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
  SetManagedProxyOverrideRulePolicy();

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

  ExtensionTestMessageListener listener("ready_for_policy",
                                        ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadTestExtension(kTest));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  SetManagedProxyOverrideRulePolicy();
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

IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_Unaffiliated) {
  SetPolicyState(/*affiliated=*/false);

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, extensions can always set and control ProxyOverrideRules
  // regardless of user affiliation.
  RunTest(kVerifyControllableAndSetRuleJs);
  EXPECT_EQ(GetProxyOverrideRulesPref().size(), 1u);
#else
  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function unaffiliatedCannotSetOrControl() {
        chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
          chrome.test.assertNoLastError();
          chrome.test.assertEq(0, details.value.length);
          chrome.test.assertEq("not_controllable", details.levelOfControl);

          const rule = {
            DestinationMatchers: ["google.com"],
            ProxyList: ["https://proxy.example.com"]
          };

          chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
            chrome.test.assertLastError(
                "Cannot set the 'proxyOverrideRulesPrivate' preference " +
                "because it is not controllable by extensions.");
            chrome.proxyOverrideRulesPrivate.rules.get({}, (details) => {
              chrome.test.assertNoLastError();
              chrome.test.assertEq(0, details.value.length);
              chrome.test.assertEq("not_controllable", details.levelOfControl);
              chrome.test.succeed();
            });
          });
        });
      }
    ]);
  )";
  RunTest(kTest);
  EXPECT_TRUE(GetProxyOverrideRulesPref().empty());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_DynamicAffiliationClearsAndNotifies) {
  SetPolicyState(/*affiliated=*/true);

#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, dynamically becoming unaffiliated should not clear extension
  // rules.
  RunTest(kVerifyControllableAndSetRuleJs);
  EXPECT_EQ(1u, GetProxyOverrideRulesPref().size());
  SetPolicyState(/*affiliated=*/false);
  EXPECT_EQ(1u, GetProxyOverrideRulesPref().size());
#else
  VerifyDynamicModifiabilityClearsAndNotifies(
      /*revoke_access=*/[&] { SetPolicyState(/*affiliated=*/false); },
      /*restore_access=*/[&] { SetPolicyState(/*affiliated=*/true); });
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ProxyOverrideRulesPrivateApiTest,
                       ProxyOverrideRules_UnaffiliatedEnabledForAllUsers) {
  SetPolicyState(/*affiliated=*/false, /*enable_for_all_users=*/1);
  VerifyDynamicModifiabilityClearsAndNotifies(
      /*revoke_access=*/[&] { SetPolicyState(/*affiliated=*/false, 0); },
      /*restore_access=*/[&] { SetPolicyState(/*affiliated=*/false, 1); });
}

IN_PROC_BROWSER_TEST_F(
    ProxyOverrideRulesPrivateApiTest,
    ProxyOverrideRules_IncognitoDynamicModifiabilityClearsAndNotifies) {
  SetPolicyState(/*affiliated=*/true);

  ResultCatcher result_catcher;
  constexpr char kTest[] = R"(
    chrome.test.runTests([
      function incognitoDynamicModifiabilityTransitions() {
        const regularRule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.example.com"]
        };
        const incognitoRule = {
          DestinationMatchers: ["incognito.example.com"],
          ProxyList: ["https://incognito-proxy.example.com"]
        };

        let events = [];
        function expectPairAndAdvance(expectedControl, next) {
          chrome.test.assertEq(2, events.length);
          const regularEvent =
              events.find((e) => !("incognitoSpecific" in e));
          const incognitoEvent =
              events.find((e) => "incognitoSpecific" in e);
          chrome.test.assertTrue(!!regularEvent);
          chrome.test.assertTrue(!!incognitoEvent);
          chrome.test.assertEq(0, regularEvent.value.length);
          chrome.test.assertEq(expectedControl, regularEvent.levelOfControl);
          chrome.test.assertEq(0, incognitoEvent.value.length);
          chrome.test.assertEq(false, incognitoEvent.incognitoSpecific);
          chrome.test.assertEq(expectedControl, incognitoEvent.levelOfControl);
          events = [];

          chrome.proxyOverrideRulesPrivate.rules.get(
              {incognito: true}, (details) => {
            chrome.test.assertNoLastError();
            chrome.test.assertEq(0, details.value.length);
            chrome.test.assertEq(false, details.incognitoSpecific);
            chrome.test.assertEq(expectedControl, details.levelOfControl);
            next();
          });
        }

        chrome.proxyOverrideRulesPrivate.rules.set(
            {value: [regularRule], scope: "regular"}, () => {
          chrome.test.assertNoLastError();
          chrome.proxyOverrideRulesPrivate.rules.set(
              {value: [incognitoRule], scope: "incognito_persistent"}, () => {
            chrome.test.assertNoLastError();

            let phase = "revoke";
            chrome.proxyOverrideRulesPrivate.rules.onChange.addListener(
                (details) => {
              events.push(details);
              if (events.length < 2) {
                return;
              }
              if (phase === "revoke") {
                phase = "restore";
                expectPairAndAdvance("not_controllable", () => {
                  chrome.test.sendMessage("ready_for_restore");
                });
              } else {
                expectPairAndAdvance("controllable_by_this_extension",
                                     chrome.test.succeed);
              }
            });

            chrome.test.sendMessage("ready_for_revoke");
          });
        });
      }
    ]);
  )";

  ExtensionTestMessageListener revoke_listener("ready_for_revoke");
  ExtensionTestMessageListener restore_listener("ready_for_restore");
  ASSERT_TRUE(LoadTestExtension(kTest, /*authorized_manifest_key=*/true,
                                /*allow_in_incognito=*/true));

  ASSERT_TRUE(revoke_listener.WaitUntilSatisfied());
  SetPolicyState(/*affiliated=*/false);
  ASSERT_TRUE(restore_listener.WaitUntilSatisfied());
  EXPECT_TRUE(GetProxyOverrideRulesPref().empty());

  SetPolicyState(/*affiliated=*/true);
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Verifies that extension-controlled POR rules persisted in `ExtensionPrefs`
// are cleared on affiliation loss (and scrubbed on unaffiliated startup) even
// when no `rules.onChange` listener is registered, without affecting unrelated
// extension-controlled preferences.
class ProxyOverrideRulesPrivatePersistenceApiTest
    : public ProxyOverrideRulesPrivateApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ProxyOverrideRulesPrivateApiTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    policies.SetDeviceAffiliationIds({kAffiliationId});
    if (content::IsPreTest()) {
      policies.SetUserAffiliationIds({kAffiliationId});
    } else {
      policies.SetUserAffiliationIds({"unaffiliated-id"});
    }
    provider_.UpdateChromePolicy(policies);
  }

 protected:
  // Installs (or updates) an unpacked extension inside the profile directory so
  // it persists across browser restarts, setting both a POR rule and an
  // unrelated `chrome.privacy` preference via `onInstalled` without registering
  // any `onChange` listener.
  void SetPersistedPrefsWithoutOnChangeListener(int version) {
    constexpr char kSetPrefsOnInstallJs[] = R"(
      chrome.runtime.onInstalled.addListener(() => {
        const rule = {
          DestinationMatchers: ["google.com"],
          ProxyList: ["https://proxy.example.com"]
        };
        chrome.privacy.services.searchSuggestEnabled.set({value: false}, () => {
          chrome.test.assertNoLastError();
          chrome.proxyOverrideRulesPrivate.rules.set({value: [rule]}, () => {
            chrome.test.assertNoLastError();
            chrome.test.sendMessage("prefs_set");
          });
        });
      });
    )";

    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath ext_dir =
        profile()->GetPath().AppendASCII("persisted_por_extension");
    ASSERT_TRUE(base::CreateDirectory(ext_dir));
    ASSERT_TRUE(
        base::WriteFile(ext_dir.Append(FILE_PATH_LITERAL("manifest.json")),
                        base::StringPrintf(kManifestTemplate,
                                           kAuthorizedManifestKey, version)));
    ASSERT_TRUE(
        base::WriteFile(ext_dir.Append(FILE_PATH_LITERAL("background.js")),
                        kSetPrefsOnInstallJs));

    ExtensionTestMessageListener prefs_set_listener("prefs_set");
    ASSERT_TRUE(LoadExtension(ext_dir, {.ignore_manifest_warnings = true}));
    ASSERT_TRUE(prefs_set_listener.WaitUntilSatisfied());
  }

  void ExpectUnrelatedExtensionPrefIntact() {
    const PrefService::Preference* pref =
        profile()->GetPrefs()->FindPreference(prefs::kSearchSuggestEnabled);
    ASSERT_TRUE(pref);
    EXPECT_TRUE(pref->IsExtensionControlled());
    EXPECT_FALSE(
        profile()->GetPrefs()->GetBoolean(prefs::kSearchSuggestEnabled));
  }
};

IN_PROC_BROWSER_TEST_F(
    ProxyOverrideRulesPrivatePersistenceApiTest,
    PRE_PRE_ProxyOverrideRules_ClearsWithoutListenerAndAcrossRestarts) {
  SetPersistedPrefsWithoutOnChangeListener(/*version=*/1);
  EXPECT_EQ(1u, GetProxyOverrideRulesPref().size());
  ExpectUnrelatedExtensionPrefIntact();

  // In-session unaffiliation clears POR rules even without an `onChange`
  // listener while leaving unrelated extension-controlled prefs untouched.
  SetPolicyState(/*affiliated=*/false);
  EXPECT_TRUE(GetProxyOverrideRulesPref().empty());
  ExpectUnrelatedExtensionPrefIntact();
  SetPolicyState(/*affiliated=*/true);
  EXPECT_TRUE(GetProxyOverrideRulesPref().empty());

  // Persist a fresh rule for the next session.
  SetPersistedPrefsWithoutOnChangeListener(/*version=*/2);
  EXPECT_EQ(1u, GetProxyOverrideRulesPref().size());
}

IN_PROC_BROWSER_TEST_F(
    ProxyOverrideRulesPrivatePersistenceApiTest,
    PRE_ProxyOverrideRules_ClearsWithoutListenerAndAcrossRestarts) {
  // Persisted rules remain active while affiliated on startup, then are cleared
  // if the profile becomes unaffiliated without calling `rules.set()` again.
  EXPECT_EQ(1u, GetProxyOverrideRulesPref().size());
  ExpectUnrelatedExtensionPrefIntact();
  SetPolicyState(/*affiliated=*/false);
  EXPECT_TRUE(GetProxyOverrideRulesPref().empty());
  ExpectUnrelatedExtensionPrefIntact();

  // Persist a fresh rule so the final session starts with stored rules while
  // already unaffiliated.
  SetPolicyState(/*affiliated=*/true);
  SetPersistedPrefsWithoutOnChangeListener(/*version=*/3);
  EXPECT_EQ(1u, GetProxyOverrideRulesPref().size());
}

IN_PROC_BROWSER_TEST_F(
    ProxyOverrideRulesPrivatePersistenceApiTest,
    ProxyOverrideRules_ClearsWithoutListenerAndAcrossRestarts) {
  // Persisted POR rules from the previous session are scrubbed on startup when
  // the profile is already unaffiliated, while unrelated extension prefs stay
  // intact.
  EXPECT_TRUE(GetProxyOverrideRulesPref().empty());
  ExpectUnrelatedExtensionPrefIntact();
  SetPolicyState(/*affiliated=*/true);
  EXPECT_TRUE(GetProxyOverrideRulesPref().empty());
  ExpectUnrelatedExtensionPrefIntact();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
