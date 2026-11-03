// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <utility>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/test/test_future.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/platform_management_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_change_registrar.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
const char kMockPolicyName[] = "AllowFileSelectionDialogs";

// Returns the policy entry for `key` in `policy_map`, even if marked ignored.
const policy::PolicyMap::Entry* GetRawPolicyEntry(
    const policy::PolicyMap& policy_map,
    const std::string& key) {
  for (const auto& [name, entry] : policy_map) {
    if (name == key) {
      return &entry;
    }
  }
  return nullptr;
}

// Simulates a management status provider that performs a blocking operation
// to retrieve the device's management authority during initial startup.
class MockBlockingManagementStatusProvider
    : public policy::ManagementStatusProvider {
 public:
  MockBlockingManagementStatusProvider()
      : policy::ManagementStatusProvider(
            policy::policy_prefs::kAzureActiveDirectoryDeviceManagement) {}
  ~MockBlockingManagementStatusProvider() override = default;

  MOCK_METHOD(policy::EnterpriseManagementAuthority,
              FetchAuthority,
              (),
              (override));
};

// A mock management status provider that allows setting GMock expectations on
// `FetchAuthority()`, which may be called from background task threads.
class MockManagementStatusProvider : public policy::ManagementStatusProvider {
 public:
  MockManagementStatusProvider()
      : policy::ManagementStatusProvider(
            policy::policy_prefs::kAzureActiveDirectoryDeviceManagement) {}
  ~MockManagementStatusProvider() override = default;

  MOCK_METHOD(policy::EnterpriseManagementAuthority,
              FetchAuthority,
              (),
              (override));
};

// Observes `PolicyService` and waits for a specific policy to be loaded and not
// ignored.
class PolicyUpdateObserver : public policy::PolicyService::Observer {
 public:
  PolicyUpdateObserver(policy::PolicyService* policy_service,
                       std::string policy_name)
      : policy_service_(CHECK_DEREF(policy_service)),
        policy_name_(std::move(policy_name)) {
    policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
  }
  ~PolicyUpdateObserver() override {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  }

  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override {
    if (ns.domain == policy::POLICY_DOMAIN_CHROME) {
      const policy::PolicyMap::Entry* entry = current.Get(policy_name_);
      if (entry && !entry->ignored() && !future_.IsReady()) {
        future_.SetValue();
      }
    }
  }

  void WaitForPolicyUpdate() { EXPECT_TRUE(future_.Wait()); }

 private:
  const raw_ref<policy::PolicyService> policy_service_;
  const std::string policy_name_;
  base::test::TestFuture<void> future_;
};
#endif

void VerifyLocalState() {
  const PrefService* prefs = g_browser_process->local_state();
  ASSERT_NE(nullptr, prefs);
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kAllowFileSelectionDialogs);
  ASSERT_NE(nullptr, pref);
  EXPECT_FALSE(pref->IsDefaultValue());
}

class ChromeBrowserMainExtraPartsPolicyValueChecker
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsPolicyValueChecker() = default;
  ChromeBrowserMainExtraPartsPolicyValueChecker(
      const ChromeBrowserMainExtraPartsPolicyValueChecker&) = delete;
  ChromeBrowserMainExtraPartsPolicyValueChecker& operator=(
      const ChromeBrowserMainExtraPartsPolicyValueChecker&) = delete;

  // ChromeBrowserMainExtraParts
  void PreCreateThreads() override { VerifyLocalState(); }
  void PreBrowserStart() override { VerifyLocalState(); }
  void PreMainMessageLoopRun() override { VerifyLocalState(); }
};

}  // namespace

// Test if the policy value can be read from the pref properly on Windows.
class PolicyInitializationBrowserTest : public InProcessBrowserTest {
 public:
  PolicyInitializationBrowserTest(const PolicyInitializationBrowserTest&) =
      delete;
  PolicyInitializationBrowserTest& operator=(
      const PolicyInitializationBrowserTest&) = delete;

 protected:
  PolicyInitializationBrowserTest() = default;

  // content::BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    SetUpPlatformPolicyValue();
  }
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        std::make_unique<ChromeBrowserMainExtraPartsPolicyValueChecker>());
  }

 private:
#if BUILDFLAG(IS_WIN)
  // Set up policy value for windows platform
  void SetUpPlatformPolicyValue() {
    HKEY root = HKEY_CURRENT_USER;
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(root));

    base::win::RegKey key;

    ASSERT_EQ(ERROR_SUCCESS, key.Create(root, policy::kRegistryChromePolicyKey,
                                        KEY_SET_VALUE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(base::ASCIIToWide(kMockPolicyName).c_str(), 1));
  }

  registry_util::RegistryOverrideManager registry_override_manager_;
#else
  // This test hasn't supported other platform yet.
  void SetUpPlatformPolicyValue() {}
#endif
};

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(PolicyInitializationBrowserTest, VerifyLocalState) {
  VerifyLocalState();
}

// Verifies that when the device is managed and obtaining management status
// requires an actual blocking call on startup, the blocking call succeeds and
// sensitive policies (like `HomepageLocation`) are honored rather than
// filtered.
class PolicyInitializationWithManagementBrowserTest
    : public InProcessBrowserTest {
 public:
  PolicyInitializationWithManagementBrowserTest() = default;
  ~PolicyInitializationWithManagementBrowserTest() override = default;
  PolicyInitializationWithManagementBrowserTest(
      const PolicyInitializationWithManagementBrowserTest&) = delete;
  PolicyInitializationWithManagementBrowserTest& operator=(
      const PolicyInitializationWithManagementBrowserTest&) = delete;

 protected:
  void SetUp() override {
    std::vector<std::unique_ptr<policy::ManagementStatusProvider>> providers;
    auto provider = std::make_unique<
        testing::StrictMock<MockBlockingManagementStatusProvider>>();
    EXPECT_CALL(*provider, FetchAuthority())
        .Times(testing::AtLeast(1))
        .WillRepeatedly([]() {
          base::AssertBlockingAllowed();
          return policy::EnterpriseManagementAuthority::CLOUD_DOMAIN;
        });
    providers.push_back(std::move(provider));
    policy::PlatformManagementService::GetInstance()
        ->SetManagementStatusProviderForTesting(std::move(providers));
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    policy::PlatformManagementService::GetInstance()->UsePrefServiceAsCache(
        nullptr);
    policy::PlatformManagementService::GetInstance()
        ->SetManagementStatusProviderForTesting({});
    InProcessBrowserTest::TearDown();
  }

  void SetUpInProcessBrowserTestFixture() override {
    HKEY root = HKEY_CURRENT_USER;
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(root));

    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Create(root, policy::kRegistryChromePolicyKey,
                                        KEY_SET_VALUE | KEY_WOW64_32KEY));
    // Write sensitive policy `HomepageLocation` and non-sensitive
    // `ShowHomeButton`.
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(
                  base::ASCIIToWide(policy::key::kHomepageLocation).c_str(),
                  L"https://example.com"));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(
                  base::ASCIIToWide(policy::key::kShowHomeButton).c_str(), 1));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
  // Set to false/nullopt to mock unmanaged state for isolated testing and to
  // pre-load management DLLs on the main thread to avoid UI thread blocking.
  base::win::ScopedDomainStateForTesting scoped_domain_{false};
  base::win::ScopedDeviceRegisteredWithManagementForTesting scoped_management_{
      false};
  base::win::ScopedAzureADJoinStateForTesting scoped_azure_ad_{std::nullopt};
};

IN_PROC_BROWSER_TEST_F(PolicyInitializationWithManagementBrowserTest,
                       SensitivePolicyLoadedWithBlockingCall) {
  // Verify that the sensitive policy was loaded and applied to preferences.
  EXPECT_EQ("https://example.com/",
            browser()->GetProfile()->GetPrefs()->GetString(prefs::kHomePage));

  // Also verify that the sensitive policy entry in `PolicyService` is not
  // ignored.
  const policy::PolicyNamespace chrome_ns(policy::POLICY_DOMAIN_CHROME, "");
  const policy::PolicyMap& policies =
      g_browser_process->policy_service()->GetPolicies(chrome_ns);
  const policy::PolicyMap::Entry* homepage_entry =
      policies.Get(policy::key::kHomepageLocation);
  ASSERT_TRUE(homepage_entry);
  EXPECT_FALSE(homepage_entry->ignored());
  ASSERT_NE(homepage_entry->value(base::Value::Type::STRING), nullptr);
  EXPECT_EQ(base::Value("https://example.com"),
            *homepage_entry->value(base::Value::Type::STRING));

  // Verify non-sensitive policy is also applied.
  EXPECT_TRUE(
      browser()->GetProfile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
}

// Verifies that sensitive policies are blocked when management trustworthiness
// does not allow them, and loaded once trustworthiness changes to allow them.
class PolicyInitializationTrustworthinessChangeBrowserTest
    : public InProcessBrowserTest {
 public:
  PolicyInitializationTrustworthinessChangeBrowserTest() = default;
  ~PolicyInitializationTrustworthinessChangeBrowserTest() override = default;
  PolicyInitializationTrustworthinessChangeBrowserTest(
      const PolicyInitializationTrustworthinessChangeBrowserTest&) = delete;
  PolicyInitializationTrustworthinessChangeBrowserTest& operator=(
      const PolicyInitializationTrustworthinessChangeBrowserTest&) = delete;

 protected:
  void SetUp() override {
    auto provider =
        std::make_unique<testing::NiceMock<MockManagementStatusProvider>>();
    mock_provider_ = provider.get();
    ON_CALL(*mock_provider_, FetchAuthority())
        .WillByDefault(
            testing::Return(policy::EnterpriseManagementAuthority::NONE));
    std::vector<std::unique_ptr<policy::ManagementStatusProvider>> providers;
    providers.push_back(std::move(provider));
    policy::PlatformManagementService::GetInstance()
        ->SetManagementStatusProviderForTesting(std::move(providers));
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    mock_provider_ = nullptr;
    policy::PlatformManagementService::GetInstance()->UsePrefServiceAsCache(
        nullptr);
    policy::PlatformManagementService::GetInstance()
        ->SetManagementStatusProviderForTesting({});
    InProcessBrowserTest::TearDown();
  }

  void SetUpInProcessBrowserTestFixture() override {
    HKEY root = HKEY_CURRENT_USER;
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(root));

    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Create(root, policy::kRegistryChromePolicyKey,
                                        KEY_SET_VALUE | KEY_WOW64_32KEY));
    // Write sensitive policy `HomepageLocation` and non-sensitive
    // `ShowHomeButton`.
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(
                  base::ASCIIToWide(policy::key::kHomepageLocation).c_str(),
                  L"https://example.com"));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(
                  base::ASCIIToWide(policy::key::kShowHomeButton).c_str(), 1));
  }

  raw_ptr<MockManagementStatusProvider> mock_provider_ = nullptr;

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
  // Set to false/nullopt to mock unmanaged state for isolated testing and to
  // pre-load management DLLs on the main thread to avoid UI thread blocking.
  base::win::ScopedDomainStateForTesting scoped_domain_{false};
  base::win::ScopedDeviceRegisteredWithManagementForTesting scoped_management_{
      false};
  base::win::ScopedAzureADJoinStateForTesting scoped_azure_ad_{std::nullopt};
};

IN_PROC_BROWSER_TEST_F(PolicyInitializationTrustworthinessChangeBrowserTest,
                       SensitivePoliciesLoadedWhenTrustworthinessChanges) {
  const policy::PolicyNamespace chrome_ns(policy::POLICY_DOMAIN_CHROME, "");
  policy::PolicyService* policy_service = g_browser_process->policy_service();

  // Verify that initially, when trustworthiness is `NONE`, sensitive policy
  // `HomepageLocation` is ignored and not applied to preferences.
  const policy::PolicyMap& initial_policies =
      policy_service->GetPolicies(chrome_ns);
  EXPECT_EQ(nullptr, initial_policies.Get(policy::key::kHomepageLocation));
  const policy::PolicyMap::Entry* homepage_raw_entry =
      GetRawPolicyEntry(initial_policies, policy::key::kHomepageLocation);
  ASSERT_TRUE(homepage_raw_entry);
  EXPECT_TRUE(homepage_raw_entry->ignored());
  EXPECT_NE("https://example.com/",
            browser()->GetProfile()->GetPrefs()->GetString(prefs::kHomePage));

  // Non-sensitive policy `ShowHomeButton` is loaded and applied.
  const policy::PolicyMap::Entry* show_home_entry =
      initial_policies.Get(policy::key::kShowHomeButton);
  ASSERT_TRUE(show_home_entry);
  EXPECT_FALSE(show_home_entry->ignored());
  EXPECT_TRUE(
      browser()->GetProfile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  // Set up observer to deterministically wait for the sensitive policy update.
  PolicyUpdateObserver observer(policy_service, policy::key::kHomepageLocation);

  // Wait for the profile's `PrefService` to apply the reloaded policy.
  PrefChangeRegistrar pref_change_registrar;
  pref_change_registrar.Init(browser()->GetProfile()->GetPrefs());
  base::test::TestFuture<void> homepage_pref_future;
  pref_change_registrar.Add(prefs::kHomePage,
                            homepage_pref_future.GetRepeatingCallback());

  // Change management authority trustworthiness to allow sensitive policies.
  EXPECT_CALL(*mock_provider_, FetchAuthority())
      .WillRepeatedly(
          testing::Return(policy::EnterpriseManagementAuthority::CLOUD_DOMAIN));

  // Trigger the management service cache refresh.
  g_browser_process->browser_policy_connector()->RefreshManagementServiceCache(
      policy::PlatformManagementService::GetInstance());

  // Deterministically wait for platform policies to be reloaded.
  observer.WaitForPolicyUpdate();
  ASSERT_TRUE(homepage_pref_future.Wait());

  // Assert that `HomepageLocation` is now loaded, not ignored, and applied.
  const policy::PolicyMap& updated_policies =
      policy_service->GetPolicies(chrome_ns);
  const policy::PolicyMap::Entry* updated_homepage_entry =
      updated_policies.Get(policy::key::kHomepageLocation);
  ASSERT_TRUE(updated_homepage_entry);
  EXPECT_FALSE(updated_homepage_entry->ignored());
  ASSERT_NE(updated_homepage_entry->value(base::Value::Type::STRING), nullptr);
  EXPECT_EQ(base::Value("https://example.com"),
            *updated_homepage_entry->value(base::Value::Type::STRING));
  EXPECT_EQ("https://example.com/",
            browser()->GetProfile()->GetPrefs()->GetString(prefs::kHomePage));
}
#endif
