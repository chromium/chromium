// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include "ash/constants/ash_pref_names.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
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

void OnGetDictProperties(const std::string& prop_name,
                         bool* success_out,
                         std::map<std::string, std::string>* props_out,
                         base::OnceClosure callback,
                         std::optional<base::Value::Dict> result) {
  *success_out = result.has_value();
  if (result) {
    base::Value::Dict* value = result->FindDict(prop_name);
    if (value != nullptr) {
      for (const auto kv : *value) {
        props_out->emplace(kv.first, kv.second.GetString());
      }
    }
  }
  std::move(callback).Run();
}

void OnGetStringsProperties(const std::string& prop_name,
                            bool* success_out,
                            std::vector<std::string>* props_out,
                            base::OnceClosure callback,
                            std::optional<base::Value::Dict> result) {
  *success_out = result.has_value();
  if (result) {
    base::Value::List* value = result->FindList(prop_name);
    if (value != nullptr) {
      for (const auto& e : *value) {
        props_out->push_back(e.GetString());
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
      base::BindOnce(&OnGetDictProperties, shill::kDNSProxyDOHProvidersProperty,
                     base::Unretained(&success), base::Unretained(&props),
                     run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
  return props;
}

std::vector<std::string> GetDOHIncludedDomains() {
  bool success = false;
  std::vector<std::string> domains;
  ShillManagerClient* shill_manager = ShillManagerClient::Get();
  base::RunLoop run_loop;
  shill_manager->GetProperties(base::BindOnce(
      &OnGetStringsProperties, shill::kDOHIncludedDomainsProperty,
      base::Unretained(&success), base::Unretained(&domains),
      run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
  return domains;
}

std::vector<std::string> GetDOHExcludedDomains() {
  bool success = false;
  std::vector<std::string> domains;
  ShillManagerClient* shill_manager = ShillManagerClient::Get();
  base::RunLoop run_loop;
  shill_manager->GetProperties(base::BindOnce(
      &OnGetStringsProperties, shill::kDOHExcludedDomainsProperty,
      base::Unretained(&success), base::Unretained(&domains),
      run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
  return domains;
}

class SecureDnsManagerObserver : public SecureDnsManager::Observer {
 public:
  explicit SecureDnsManagerObserver(
      raw_ptr<SecureDnsManager> secure_dns_manager)
      : secure_dns_manager_(secure_dns_manager) {
    secure_dns_manager_->AddObserver(this);
  }

  SecureDnsManagerObserver(const SecureDnsManagerObserver&) = delete;
  SecureDnsManagerObserver& operator=(const SecureDnsManagerObserver&) = delete;
  ~SecureDnsManagerObserver() override {
    if (secure_dns_manager_) {
      secure_dns_manager_->RemoveObserver(this);
    }
  }
  std::string doh_mode() const { return doh_mode_; }
  std::string doh_template_uri() const { return doh_template_uri_; }

 private:
  void OnTemplateUrisChanged(const std::string& template_uris) override {
    doh_template_uri_ = template_uris;
  }
  void OnModeChanged(const std::string& mode) override { doh_mode_ = mode; }
  void OnSecureDnsManagerShutdown() override {
    secure_dns_manager_->RemoveObserver(this);
    secure_dns_manager_ = nullptr;
  }

  raw_ptr<SecureDnsManager> secure_dns_manager_;
  std::string doh_mode_;
  std::string doh_template_uri_;
};

class SecureDnsManagerTest : public testing::Test {
 public:
  SecureDnsManagerTest() = default;

  SecureDnsManagerTest(const SecureDnsManagerTest&) = delete;
  SecureDnsManagerTest& operator=(const SecureDnsManagerTest&) = delete;

  void SetUp() override {
    SecureDnsManager::RegisterProfilePrefs(profile_prefs_.registry());
    SecureDnsManager::RegisterLocalStatePrefs(local_state_.registry());

    local_state_.registry()->RegisterStringPref(::prefs::kDnsOverHttpsMode,
                                                SecureDnsConfig::kModeOff);
    local_state_.registry()->RegisterStringPref(::prefs::kDnsOverHttpsTemplates,
                                                "");
    local_state_.registry()->RegisterStringPref(
        ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS, "");
    local_state_.registry()->RegisterListPref(
        prefs::kDnsOverHttpsExcludedDomains, base::Value::List());
    local_state_.registry()->RegisterListPref(
        prefs::kDnsOverHttpsIncludedDomains, base::Value::List());
    local_state_.registry()->RegisterBooleanPref(
        ::prefs::kBuiltInDnsClientEnabled, true);
    local_state_.registry()->RegisterBooleanPref(
        ::prefs::kAdditionalDnsQueryTypesEnabled, true);
    network_handler_test_helper_.RegisterPrefs(profile_prefs_.registry(),
                                               local_state_.registry());
    network_handler_test_helper_.InitializePrefs(&profile_prefs_,
                                                 &local_state_);
    network_handler_test_helper_.AddDefaultProfiles();

    // SystemNetworkContextManager cannot be instantiated here,
    // which normally owns the StubResolverConfigReader instance, so
    // inject a StubResolverConfigReader instance here.
    stub_resolver_config_reader_ =
        std::make_unique<StubResolverConfigReader>(&local_state_);
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        stub_resolver_config_reader_.get());

    secure_dns_manager_ = std::make_unique<SecureDnsManager>(
        local_state(), /*profile_prefs=*/nullptr, /*is_profile_managed=*/true);
    secure_dns_manager_observer_ =
        std::make_unique<SecureDnsManagerObserver>(secure_dns_manager_.get());
  }

  void TearDown() override {
    NetworkHandler::Get()->ShutdownPrefServices();
    secure_dns_manager_observer_.reset();
    secure_dns_manager_.reset();
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        nullptr);
    stub_resolver_config_reader_.reset();
  }

  void ChangeNetworkOncSource(const std::string& path,
                              ::onc::ONCSource onc_source) {
    std::unique_ptr<ash::NetworkUIData> ui_data =
        ash::NetworkUIData::CreateFromONC(onc_source);
    network_handler_test_helper_.SetServiceProperty(
        path, shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));
  }

  void ResetSecureDnsManager() {
    secure_dns_manager_observer_.reset();
    secure_dns_manager_.reset();
  }

  TestingPrefServiceSimple* local_state() { return &local_state_; }
  PrefService* profile_prefs() { return &profile_prefs_; }
  SecureDnsManager* secure_dns_manager() { return secure_dns_manager_.get(); }
  SecureDnsManagerObserver* secure_dns_manager_observer() {
    return secure_dns_manager_observer_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
  std::unique_ptr<StubResolverConfigReader> stub_resolver_config_reader_;
  TestingPrefServiceSimple local_state_;
  TestingPrefServiceSimple profile_prefs_;
  std::unique_ptr<SecureDnsManager> secure_dns_manager_;
  std::unique_ptr<SecureDnsManagerObserver> secure_dns_manager_observer_;
};

TEST_F(SecureDnsManagerTest, SetModeOff) {
  local_state()->Set(::prefs::kDnsOverHttpsMode,
                     base::Value(SecureDnsConfig::kModeOff));

  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.empty());
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
            "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            base::Value(SecureDnsConfig::kModeOff));
}

TEST_F(SecureDnsManagerTest, SetModeOffIgnoresTemplates) {
  local_state()->Set(::prefs::kDnsOverHttpsMode,
                     base::Value(SecureDnsConfig::kModeOff));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.empty());
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
            "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeOff);
}

TEST_F(SecureDnsManagerTest, SetModeSecure) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto providers = GetDOHProviders();

  const auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_TRUE(it->second.empty());
  EXPECT_EQ(providers.size(), 1u);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), kGoogleDns);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeSecure);
}

TEST_F(SecureDnsManagerTest, SetModeSecureMultipleTemplates) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates,
                     base::Value(kMultipleTemplates));

  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.find(kGoogleDns) != providers.end());
  EXPECT_TRUE(providers.find(kCloudflareDns) != providers.end());
  EXPECT_EQ(providers.size(), 2u);
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
            kMultipleTemplates);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(),
            kMultipleTemplates);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeSecure);
}

TEST_F(SecureDnsManagerTest, SetModeSecureWithFallback) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto providers = GetDOHProviders();

  const auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_EQ(it->second, "*");
  EXPECT_EQ(providers.size(), 1u);
}

TEST_F(SecureDnsManagerTest, SetModeSecureWithFallbackMultipleTemplates) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates,
                     base::Value(kMultipleTemplates));

  auto providers = GetDOHProviders();

  EXPECT_TRUE(providers.find(kGoogleDns) != providers.end());
  EXPECT_TRUE(providers.find(kCloudflareDns) != providers.end());
  EXPECT_EQ(providers.size(), 2u);
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
            kMultipleTemplates);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(),
            kMultipleTemplates);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeAutomatic);
}

TEST_F(SecureDnsManagerTest, SetModeAutomaticWithTemplates) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates,
                     base::Value(kMultipleTemplates));

  auto providers = GetDOHProviders();

  auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_FALSE(it->second.empty());
  it = providers.find(kCloudflareDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_FALSE(it->second.empty());
  EXPECT_EQ(providers.size(), 2u);
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
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

  secure_dns_manager()->SetDoHTemplatesUriResolverForTesting(
      std::move(template_uri_resolver));

  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates,
                     base::Value(kMultipleTemplates));
  local_state()->Set(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                     base::Value(kMultipleTemplates));
  local_state()->Set(::prefs::kDnsOverHttpsSalt, base::Value("testsalt"));

  auto providers = GetDOHProviders();

  EXPECT_THAT(providers, SizeIs(1));
  EXPECT_THAT(providers, Contains(Key(effectiveTemplate)));
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
            effectiveTemplate);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(),
            effectiveTemplate);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeAutomatic);
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

  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                     base::Value("https://dns.google/dns-query{?dns}"));
  local_state()->Set(::prefs::kDnsOverHttpsSalt, base::Value("testsalt"));

  auto providers = GetDOHProviders();

  EXPECT_TRUE(NetworkHandler::Get()
                  ->network_metadata_store()
                  ->secure_dns_templates_with_identifiers_active());

  local_state()->ClearPref(::prefs::kDnsOverHttpsTemplatesWithIdentifiers);
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

  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                     base::Value(kUriTemplateWithIdentifiers));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto providers = GetDOHProviders();

  // Verify that the value of kDnsOverHttpsEffectiveTemplatesChromeOS pref is
  // ::prefs::kDnsOverHttpsTemplatesWithIdentifiers with the hex encoded hashed
  // value of the user identifier.
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
            kEffectiveUriTemplateWithIdentifiers);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(),
            kEffectiveUriTemplateWithIdentifiers);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeAutomatic);

  local_state()->ClearPref(::prefs::kDnsOverHttpsTemplatesWithIdentifiers);

  providers = GetDOHProviders();

  // Verify that the value of kDnsOverHttpsEffectiveTemplatesChromeOS pref is
  // prefs::kDnsOverHttpsTemplates since the URI template with identifiers pref
  // was cleared.
  EXPECT_EQ(local_state()->GetString(
                ::prefs::kDnsOverHttpsEffectiveTemplatesChromeOS),
            kGoogleDns);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), kGoogleDns);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeAutomatic);
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

  secure_dns_manager()->SetDoHTemplatesUriResolverForTesting(
      std::move(template_uri_resolver));

  EXPECT_EQ(actual_uri_template_update_count,
            expected_uri_template_update_count);

  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
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

  local_state()->Set(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                     base::Value(kUriTemplateWithIp));
  EXPECT_EQ(actual_uri_template_update_count,
            ++expected_uri_template_update_count);

  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  EXPECT_EQ(actual_uri_template_update_count,
            ++expected_uri_template_update_count);
}

TEST_F(SecureDnsManagerTest, DefaultTemplateUrisForwardedToShill) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
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

  // The call to update the shill properties should be invoked just once. For
  // kDOHIncludedDomainsProperty, the mock TemplateUriResolver always returns
  // the same DoH providers.
  testing::StrictMock<MockPropertyChangeObserver> observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kDOHIncludedDomainsProperty, testing::_))
      .Times(1);
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kDOHExcludedDomainsProperty, testing::_))
      .Times(1);

  // The two calls are generated:
  // 1. at startup, with an empty value
  // 2. after the template URI resolver is configured
  EXPECT_CALL(observer, OnPropertyChanged(shill::kDNSProxyDOHProvidersProperty,
                                          testing::_))
      .Times(2);

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

  secure_dns_manager()->SetDoHTemplatesUriResolverForTesting(
      std::move(template_uri_resolver));

  EXPECT_EQ(actual_uri_template_update_count, 0);

  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                     base::Value(kTemplateUri1));
  local_state()->Set(::prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                     base::Value(kTemplateUri2));
  // Verify that every pref update above will trigger an update request for the
  // DoH providers.
  EXPECT_EQ(actual_uri_template_update_count, 3);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SecureDnsManagerTest, SetDOHIncludedDomains) {
  std::vector<std::string> domains = {"test.com", "*.test.com"};
  base::Value pref_value(base::Value::Type::LIST);
  for (const auto& domain : domains) {
    pref_value.GetList().Append(domain);
  }
  local_state()->Set(prefs::kDnsOverHttpsIncludedDomains, pref_value);

  EXPECT_EQ(domains, GetDOHIncludedDomains());
}

TEST_F(SecureDnsManagerTest, SetDOHExcludedDomains) {
  std::vector<std::string> domains = {"test.com", "*.test.com"};
  base::Value pref_value(base::Value::Type::LIST);
  for (const auto& domain : domains) {
    pref_value.GetList().Append(domain);
  }
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  EXPECT_EQ(domains, GetDOHExcludedDomains());
}
// This test verifies the user-set local_state to user-set profile_prefs
// migration logic for DoH prefs.
TEST_F(SecureDnsManagerTest, LocalStateToProfilePrefMigration) {
  local_state()->Set(::prefs::kDnsOverHttpsMode,
                     base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsMode), "");
  EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsTemplates), "");

  {
    auto consumer_secure_dns_manager = std::make_unique<SecureDnsManager>(
        local_state(), profile_prefs(), /*is_profile_managed=*/false);
    // Verify that the user-set local state prefs are copied to profile prefs
    // for unmanaged users.
    EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsMode),
              SecureDnsConfig::kModeSecure);
    EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsTemplates),
              kGoogleDns);
  }

  profile_prefs()->ClearPref(::prefs::kDnsOverHttpsMode);
  profile_prefs()->ClearPref(::prefs::kDnsOverHttpsTemplates);

  {
    auto consumer_secure_dns_manager = std::make_unique<SecureDnsManager>(
        local_state(), profile_prefs(), /*is_profile_managed=*/true);
    // Verify that the user-set local state prefs are not copied to profile
    // prefs for managed users. SecureDnsConfig::kModeAutomatic is the default
    // value for secure DoH mode.
    EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsMode),
              SecureDnsConfig::kModeAutomatic);
    EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsTemplates), "");
  }

  profile_prefs()->Set(::prefs::kDnsOverHttpsMode,
                       base::Value(SecureDnsConfig::kModeSecure));
  profile_prefs()->Set(::prefs::kDnsOverHttpsTemplates,
                       base::Value(kCloudflareDns));

  {
    auto consumer_secure_dns_manager = std::make_unique<SecureDnsManager>(
        local_state(), profile_prefs(), /*is_profile_managed=*/false);
    // When the profile prefs already have DoH prefs configured, verify that the
    // pref migration will not override them.
    EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsMode),
              SecureDnsConfig::kModeSecure);
    EXPECT_EQ(profile_prefs()->GetString(::prefs::kDnsOverHttpsTemplates),
              kCloudflareDns);
  }
}

// This test verifies that the SecureDnsManager updates observers with the
// correct DoH configuration when the user profile is not managed.
TEST_F(SecureDnsManagerTest, ObserverForUnmanagedUsers) {
  local_state()->Set(::prefs::kDnsOverHttpsMode,
                     base::Value(SecureDnsConfig::kModeAutomatic));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto consumer_secure_dns_manager = std::make_unique<SecureDnsManager>(
      local_state(), profile_prefs(), /*is_profile_managed=*/false);
  auto consumer_observer = std::make_unique<SecureDnsManagerObserver>(
      consumer_secure_dns_manager.get());

  EXPECT_EQ(consumer_observer->doh_template_uri(), kGoogleDns);
  EXPECT_EQ(consumer_observer->doh_mode(), SecureDnsConfig::kModeAutomatic);

  profile_prefs()->Set(::prefs::kDnsOverHttpsMode,
                       base::Value(SecureDnsConfig::kModeSecure));
  profile_prefs()->Set(::prefs::kDnsOverHttpsTemplates,
                       base::Value(kCloudflareDns));

  EXPECT_EQ(consumer_observer->doh_template_uri(), kCloudflareDns);
  EXPECT_EQ(consumer_observer->doh_mode(), SecureDnsConfig::kModeSecure);
}

TEST_F(SecureDnsManagerTest, DohIncludedDomains_ChromeDohConfig) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  // Set DoHIncludedDomains, expect Chrome DoH to be disabled.
  base::Value pref_value(base::Value::Type::LIST);
  pref_value.GetList().Append("test.com");
  local_state()->Set(prefs::kDnsOverHttpsIncludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeOff);

  // Unset DoHIncludedDomains, expect Chrome DoH to be re-enabled.
  pref_value.GetList().clear();
  local_state()->Set(prefs::kDnsOverHttpsIncludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), kGoogleDns);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeSecure);
}

TEST_F(SecureDnsManagerTest, DohExcludedDomains_ChromeDohConfig) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  // Set DoHExcludedDomains, expect Chrome DoH to be disabled.
  base::Value pref_value(base::Value::Type::LIST);
  pref_value.GetList().Append("test.com");
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeOff);

  // Unset DoHExcludedDomains, expect Chrome DoH to be re-enabled.
  pref_value.GetList().clear();
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), kGoogleDns);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeSecure);
}

TEST_F(SecureDnsManagerTest, DohDomainConfig_ChromeDohConfig) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  // Set DoHIncludedDomains, expect Chrome DoH to be disabled.
  base::Value pref_value(base::Value::Type::LIST);
  pref_value.GetList().Append("include.com");
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeOff);

  // Set DoHExcludedDomains, expect Chrome DoH to still be disabled.
  pref_value.GetList().clear();
  pref_value.GetList().Append("exclude.com");
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeOff);

  // Unset DoHIncludedDomains, expect Chrome DoH to still be disabled.
  pref_value.GetList().clear();
  local_state()->Set(prefs::kDnsOverHttpsIncludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), "");
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeOff);

  // Unset DoHExcludedDomains, expect Chrome DoH to be re-enabled.
  pref_value.GetList().clear();
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  EXPECT_EQ(secure_dns_manager_observer()->doh_template_uri(), kGoogleDns);
  EXPECT_EQ(secure_dns_manager_observer()->doh_mode(),
            SecureDnsConfig::kModeSecure);
}

TEST_F(SecureDnsManagerTest, DohIncludedDomains_ShillDohConfig) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  // Set DoHIncludedDomains, expect no change to shill DoH config.
  base::Value pref_value(base::Value::Type::LIST);
  pref_value.GetList().Append("test.com");
  local_state()->Set(prefs::kDnsOverHttpsIncludedDomains, pref_value);

  auto providers = GetDOHProviders();

  auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_TRUE(it->second.empty());
  EXPECT_EQ(providers.size(), 1u);

  // Unset DoHIncludedDomains, expect no change to shill DoH config.
  pref_value.GetList().clear();
  local_state()->Set(prefs::kDnsOverHttpsIncludedDomains, pref_value);

  providers = GetDOHProviders();

  it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_TRUE(it->second.empty());
  EXPECT_EQ(providers.size(), 1u);
}

TEST_F(SecureDnsManagerTest, DohExcludedDomains_ShillDohConfig) {
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  // Set DoHExcludedDomains, expect no change to shill DoH config.
  base::Value pref_value(base::Value::Type::LIST);
  pref_value.GetList().Append("test.com");
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  auto providers = GetDOHProviders();

  auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_TRUE(it->second.empty());
  EXPECT_EQ(providers.size(), 1u);

  // Unset DoHExcludedDomains, expect no change to shill DoH config.
  pref_value.GetList().clear();
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  providers = GetDOHProviders();

  it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_TRUE(it->second.empty());
  EXPECT_EQ(providers.size(), 1u);
}

TEST_F(SecureDnsManagerTest, ResetShillState) {
  // Set DnsOverHttpsMode and DnsOverHttpsTemplates.
  local_state()->SetManagedPref(::prefs::kDnsOverHttpsMode,
                                base::Value(SecureDnsConfig::kModeSecure));
  local_state()->Set(::prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));

  auto providers = GetDOHProviders();

  auto it = providers.find(kGoogleDns);
  EXPECT_TRUE(it != providers.end());
  EXPECT_EQ(it->first, kGoogleDns);
  EXPECT_TRUE(it->second.empty());
  EXPECT_EQ(providers.size(), 1u);

  // Set DnsOverHttpsIncludedDomains and DnsOverHttpsExcludedDomains.
  std::vector<std::string> domains = {"test.com", "*.test.com"};
  base::Value pref_value(base::Value::Type::LIST);
  for (const auto& domain : domains) {
    pref_value.GetList().Append(domain);
  }
  local_state()->Set(prefs::kDnsOverHttpsIncludedDomains, pref_value);
  local_state()->Set(prefs::kDnsOverHttpsExcludedDomains, pref_value);

  EXPECT_EQ(domains, GetDOHIncludedDomains());
  EXPECT_EQ(domains, GetDOHExcludedDomains());

  // Expect Shill's state to be cleared when the class is destroyed.
  ResetSecureDnsManager();

  EXPECT_TRUE(GetDOHProviders().empty());
  EXPECT_TRUE(GetDOHIncludedDomains().empty());
  EXPECT_TRUE(GetDOHExcludedDomains().empty());
}

}  // namespace
}  // namespace ash
