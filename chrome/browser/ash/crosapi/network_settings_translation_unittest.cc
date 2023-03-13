// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/network_settings_translation.h"

#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPacUrl[] = "http://pac.pac/";

base::Value::Dict GetPacProxyConfig(const std::string& pac_url,
                                    bool pac_mandatory) {
  return ProxyConfigDictionary::CreatePacScript(pac_url, pac_mandatory);
}

base::Value::Dict GetManualProxyConfig(const std::string& proxy_servers,
                                       const std::string& bypass_list) {
  return ProxyConfigDictionary::CreateFixedServers(proxy_servers, bypass_list);
}

}  // namespace

namespace crosapi {

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyDirect) {
  ProxyConfigDictionary proxy_dict(ProxyConfigDictionary::CreateDirect());
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));
  EXPECT_TRUE(actual->proxy_settings->is_direct());
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyWpadNoUrl) {
  ProxyConfigDictionary proxy_dict(ProxyConfigDictionary::CreateAutoDetect());
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));
  GURL default_wpad_url("http://wpad/wpad.dat");
  ASSERT_TRUE(actual->proxy_settings->is_wpad());
  EXPECT_EQ(actual->proxy_settings->get_wpad()->pac_url, default_wpad_url);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyWpadUrl) {
  GURL wpad_url(kPacUrl);
  ProxyConfigDictionary proxy_dict(ProxyConfigDictionary::CreateAutoDetect());

  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict, wpad_url);
  ASSERT_TRUE(actual->proxy_settings->is_wpad());
  EXPECT_EQ(actual->proxy_settings->get_wpad()->pac_url, wpad_url);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyPacMandatory) {
  ProxyConfigDictionary proxy_dict(
      GetPacProxyConfig(kPacUrl, /*pac_mandatory=*/true));
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));

  ASSERT_TRUE(actual->proxy_settings->is_pac());
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_url, GURL(kPacUrl));
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_mandatory, true);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyPacNotMandatory) {
  ProxyConfigDictionary proxy_dict(
      GetPacProxyConfig(kPacUrl, /*pac_mandatory=*/false));
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));

  ASSERT_TRUE(actual->proxy_settings->is_pac());
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_url, GURL(kPacUrl));
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_mandatory, false);
}

TEST(NetworkSettingsTranslationTest, ProxyConfigToCrosapiProxyManual) {
  std::string proxy_servers =
      "http=proxy:80;http=proxy2:80;https=secure_proxy:81;socks=socks_proxy:"
      "82;";
  std::string bypass_list = "localhost;google.com;";
  ProxyConfigDictionary proxy_dict(
      GetManualProxyConfig(proxy_servers, bypass_list));
  crosapi::mojom::ProxyConfigPtr actual =
      crosapi::ProxyConfigToCrosapiProxy(&proxy_dict,
                                         /*wpad_url=*/GURL(""));
  ASSERT_TRUE(actual->proxy_settings->is_manual());

  std::vector<crosapi::mojom::ProxyLocationPtr> proxy_ptr =
      std::move(actual->proxy_settings->get_manual()->http_proxies);
  ASSERT_EQ(proxy_ptr.size(), 2u);
  EXPECT_EQ(proxy_ptr[0]->host, "proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 80);
  EXPECT_EQ(proxy_ptr[1]->host, "proxy2");
  EXPECT_EQ(proxy_ptr[1]->port, 80);
  proxy_ptr =
      std::move(actual->proxy_settings->get_manual()->secure_http_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "secure_proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 81);

  proxy_ptr = std::move(actual->proxy_settings->get_manual()->socks_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "socks_proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 82);

  const std::vector<std::string> exclude_domains =
      actual->proxy_settings->get_manual()->exclude_domains;
  ASSERT_EQ(exclude_domains.size(), 2u);
  EXPECT_EQ(exclude_domains[0], "localhost");
  EXPECT_EQ(exclude_domains[1], "google.com");
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigDirect) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  ptr->proxy_settings = crosapi::mojom::ProxySettings::NewDirect(
      crosapi::mojom::ProxySettingsDirect::New());

  EXPECT_EQ(CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
            ProxyConfigDictionary::CreateDirect());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigWpad) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsWpadPtr wpad =
      crosapi::mojom::ProxySettingsWpad::New();
  wpad->pac_url = GURL("pac.pac");
  ptr->proxy_settings = crosapi::mojom::ProxySettings::NewWpad(std::move(wpad));

  EXPECT_EQ(CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
            ProxyConfigDictionary::CreateAutoDetect());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigPac) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPacPtr pac =
      crosapi::mojom::ProxySettingsPac::New();
  pac->pac_url = GURL(kPacUrl);
  pac->pac_mandatory = true;
  ptr->proxy_settings = crosapi::mojom::ProxySettings::NewPac(pac.Clone());
  EXPECT_EQ(CrosapiProxyToProxyConfig(ptr.Clone()).GetDictionary(),
            GetPacProxyConfig(kPacUrl, true));

  pac->pac_mandatory = false;
  ptr->proxy_settings = crosapi::mojom::ProxySettings::NewPac(pac.Clone());
  EXPECT_EQ(CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
            GetPacProxyConfig(kPacUrl, false));
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToProxyConfigManual) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsManualPtr manual =
      crosapi::mojom::ProxySettingsManual::New();
  crosapi::mojom::ProxyLocationPtr location =
      crosapi::mojom::ProxyLocation::New();
  location->host = "proxy1";
  location->port = 80;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kHttp;
  manual->http_proxies.push_back(location.Clone());
  location->host = "proxy2";
  location->port = 80;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kHttps;
  manual->http_proxies.push_back(location.Clone());
  location->host = "proxy3";
  location->port = 83;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kUnknown;
  manual->http_proxies.push_back(location.Clone());
  location->host = "proxy4";
  location->port = 84;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kInvalid;
  manual->http_proxies.push_back(location.Clone());
  location->host = "proxy5";
  location->port = 85;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kDirect;
  manual->http_proxies.push_back(location.Clone());
  location->host = "proxy6";
  location->port = 86;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kSocks5;
  manual->http_proxies.push_back(location.Clone());
  location->host = "secure_proxy";
  location->port = 81;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kHttps;
  manual->secure_http_proxies.push_back(location.Clone());
  location->host = "socks_proxy";
  location->port = 82;
  location->scheme = crosapi::mojom::ProxyLocation::Scheme::kSocks4;
  manual->socks_proxies.push_back(std::move(location));
  manual->exclude_domains = {"localhost", "google.com"};

  ptr->proxy_settings =
      crosapi::mojom::ProxySettings::NewManual(std::move(manual));
  EXPECT_EQ(
      CrosapiProxyToProxyConfig(std::move(ptr)).GetDictionary(),
      GetManualProxyConfig("http=http://proxy1:80;http=https://proxy2:80;"
                           "http=http://proxy3:83;http=invalid://proxy4:84;"
                           "http=direct://proxy5:85;http=socks5://proxy6:86;"
                           "https=https://secure_proxy:81;"
                           "socks=socks://socks_proxy:82",
                           /*bypass_list=*/"localhost;google.com"));
}

}  // namespace crosapi
