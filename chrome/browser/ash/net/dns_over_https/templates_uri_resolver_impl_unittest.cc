// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver_impl.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_ui_data.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::dns_over_https {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Key;
using ::testing::UnorderedElementsAre;

constexpr const char kGoogleDns[] = "https://dns.google/dns-query{?dns}";

constexpr char kTemplateIdentifiers[] =
    "https://dns.google.alternativeuri/"
    "${USER_EMAIL}-${USER_EMAIL_DOMAIN}-${USER_EMAIL_NAME}-${DEVICE_"
    "DIRECTORY_"
    "ID}-${DEVICE_ASSET_ID}-${DEVICE_SERIAL_NUMBER}-${DEVICE_ANNOTATED_"
    "LOCATION}/"
    "dns-query{?dns}";
constexpr char kDisplayTemplateIdentifiers[] =
    "https://dns.google.alternativeuri/"
    "${test-user@testdomain.com}-${testdomain.com}-${test-user}-${85729104-"
    "ef7a-5718d62e72ca}-${admin-provided-"
    "test-asset-ID}-${serial-number}-${admin-provided-test-location}/"
    "dns-query{?dns}";
constexpr char kDisplayTemplateIdentifiersUnaffiliated[] =
    "https://dns.google.alternativeuri/"
    "${test-user@testdomain.com}-${testdomain.com}-${test-user}-${VALUE_NOT_"
    "AVAILABLE}-${VALUE_NOT_AVAILABLE}-${VALUE_NOT_AVAILABLE}-${VALUE_NOT_"
    "AVAILABLE}/dns-query{?dns}";

constexpr char kEffectiveTemplateIdentifiersUnaffiliated[] =
    "https://dns.google.alternativeuri/"
    "EAE2DCB2164EB64B695BC555C4EC45D01C8F0DF73CCD3321E45E5B49F22A22DF-"
    "0641BDB5149AF8B202F8EC96D8C256774CDFE9456CB12663DDF5897AFD91BC78-"
    "F3BA0BDE2D6E8DBE626D0B9ECF7862B18256C4D1807621F9F01AF06A3F603137-"
    "9AB270C9961EBBDF728F43396B0A25A1F198EA5F1F31719758C64E78839B928B-"
    "9AB270C9961EBBDF728F43396B0A25A1F198EA5F1F31719758C64E78839B928B-"
    "9AB270C9961EBBDF728F43396B0A25A1F198EA5F1F31719758C64E78839B928B-"
    "9AB270C9961EBBDF728F43396B0A25A1F198EA5F1F31719758C64E78839B928B/"
    "dns-query{?dns}";

constexpr char kEffectiveTemplateIdentifiers[] =
    "https://dns.google.alternativeuri/"
    "EAE2DCB2164EB64B695BC555C4EC45D01C8F0DF73CCD3321E45E5B49F22A22DF-"
    "0641BDB5149AF8B202F8EC96D8C256774CDFE9456CB12663DDF5897AFD91BC78-"
    "F3BA0BDE2D6E8DBE626D0B9ECF7862B18256C4D1807621F9F01AF06A3F603137-"
    "542BD404A979AB36D342019A18FFE0691B7763C1D145F2D7119FC1DCBBB7B248-"
    "2601D4E337D68B6EE97B338482E45F7D3670E78075490A7F9916321AF7B4539F-"
    "D60336AF57006D9FD327FD96F4B44F9067A09FB0A4DD83FB67EC0B2C472B9734-"
    "F7E37C960A2F15DD63CE0F694F29A3A76E4BE2700918E01FA22F0692098C6B28/"
    "dns-query{?dns}";

constexpr char kEffectiveTemplateIdentifiersNoSalt[] =
    "https://dns.google.alternativeuri/"
    "B07D2C5D119EB1881671C3B8D84CBE4FE3595C0C9ECBBF7670B18DDFDA072F66-"
    "E5E8A65918F11869E27483F8FB2014EF91D3E3C27DE3959FFFF365E59A8D3A4F-"
    "F85AC825D102B9F2D546AA1679EA991AE845994C1343730D564F3FCD0A2168C3-"
    "519F1980774A18DFCFC2003B4DC27E3497BF9B586E5901D7F2F6EDD1845613A9-"
    "505CBC62B85263246EE6FC89264D4039E5B55FD353885EC86C2DAF5CAA05399E-"
    "87278CD685B7191BB97AA713083522D99DBA30FD6F1DEC3C898E8745FB97E3E3-"
    "9CBE0CF3CA986C6BD8241B5A7030FBB807B7340AEFE0C53541B54545A888B551/"
    "dns-query{?dns}";

constexpr char kTestDeviceDirectoryId[] = "85729104-ef7a-5718d62e72ca";
constexpr char kTestDeviceAssetId[] = "admin-provided-test-asset-ID";
constexpr char kTestDeviceAnnotatedLocation[] = "admin-provided-test-location";
constexpr char kTestSerialNumber[] = "serial-number";

class DevicePropertyObserver : public ash::ShillPropertyChangedObserver {
 public:
  explicit DevicePropertyObserver(const std::string& path) : path_(path) {
    ash::ShillDeviceClient::Get()->AddPropertyChangedObserver(
        dbus::ObjectPath(path_), this);
  }
  ~DevicePropertyObserver() override {
    ash::ShillDeviceClient::Get()->RemovePropertyChangedObserver(
        dbus::ObjectPath(path_), this);
  }
  void Wait() { return future_.Take(); }

 private:
  const std::string path_;
  void OnPropertyChanged(const std::string& name,
                         const base::Value& value) override {
    future_.AddValue();
  }
  base::test::RepeatingTestFuture<void> future_;
};

// Sets an IP address for the device identified by `device_path` and waits for
// the observers to be notified by the change.
bool SetIpAddress(const std::string& device_path,
                  const std::string& ipconfig_path,
                  const std::string& ip_address) {
  DevicePropertyObserver property_observer(device_path);
  base::Value::List ip_configs;
  ip_configs.Append(ipconfig_path);
  ShillDeviceClient::Get()->GetTestInterface()->SetDeviceProperty(
      device_path, shill::kIPConfigsProperty,
      base::Value(std::move(ip_configs)),
      /*notify_changed=*/true);
  property_observer.Wait();

  base::test::TestFuture<bool> wait_for_ip_change;
  ShillIPConfigClient::Get()->SetProperty(
      dbus::ObjectPath(ipconfig_path), shill::kAddressProperty,
      base::Value(ip_address), wait_for_ip_change.GetRepeatingCallback());

  return wait_for_ip_change.Wait();
}

}  // namespace

class TemplatesUriResolverImplTest : public testing::Test {
 public:
  TemplatesUriResolverImplTest() = default;

  TemplatesUriResolverImplTest(const TemplatesUriResolverImplTest&) = delete;
  TemplatesUriResolverImplTest& operator=(const TemplatesUriResolverImplTest&) =
      delete;

  void SetUp() override {
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsMode,
                                                 SecureDnsConfig::kModeOff);
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsTemplates,
                                                 "");
    pref_service_.registry()->RegisterStringPref(
        prefs::kDnsOverHttpsTemplatesWithIdentifiers, "");
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsSalt, "");

    fake_user_manager_ = new user_manager::FakeUserManager(&pref_service_);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_.get()));

    doh_template_uri_resolver_ = std::make_unique<TemplatesUriResolverImpl>();

    // Set up fake device attributes.
    std::unique_ptr<policy::FakeDeviceAttributes> device_attributes =
        std::make_unique<policy::FakeDeviceAttributes>();
    device_attributes->SetFakeDirectoryApiId(kTestDeviceDirectoryId);
    device_attributes->SetFakeDeviceAssetId(kTestDeviceAssetId);
    device_attributes->SetFakeDeviceAnnotatedLocation(
        kTestDeviceAnnotatedLocation);
    device_attributes->SetFakeDeviceSerialNumber(kTestSerialNumber);

    network_handler_test_helper_ =
        std::make_unique<ash::NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();

    doh_template_uri_resolver_->SetDeviceAttributesForTesting(
        std::move(device_attributes));
  }

  void TearDown() override { scoped_user_manager_.reset(); }

  void SetupAffiliatedUser() {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        "test-user@testdomain.com", "1234567890"));
    fake_user_manager_->AddUserWithAffiliation(account_id, true);
  }

  void SetupUnaffiliatedUser() {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        "test-user@testdomain.com", "1234567890"));
    fake_user_manager_->AddUser(account_id);
  }

  void ChangeNetworkOncSource(const std::string& path,
                              ::onc::ONCSource onc_source) {
    std::unique_ptr<ash::NetworkUIData> ui_data =
        ash::NetworkUIData::CreateFromONC(onc_source);
    network_handler_test_helper_->SetServiceProperty(
        path, shill::kUIDataProperty, base::Value(ui_data->GetAsJson()));
  }

  PrefService* pref_service() { return &pref_service_; }
  user_manager::FakeUserManager* user_manager() { return fake_user_manager_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TemplatesUriResolverImpl> doh_template_uri_resolver_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;

 private:
  TestingPrefServiceSimple pref_service_;
  raw_ptr<user_manager::FakeUserManager, DanglingUntriaged> fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  ScopedStubInstallAttributes test_install_attributes_{
      StubInstallAttributes::CreateCloudManaged("fake-domain", "fake-id")};
};

// Test that verifies the correct substitution of placeholders in the template
// uri.
TEST_F(TemplatesUriResolverImplTest, TemplatesWithIdentifiers) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiers));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));
  doh_template_uri_resolver_->Update(pref_service());

  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiers);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiers);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());

  // `prefs::kDnsOverHttpsTemplates` should apply when
  // `prefs::kDnsOverHttpsTemplatesWithIdentifiers` is cleared.
  pref_service()->ClearPref(prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(), kGoogleDns);
  EXPECT_FALSE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());
}

// Tests that only user indentifiers are replaced in
// `prefs::kDnsOverHttpsTemplatesWithIdentifiers` if the user is not affiliated.
TEST_F(TemplatesUriResolverImplTest, TemplatesWithIdentifiersUnaffiliated) {
  SetupUnaffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiers));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));
  doh_template_uri_resolver_->Update(pref_service());

  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiersUnaffiliated);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiersUnaffiliated);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());
}

// Verifies that, when the pref sets a list of template URI separated by space,
// all template URIs are being resolved.
TEST_F(TemplatesUriResolverImplTest, MultipleTemplatesWithIdentifiers) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));
  std::string multiple_templates = base::StringPrintf(
      "%s %s %s", kTemplateIdentifiers, kGoogleDns, kTemplateIdentifiers);
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(multiple_templates));
  doh_template_uri_resolver_->Update(pref_service());

  std::string expected_multiple_templates =
      base::StringPrintf("%s %s %s", kEffectiveTemplateIdentifiers, kGoogleDns,
                         kEffectiveTemplateIdentifiers);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            expected_multiple_templates);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());
}

TEST_F(TemplatesUriResolverImplTest, TemplatesWithIdentifiersNoSalt) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value(""));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiers));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));
  doh_template_uri_resolver_->Update(pref_service());

  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiers);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiersNoSalt);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());

  // `prefs::kDnsOverHttpsTemplates` should apply when
  // `prefs::kDnsOverHttpsTemplatesWithIdentifiers` is cleared.
  pref_service()->ClearPref(prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(), kGoogleDns);
}

constexpr char kTemplateIdentifiersWithIp[] =
    "https://dns.google.alternativeuri/${DEVICE_IP_ADDRESSES}";

constexpr char kEffectiveTemplateIdentifiersWithIp[] =
    "https://dns.google.alternativeuri/"
    "001064000001002000000000000000000100000000000001";
constexpr char kDisplayTemplateIdentifiersWithIp[] =
    "https://dns.google.alternativeuri/${100.0.0.1}${::100:0:0:1}";

constexpr char kNoReplacementEffectiveTemplateIdentifiersWithIp[] =
    "https://dns.google.alternativeuri/";
constexpr char kNoReplacementDisplayTemplateIdentifiersWithIp[] =
    "https://dns.google.alternativeuri/";

// Verifies IP addresses placeholder replacement for the
// DnsOverHttpsTemplatesWithIdentifiers policy when the user is not affiliated.
// More specifically, it tests that IP addresses are only included if the
// default network is managed by user policy.
TEST_F(TemplatesUriResolverImplTest,
       TemplatesWithIdentifiersIpAddressUnaffiliatedUser) {
  SetupUnaffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value(""));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiersWithIp));
  const ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();

  const ash::NetworkState* network = network_state_handler->DefaultNetwork();

  // Verify that the IP addresses are not replaced for unmanaged networks.
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(network->onc_source(), ::onc::ONCSource::ONC_SOURCE_UNKNOWN);
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kNoReplacementDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kNoReplacementEffectiveTemplateIdentifiersWithIp);

  // Verify that the IP addresses are replaced for networks managed by user
  // policy.
  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiersWithIp);

  // Verify that the IP addresses are not replaced for networks managed by
  // device policy.
  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kNoReplacementDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kNoReplacementEffectiveTemplateIdentifiersWithIp);
}

// Verifies IP addresses placeholder replacement for the
// DnsOverHttpsTemplatesWithIdentifiers policy when the user is affiliated.
// More specifically, it tests that IP addresses are included if the
// default network is managed by user policy or device policy.
TEST_F(TemplatesUriResolverImplTest,
       TemplatesWithIdentifiersIpAddressAffiliatedUser) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value(""));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiersWithIp));
  const ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();

  const ash::NetworkState* network = network_state_handler->DefaultNetwork();

  // Verify that the IP is not replaced for unmanaged networks.
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(network->onc_source(), ::onc::ONCSource::ONC_SOURCE_UNKNOWN);
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kNoReplacementDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kNoReplacementEffectiveTemplateIdentifiersWithIp);

  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_USER_POLICY);
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiersWithIp);

  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_DEVICE_POLICY);
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiersWithIp);
}

TEST_F(TemplatesUriResolverImplTest,
       TemplatesWithIdentifiersIpProtocolUpdates) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value(""));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiersWithIp));

  const ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* network = network_state_handler->DefaultNetwork();
  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_USER_POLICY);

  SetIpAddress(network->device_path(), /*ipconfig_path=*/"",
               /*ipconfig_path=*/"");
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kNoReplacementDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kNoReplacementEffectiveTemplateIdentifiersWithIp);

  SetIpAddress(network->device_path(), "ipconfig_v4_path", "100.0.0.1");
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            "https://dns.google.alternativeuri/${100.0.0.1}");
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            "https://dns.google.alternativeuri/001064000001");

  SetIpAddress(network->device_path(), "ipconfig_v6_path", "0:0:0:0:100:0:0:1");
  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            "https://dns.google.alternativeuri/${::100:0:0:1}");
  EXPECT_EQ(
      doh_template_uri_resolver_->GetEffectiveTemplates(),
      "https://dns.google.alternativeuri/002000000000000000000100000000000001");
}

// Verify that IP replacement is not happening when a VPN is connected.
TEST_F(TemplatesUriResolverImplTest, TemplatesWithIdentifiersIpWithVpn) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value(""));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiersWithIp));

  const ash::NetworkState* network =
      ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  ChangeNetworkOncSource(network->path(),
                         ::onc::ONCSource::ONC_SOURCE_USER_POLICY);

  network_handler_test_helper_->SetServiceProperty(
      "/service/eth1", shill::kStateProperty, base::Value(shill::kStateIdle));
  network_handler_test_helper_->SetServiceProperty(
      "/service/wifi1", shill::kStateProperty, base::Value(shill::kStateIdle));
  network_handler_test_helper_->SetServiceProperty(
      "/service/vpn1", shill::kStateProperty, base::Value(shill::kStateOnline));

  doh_template_uri_resolver_->Update(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kNoReplacementDisplayTemplateIdentifiersWithIp);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kNoReplacementEffectiveTemplateIdentifiersWithIp);
}

}  // namespace ash::dns_over_https
