// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

using testing::_;
using testing::Contains;
using testing::Key;
using testing::Return;
using testing::SizeIs;

constexpr const char kGoogleDns[] = "https://dns.google/dns-query{?dns}";
constexpr const char kCloudflareDns[] =
    "https://chrome.cloudflare-dns.com/dns-query";

class MockDoHTemplatesUriResolver
    : public dns_over_https::TemplatesUriResolver {
 public:
  MockDoHTemplatesUriResolver() = default;
  MOCK_METHOD1(UpdateFromPrefs, void(PrefService*));
  MOCK_METHOD0(GetDohWithIdentifiersActive, bool());
  MOCK_METHOD0(GetEffectiveTemplates, std::string());
  MOCK_METHOD0(GetDisplayTemplates, std::string());
};

void OnGetProperties(bool* success_out,
                     std::map<std::string, std::string>* props_out,
                     base::OnceClosure callback,
                     absl::optional<base::Value> result) {
  *success_out = result.has_value();
  if (result) {
    base::Value* value = result->FindKeyOfType(
        shill::kDNSProxyDOHProvidersProperty, base::Value::Type::DICTIONARY);
    if (value != nullptr) {
      for (const auto kv : value->DictItems()) {
        props_out->emplace(kv.first, kv.second.GetString());
      }
    }
  }
  std::move(callback).Run();
}

std::map<std::string, std::string> GetDOHProviders() {
  bool success = false;
  std::map<std::string, std::string> props;
  ShillManagerClient* shill_manager = ShillManagerClient::Get();
  base::RunLoop run_loop;
  shill_manager->GetProperties(
      base::BindOnce(&OnGetProperties, base::Unretained(&success),
                     base::Unretained(&props), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
  return props;
}

class SecureDnsManagerTest : public testing::Test {
 public:
  SecureDnsManagerTest() = default;

  SecureDnsManagerTest(const SecureDnsManagerTest&) = delete;
  SecureDnsManagerTest& operator=(const SecureDnsManagerTest&) = delete;

  void SetUp() override {
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsMode,
                                                 SecureDnsConfig::kModeOff);
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsTemplates,
                                                 "");
    pref_service_.registry()->RegisterStringPref(
        prefs::kDnsOverHttpsTemplatesWithIdentifiers, "");
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsSalt, "");
    network_handler_test_helper_.RegisterPrefs(pref_service_.registry(),
                                               local_state_.registry());
    network_handler_test_helper_.InitializePrefs(&pref_service_, &local_state_);
  }

  void TearDown() override { NetworkHandler::Get()->ShutdownPrefServices(); }

  PrefService* pref_service() { return &pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
  TestingPrefServiceSimple pref_service_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(SecureDnsManagerTest, SetModeOff) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeOff));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.empty());
}

TEST_F(SecureDnsManagerTest, SetModeOffIgnoresTemplates) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeOff));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.empty());
}

TEST_F(SecureDnsManagerTest, SetModeSecure) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  const auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_TRUE(it->second.empty());
  EXPECT_EQ(providers.size(), 1u);
}

TEST_F(SecureDnsManagerTest, SetModeSecureMultipleTemplates) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(
      prefs::kDnsOverHttpsTemplates,
      base::Value("https://dns.google/dns-query{?dns}  "
                  "https://chrome.cloudflare-dns.com/dns-query "));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.find(kGoogleDns) != providers.end());
  EXPECT_TRUE(providers.find(kCloudflareDns) != providers.end());
  EXPECT_EQ(providers.size(), 2u);
}

TEST_F(SecureDnsManagerTest, SetModeAutomaticWithTemplates) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(
      prefs::kDnsOverHttpsTemplates,
      base::Value("https://dns.google/dns-query{?dns}  "
                  "https://chrome.cloudflare-dns.com/dns-query "));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_FALSE(it->second.empty());
  it = providers.find(kCloudflareDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_FALSE(it->second.empty());
  EXPECT_EQ(providers.size(), 2u);
}

// Tests that the `DoHTemplatesUriResolver` resolver is called when secure DNS
// prefs change and that the result, provided by `GetEffectiveTemplates` is
// read.
TEST_F(SecureDnsManagerTest, DoHTemplatesUriResolverCalled) {
  constexpr char effectiveTemplate[] = "effectiveTemplate";
  // The test will update the four prefs that `SecureDnsManager` is observing.
  constexpr int prefUpdatesCallCount = 4;

  MockDoHTemplatesUriResolver* templateUriResolver =
      new MockDoHTemplatesUriResolver();
  EXPECT_CALL(*templateUriResolver, UpdateFromPrefs(_))
      .Times(prefUpdatesCallCount);
  EXPECT_CALL(*templateUriResolver, GetEffectiveTemplates())
      .Times(prefUpdatesCallCount)
      .WillRepeatedly(Return(effectiveTemplate));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  secure_dns_manager->SetDoHTemplatesUriResolverForTesting(
      base::WrapUnique(templateUriResolver));

  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(
      prefs::kDnsOverHttpsTemplates,
      base::Value("https://dns.google/dns-query{?dns}  "
                  "https://chrome.cloudflare-dns.com/dns-query "));
  pref_service()->Set(
      prefs::kDnsOverHttpsTemplatesWithIdentifiers,
      base::Value("https://dns.google/dns-query{?dns}  "
                  "https://chrome.cloudflare-dns.com/dns-query "));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("testsalt"));

  auto providers = GetDOHProviders();

  EXPECT_THAT(providers, SizeIs(1));
  EXPECT_THAT(providers, Contains(Key(effectiveTemplate)));
}

TEST_F(SecureDnsManagerTest, NetworkMetadataStoreHasDohWithIdentifiersActive) {
  // Setup an active user.
  user_manager::FakeUserManager* fake_user_manager =
      new user_manager::FakeUserManager();
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager =
      std::make_unique<user_manager::ScopedUserManager>(
          base::WrapUnique(fake_user_manager));
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId("test-user@testdomain.com", "1234567890"));
  fake_user_manager->AddUser(account_id);

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value("https://dns.google/dns-query{?dns}"));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("testsalt"));

  auto providers = GetDOHProviders();

  EXPECT_TRUE(NetworkHandler::Get()
                  ->network_metadata_store()
                  ->secure_dns_templates_with_identifiers_active());

  pref_service()->ClearPref(prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  providers = GetDOHProviders();

  EXPECT_FALSE(NetworkHandler::Get()
                   ->network_metadata_store()
                   ->secure_dns_templates_with_identifiers_active());
}

}  // namespace
}  // namespace ash
