// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include "base/functional/bind.h"
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
#include "chromeos/ash/components/network/network_ui_data.h"
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
constexpr const char kMultipleTemplates[] =
    "https://dns.google/dns-query{?dns}  "
    "https://chrome.cloudflare-dns.com/dns-query ";

class MockDoHTemplatesUriResolver
    : public dns_over_https::TemplatesUriResolver {
 public:
  MockDoHTemplatesUriResolver() = default;
  MOCK_METHOD(void, Update, (PrefService*), (override));
  MOCK_METHOD(bool, GetDohWithIdentifiersActive, (), (override));
  MOCK_METHOD(std::string, GetEffectiveTemplates, (), (override));
  MOCK_METHOD(std::string, GetDisplayTemplates, (), (override));
};

void OnGetProperties(bool* success_out,
                     std::map<std::string, std::string>* props_out,
                     base::OnceClosure callback,
                     std::optional<base::Value::Dict> result) {
  *success_out = result.has_value();
  if (result) {
    base::Value::Dict* value =
        result->FindDict(shill::kDNSProxyDOHProvidersProperty);
    if (value != nullptr) {
      for (const auto kv : *value) {
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
    pref_service_.registry()->RegisterStringPref(
        prefs::kDnsOverHttpsEffectiveTemplatesChromeOS, "");
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsSalt, "");
    network_handler_test_helper_.RegisterPrefs(pref_service_.registry(),
                                               local_state_.registry());
    network_handler_test_helper_.InitializePrefs(&pref_service_, &local_state_);
    network_handler_test_helper_.AddDefaultProfiles();
  }

  void TearDown() override { NetworkHandler::Get()->ShutdownPrefServices(); }

  void ChangeNetworkOncSource(const std::string& path,
                              ::onc::ONCSource onc_source) {
    std::unique_ptr<ash::NetworkUIData> ui_data =
        ash::NetworkUIData::CreateFromONC(onc_source);
    network_handler_test_helper_.SetServiceProperty(
        path, shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));
  }

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
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      "");
}

TEST_F(SecureDnsManagerTest, SetModeOffIgnoresTemplates) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeOff));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.empty());
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      "");
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
  pref_service()->Set(prefs::kDnsOverHttpsTemplates,
                      base::Value(kMultipleTemplates));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.find(kGoogleDns) != providers.end());
  EXPECT_TRUE(providers.find(kCloudflareDns) != providers.end());
  EXPECT_EQ(providers.size(), 2u);
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      kMultipleTemplates);
}

TEST_F(SecureDnsManagerTest, SetModeSecureWithFallback) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  const auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_EQ(it->second, "*");
  EXPECT_EQ(providers.size(), 1u);
}

TEST_F(SecureDnsManagerTest, SetModeSecureWithFallbackMultipleTemplates) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates,
                      base::Value(kMultipleTemplates));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.find(kGoogleDns) != providers.end());
  EXPECT_TRUE(providers.find(kCloudflareDns) != providers.end());
  EXPECT_EQ(providers.size(), 2u);
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      kMultipleTemplates);
}

TEST_F(SecureDnsManagerTest, SetModeAutomaticWithTemplates) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates,
                      base::Value(kMultipleTemplates));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();

  auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_FALSE(it->second.empty());
  it = providers.find(kCloudflareDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_FALSE(it->second.empty());
  EXPECT_EQ(providers.size(), 2u);
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      kMultipleTemplates);
}

// Tests that the `DoHTemplatesUriResolver` resolver is called when secure DNS
// prefs change and that the result, provided by `GetEffectiveTemplates` is
// read.
TEST_F(SecureDnsManagerTest, DoHTemplatesUriResolverCalled) {
  constexpr char effectiveTemplate[] = "effectiveTemplate";
  // The test will update the four prefs that `SecureDnsManager` is observing.
  constexpr int prefUpdatesCallCount = 4;

  std::unique_ptr<MockDoHTemplatesUriResolver> template_uri_resolver =
      std::make_unique<MockDoHTemplatesUriResolver>();
  EXPECT_CALL(*template_uri_resolver, Update(_)).Times(prefUpdatesCallCount);
  EXPECT_CALL(*template_uri_resolver, GetEffectiveTemplates())
      .Times(prefUpdatesCallCount)
      .WillRepeatedly(Return(effectiveTemplate));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  secure_dns_manager->SetDoHTemplatesUriResolverForTesting(
      std::move(template_uri_resolver));

  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates,
                      base::Value(kMultipleTemplates));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kMultipleTemplates));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("testsalt"));

  auto providers = GetDOHProviders();

  EXPECT_THAT(providers, SizeIs(1));
  EXPECT_THAT(providers, Contains(Key(effectiveTemplate)));
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      effectiveTemplate);
}

TEST_F(SecureDnsManagerTest, NetworkMetadataStoreHasDohWithIdentifiersActive) {
  // Setup an active user.
  auto fake_user_manager_owned =
      std::make_unique<user_manager::FakeUserManager>();
  user_manager::FakeUserManager* fake_user_manager =
      fake_user_manager_owned.get();
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager =
      std::make_unique<user_manager::ScopedUserManager>(
          std::move(fake_user_manager_owned));
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

TEST_F(SecureDnsManagerTest, kDnsOverHttpsEffectiveTemplatesChromeOS) {
  // Setup an active user.
  auto fake_user_manager_owned =
      std::make_unique<user_manager::FakeUserManager>();
  user_manager::FakeUserManager* fake_user_manager =
      fake_user_manager_owned.get();
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager =
      std::make_unique<user_manager::ScopedUserManager>(
          std::move(fake_user_manager_owned));
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId("test-user@testdomain.com", "1234567890"));
  fake_user_manager->AddUser(account_id);

  constexpr char kUriTemplateWithIdentifiers[] =
      "https://dns.google.alternativeuri/"
      "${USER_EMAIL}/{?dns}";

  constexpr char kEffectiveUriTemplateWithIdentifiers[] =
      "https://dns.google.alternativeuri/"
      "B07D2C5D119EB1881671C3B8D84CBE4FE3595C0C9ECBBF7670B18DDFDA072F66/{?dns}";

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kUriTemplateWithIdentifiers));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto providers = GetDOHProviders();

  // Verify that the value of kDnsOverHttpsEffectiveTemplatesChromeOS pref is
  // prefs::kDnsOverHttpsTemplatesWithIdentifiers with the hex encoded hashed
  // value of the user identifier.
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      kEffectiveUriTemplateWithIdentifiers);

  pref_service()->ClearPref(prefs::kDnsOverHttpsTemplatesWithIdentifiers);

  providers = GetDOHProviders();

  // Verify that the value of kDnsOverHttpsEffectiveTemplatesChromeOS pref is
  // prefs::kDnsOverHttpsTemplates since the URI template with identifiers pref
  // was cleared.
  EXPECT_EQ(
      pref_service()->GetString(prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
      kGoogleDns);
}

TEST_F(SecureDnsManagerTest, DefaultNetworkObservedForIpAddressPlaceholder) {
  constexpr char kUriTemplateWithEmail[] =
      "https://dns.google.alternativeuri/"
      "${USER_EMAIL}/{?dns}";
  constexpr char kUriTemplateWithIp[] =
      "https://dns.google.alternativeuri/"
      "${DEVICE_IP_ADDRESSES}/{?dns}";

  int expected_uri_template_update_count = 0;
  int actual_uri_template_update_count = 0;

  std::unique_ptr<MockDoHTemplatesUriResolver> template_uri_resolver =
      std::make_unique<MockDoHTemplatesUriResolver>();

  ON_CALL(*template_uri_resolver, Update(_))
      .WillByDefault(testing::Invoke([&actual_uri_template_update_count]() {
        actual_uri_template_update_count++;
      }));
  EXPECT_CALL(*template_uri_resolver, GetDohWithIdentifiersActive())
      .WillRepeatedly(testing::Return(true));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  secure_dns_manager->SetDoHTemplatesUriResolverForTesting(
      std::move(template_uri_resolver));

  EXPECT_EQ(actual_uri_template_update_count,
            expected_uri_template_update_count);

  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kUriTemplateWithEmail));
  // Each pref update above will trigger an update request for the URI
  // templates.
  expected_uri_template_update_count = 2;
  EXPECT_EQ(actual_uri_template_update_count,
            expected_uri_template_update_count);

  const ash::NetworkState* network =
      ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  // Default network changes should not trigger a re-evaluation of the templates
  // URI if the DoH policy is not configured to use the device IP addresses.
  EXPECT_EQ(actual_uri_template_update_count,
            expected_uri_template_update_count);

  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kUriTemplateWithIp));
  EXPECT_EQ(actual_uri_template_update_count,
            ++expected_uri_template_update_count);

  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  EXPECT_EQ(actual_uri_template_update_count,
            ++expected_uri_template_update_count);
}

TEST_F(SecureDnsManagerTest, DefaultTemplateUrisForwardedToShill) {
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  auto providers = GetDOHProviders();
  // The content of the provider list depends on the current country.
  EXPECT_FALSE(providers.empty());
}

class MockPropertyChangeObserver : public ash::ShillPropertyChangedObserver {
 public:
  MockPropertyChangeObserver() = default;
  ~MockPropertyChangeObserver() override = default;
  MOCK_METHOD(void,
              OnPropertyChanged,
              (const std::string& name, const base::Value& value),
              (override));
};

TEST_F(SecureDnsManagerTest, NoDuplicateShillPropertyUpdateRequests) {
  constexpr char kTemplateUri1[] = "https://dns.google1.com";
  constexpr char kTemplateUri2[] = "https://dns.google2.com";
  constexpr char kEffectiveTemplateUri[] = "https://dns.google2.com";

  // The call to update the property kDNSProxyDOHProvidersProperty should be
  // invoked just once because the mock TemplateUriResolver always returns the
  // same DoH providers.
  testing::StrictMock<MockPropertyChangeObserver> observer;
  EXPECT_CALL(observer, OnPropertyChanged(shill::kDNSProxyDOHProvidersProperty,
                                          testing::_))
      .Times(1);

  ash::ShillManagerClient* shill_manager_client =
      ash::ShillManagerClient::Get();
  shill_manager_client->AddPropertyChangedObserver(&observer);

  int actual_uri_template_update_count = 0;

  std::unique_ptr<MockDoHTemplatesUriResolver> template_uri_resolver =
      std::make_unique<MockDoHTemplatesUriResolver>();

  ON_CALL(*template_uri_resolver, Update(_))
      .WillByDefault(testing::Invoke([&actual_uri_template_update_count]() {
        actual_uri_template_update_count++;
      }));
  EXPECT_CALL(*template_uri_resolver, GetDohWithIdentifiersActive())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*template_uri_resolver, GetEffectiveTemplates())
      .WillRepeatedly(testing::Return(kEffectiveTemplateUri));

  auto secure_dns_manager = std::make_unique<SecureDnsManager>(pref_service());
  secure_dns_manager->SetDoHTemplatesUriResolverForTesting(
      std::move(template_uri_resolver));

  EXPECT_EQ(actual_uri_template_update_count, 0);

  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeAutomatic));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateUri1));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateUri2));
  // Verify that every pref update above will trigger an update request for the
  // DoH providers.
  EXPECT_EQ(actual_uri_template_update_count, 3);
  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace ash
