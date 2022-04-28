// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/net/network_settings_translation.h"

#include "chromeos/crosapi/mojom/network_settings_service.mojom.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "net/base/proxy_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kPacUrl[] = "http://pac.pac/";

}  // namespace

namespace chromeos {

TEST(NetworkSettingsTranslationTest, NetProxyToCrosapiProxyDirect) {
  crosapi::mojom::ProxyConfigPtr actual =
      NetProxyToCrosapiProxy(net::ProxyConfigWithAnnotation::CreateDirect());
  EXPECT_TRUE(actual->proxy_settings->is_direct());
}

TEST(NetworkSettingsTranslationTest, NetProxyToCrosapiProxyWpad) {
  crosapi::mojom::ProxyConfigPtr actual =
      NetProxyToCrosapiProxy(net::ProxyConfigWithAnnotation(
          net::ProxyConfig::CreateAutoDetect(), TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(actual->proxy_settings->is_wpad());
}

TEST(NetworkSettingsTranslationTest, NetProxyToCrosapiProxyPacNotMandatory) {
  net::ProxyConfig config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kPacUrl));

  crosapi::mojom::ProxyConfigPtr actual = NetProxyToCrosapiProxy(
      net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_TRUE(actual->proxy_settings->is_pac());
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_url, GURL(kPacUrl));
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_mandatory, false);
}

TEST(NetworkSettingsTranslationTest, NetProxyToCrosapiProxyPacMandatory) {
  net::ProxyConfig config =
      net::ProxyConfig::CreateFromCustomPacURL(GURL(kPacUrl));
  config.set_pac_mandatory(true);

  crosapi::mojom::ProxyConfigPtr actual = NetProxyToCrosapiProxy(
      net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_TRUE(actual->proxy_settings->is_pac());
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_url, GURL(kPacUrl));
  EXPECT_EQ(actual->proxy_settings->get_pac()->pac_mandatory, true);
}

TEST(NetworkSettingsTranslationTest, NetProxyToCrosapiProxyManual) {
  net::ProxyConfig config;
  config.proxy_rules().ParseFromString(
      "http=proxy:80;https=secure_proxy:81;socks=socks_proxy:82;");
  config.proxy_rules().bypass_rules.ParseFromString("localhost;google.com;");

  crosapi::mojom::ProxyConfigPtr actual = NetProxyToCrosapiProxy(
      net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_TRUE(actual->proxy_settings->is_manual());
  std::vector<crosapi::mojom::ProxyLocationPtr> proxy_ptr =
      std::move(actual->proxy_settings->get_manual()->http_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 80);

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

TEST(NetworkSettingsTranslationTest,
     NetProxyToCrosapiProxyManualFromSingleProxies) {
  net::ProxyConfig config;
  config.proxy_rules().ParseFromString("proxy:80;");

  crosapi::mojom::ProxyConfigPtr actual = NetProxyToCrosapiProxy(
      net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_TRUE(actual->proxy_settings->is_manual());
  std::vector<crosapi::mojom::ProxyLocationPtr> proxy_ptr =
      std::move(actual->proxy_settings->get_manual()->http_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 80);

  proxy_ptr =
      std::move(actual->proxy_settings->get_manual()->secure_http_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 80);

  proxy_ptr = std::move(actual->proxy_settings->get_manual()->socks_proxies);
  ASSERT_EQ(proxy_ptr.size(), 1u);
  EXPECT_EQ(proxy_ptr[0]->host, "proxy");
  EXPECT_EQ(proxy_ptr[0]->port, 80);
}

TEST(NetworkSettingsTranslationTest, NetProxyToCrosapiProxyManualEmptyList) {
  net::ProxyConfig config;
  config.proxy_rules().ParseFromString("");
  crosapi::mojom::ProxyConfigPtr actual = NetProxyToCrosapiProxy(
      net::ProxyConfigWithAnnotation(config, TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_TRUE(actual->proxy_settings->is_direct());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToNetProxyDirect) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsDirectPtr direct =
      crosapi::mojom::ProxySettingsDirect::New();
  ptr->proxy_settings =
      crosapi::mojom::ProxySettings::NewDirect(std::move(direct));

  EXPECT_EQ(CrosapiProxyToNetProxy(std::move(ptr)).value().ToValue(),
            net::ProxyConfig::CreateDirect().ToValue());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToNetProxyWpad) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsWpadPtr wpad =
      crosapi::mojom::ProxySettingsWpad::New();
  ptr->proxy_settings = crosapi::mojom::ProxySettings::NewWpad(std::move(wpad));

  auto actual = CrosapiProxyToNetProxy(std::move(ptr));
  EXPECT_TRUE(actual.value().auto_detect());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToNetProxyPac) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsPacPtr pac =
      crosapi::mojom::ProxySettingsPac::New();
  pac->pac_url = GURL(kPacUrl);
  pac->pac_mandatory = true;
  crosapi::mojom::ProxySettingsPtr proxy =
      crosapi::mojom::ProxySettings::NewPac(pac.Clone());
  ptr->proxy_settings = proxy.Clone();
  auto actual = CrosapiProxyToNetProxy(ptr.Clone());
  EXPECT_FALSE(actual.value().auto_detect());
  EXPECT_EQ(actual.value().pac_url(), GURL(kPacUrl));
  EXPECT_TRUE(actual.value().pac_mandatory());

  pac->pac_mandatory = false;
  proxy->set_pac(std::move(pac));
  ptr->proxy_settings = std::move(proxy);
  actual = CrosapiProxyToNetProxy(std::move(ptr));
  EXPECT_FALSE(actual.value().pac_mandatory());
}

TEST(NetworkSettingsTranslationTest, CrosapiProxyToNetProxyManual) {
  crosapi::mojom::ProxyConfigPtr ptr = crosapi::mojom::ProxyConfig::New();
  crosapi::mojom::ProxySettingsManualPtr manual =
      crosapi::mojom::ProxySettingsManual::New();
  crosapi::mojom::ProxyLocationPtr location =
      crosapi::mojom::ProxyLocation::New();
  location->host = "proxy";
  location->port = 80;
  manual->http_proxies.push_back(location.Clone());
  location->host = "secure_proxy";
  location->port = 81;
  manual->secure_http_proxies.push_back(location.Clone());
  location->host = "socks_proxy";
  location->port = 82;
  manual->socks_proxies.push_back(std::move(location));
  manual->exclude_domains = {"localhost", "google.com"};
  ptr->proxy_settings =
      crosapi::mojom::ProxySettings::NewManual(std::move(manual));

  auto actual = CrosapiProxyToNetProxy(std::move(ptr));

  EXPECT_EQ(actual.value().proxy_rules().proxies_for_http.ToPacString(),
            "PROXY proxy:80");
  EXPECT_EQ(actual.value().proxy_rules().proxies_for_https.ToPacString(),
            "HTTPS secure_proxy:81");
  EXPECT_EQ(actual.value().proxy_rules().fallback_proxies.ToPacString(),
            "SOCKS socks_proxy:82");
  EXPECT_EQ(actual.value().proxy_rules().bypass_rules.ToString(),
            "localhost;google.com;");
}

}  // namespace chromeos
