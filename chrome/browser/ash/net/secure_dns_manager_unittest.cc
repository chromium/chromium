// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/secure_dns_manager.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/network/network_handler.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

#include "base/logging.h"

namespace {

constexpr const char kGoogleDns[] = "https://dns.google/dns-query{?dns}";
constexpr const char kCloudflareDns[] =
    "https://chrome.cloudflare-dns.com/dns-query";

void OnGetProperties(bool* success_out,
                     std::map<std::string, std::string>* props_out,
                     base::OnceClosure callback,
                     base::Optional<base::Value> result) {
  *success_out = result.has_value();
  if (result) {
    base::Value* value = result->FindKeyOfType(
        shill::kDNSProxyDOHProvidersProperty, base::Value::Type::DICTIONARY);
    if (value != nullptr) {
      for (const auto& kv : value->DictItems()) {
        props_out->emplace(kv.first, kv.second.GetString());
      }
    }
  }
  std::move(callback).Run();
}

std::map<std::string, std::string> GetDOHProviders() {
  bool success = false;
  std::map<std::string, std::string> props;
  chromeos::ShillManagerClient* shill_manager =
      chromeos::DBusThreadManager::Get()->GetShillManagerClient();
  base::RunLoop run_loop;
  shill_manager->GetProperties(
      base::BindOnce(&OnGetProperties, base::Unretained(&success),
                     base::Unretained(&props), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(success);
  return props;
}

}  // namespace

namespace net {
namespace {

class SecureDnsManagerTest : public testing::Test {
 public:
  SecureDnsManagerTest() = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    EXPECT_TRUE(chromeos::DBusThreadManager::Get()->IsUsingFakes());
    chromeos::NetworkHandler::Initialize();
    EXPECT_TRUE(chromeos::NetworkHandler::IsInitialized());
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsMode,
                                                 SecureDnsConfig::kModeOff);
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsTemplates,
                                                 "");
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  PrefService* pref_service() { return &pref_service_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(SecureDnsManagerTest);
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
  EXPECT_EQ(providers.size(), 1);
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
  EXPECT_EQ(providers.size(), 2);
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
  EXPECT_EQ(providers.size(), 2);
}

}  // namespace
}  // namespace net
