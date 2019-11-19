// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for helper functions for the Chrome Extensions Proxy Settings API.

#include "chrome/browser/extensions/api/proxy/proxy_api_helpers.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/api/proxy/proxy_api_constants.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = proxy_api_constants;

namespace {

const char kSamplePacScript[] = "test";
const char kSamplePacScriptAsDataUrl[] =
    "data:application/x-ns-proxy-autoconfig;base64,dGVzdA==";
const char kSamplePacScriptAsDataUrl2[] =
    "data:;base64,dGVzdA==";
const char kSamplePacScriptUrl[] = "http://wpad/wpad.dat";

// Helper function to create a ProxyServer dictionary as defined in the
// extension API.
std::unique_ptr<base::DictionaryValue> CreateTestProxyServerDict(
    const std::string& host) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString(keys::kProxyConfigRuleHost, host);
  return dict;
}

// Helper function to create a ProxyServer dictionary as defined in the
// extension API.
std::unique_ptr<base::DictionaryValue> CreateTestProxyServerDict(
    const std::string& schema,
    const std::string& host,
    int port) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString(keys::kProxyConfigRuleScheme, schema);
  dict->SetString(keys::kProxyConfigRuleHost, host);
  dict->SetInteger(keys::kProxyConfigRulePort, port);
  return dict;
}

}  // namespace

namespace proxy_api_helpers {

TEST(ExtensionProxyApiHelpers, CreateDataURLFromPACScript) {
  std::string out;
  ASSERT_TRUE(CreateDataURLFromPACScript(kSamplePacScript, &out));
  EXPECT_EQ(kSamplePacScriptAsDataUrl, out);
}

TEST(ExtensionProxyApiHelpers, CreatePACScriptFromDataURL) {
  std::string out;
  // Verify deserialization of a PAC data:// URL that we created ourselves.
  ASSERT_TRUE(CreatePACScriptFromDataURL(kSamplePacScriptAsDataUrl, &out));
  EXPECT_EQ(kSamplePacScript, out);

  // Check that we don't require a mime-type.
  out.clear();
  ASSERT_TRUE(CreatePACScriptFromDataURL(kSamplePacScriptAsDataUrl2, &out));
  EXPECT_EQ(kSamplePacScript, out);

  out.clear();
  EXPECT_FALSE(CreatePACScriptFromDataURL("http://www.google.com", &out));
}

TEST(ExtensionProxyApiHelpers, GetProxyModeFromExtensionPref) {
  base::DictionaryValue proxy_config;
  ProxyPrefs::ProxyMode mode;
  std::string error;
  bool bad_message = false;

  // Test positive case.
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_DIRECT));
  ASSERT_TRUE(GetProxyModeFromExtensionPref(&proxy_config, &mode, &error,
                                            &bad_message));
  EXPECT_EQ(ProxyPrefs::MODE_DIRECT, mode);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  // Test negative case.
  proxy_config.SetString(keys::kProxyConfigMode, "foobar");
  EXPECT_FALSE(GetProxyModeFromExtensionPref(&proxy_config, &mode, &error,
                                             &bad_message));
  EXPECT_TRUE(bad_message);

  // Do not test |error|, as an invalid enumeration value is considered an
  // internal error. It should be filtered by the extensions API.
}

TEST(ExtensionProxyApiHelpers, GetPacUrlFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Currently we are still missing a PAC script entry.
  // This is silently ignored.
  ASSERT_TRUE(GetPacUrlFromExtensionPref(&proxy_config, &out, &error,
                                         &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  // Set up a pac script.
  auto pacScriptDict = std::make_unique<base::DictionaryValue>();
  pacScriptDict->SetString(keys::kProxyConfigPacScriptUrl, kSamplePacScriptUrl);
  proxy_config.Set(keys::kProxyConfigPacScript, std::move(pacScriptDict));

  ASSERT_TRUE(GetPacUrlFromExtensionPref(&proxy_config, &out, &error,
                                         &bad_message));
  EXPECT_EQ(kSamplePacScriptUrl, out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, GetPacDataFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Currently we are still missing a PAC data entry. This is silently ignored.
  ASSERT_TRUE(GetPacDataFromExtensionPref(&proxy_config, &out, &error,
                                          &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  // Set up a PAC script.
  auto pacScriptDict = std::make_unique<base::DictionaryValue>();
  pacScriptDict->SetString(keys::kProxyConfigPacScriptData, kSamplePacScript);
  proxy_config.Set(keys::kProxyConfigPacScript, std::move(pacScriptDict));

  ASSERT_TRUE(GetPacDataFromExtensionPref(&proxy_config, &out, &error,
                                          &bad_message));
  EXPECT_EQ(kSamplePacScript, out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, GetProxyRulesStringFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));

  // Currently we are still missing a proxy config entry.
  // This is silently ignored.
  ASSERT_TRUE(
      GetProxyRulesStringFromExtensionPref(&proxy_config, &out, &error,
                                           &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);

  auto proxy_rules = std::make_unique<base::DictionaryValue>();
  proxy_rules->Set(keys::field_name[1], CreateTestProxyServerDict("proxy1"));
  proxy_rules->Set(keys::field_name[2], CreateTestProxyServerDict("proxy2"));
  proxy_config.Set(keys::kProxyConfigRules, std::move(proxy_rules));

  ASSERT_TRUE(
      GetProxyRulesStringFromExtensionPref(&proxy_config, &out, &error,
                                           &bad_message));
  EXPECT_EQ("http=proxy1:80;https=proxy2:80", out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, GetBypassListFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::DictionaryValue proxy_config;
  proxy_config.SetString(
      keys::kProxyConfigMode,
      ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_FIXED_SERVERS));

  // Currently we are still missing a proxy config entry.
  // This is silently ignored.
  ASSERT_TRUE(
      GetBypassListFromExtensionPref(&proxy_config, &out, &error,
                                     &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  auto bypass_list = std::make_unique<base::ListValue>();
  bypass_list->AppendString("host1");
  bypass_list->AppendString("host2");
  auto proxy_rules = std::make_unique<base::DictionaryValue>();
  proxy_rules->Set(keys::kProxyConfigBypassList, std::move(bypass_list));
  proxy_config.Set(keys::kProxyConfigRules, std::move(proxy_rules));

  ASSERT_TRUE(
      GetBypassListFromExtensionPref(&proxy_config, &out, &error,
                                     &bad_message));
  EXPECT_EQ("host1,host2", out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, CreateProxyConfigDict) {
  std::string error;
  base::Value exp_direct = ProxyConfigDictionary::CreateDirect();
  std::unique_ptr<base::Value> out_direct(CreateProxyConfigDict(
      ProxyPrefs::MODE_DIRECT, false, std::string(), std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_direct, *out_direct);

  base::Value exp_auto = ProxyConfigDictionary::CreateAutoDetect();
  std::unique_ptr<base::Value> out_auto(CreateProxyConfigDict(
      ProxyPrefs::MODE_AUTO_DETECT, false, std::string(), std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_auto, *out_auto);

  base::Value exp_pac_url =
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptUrl, false);
  std::unique_ptr<base::Value> out_pac_url(CreateProxyConfigDict(
      ProxyPrefs::MODE_PAC_SCRIPT, false, kSamplePacScriptUrl, std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_pac_url, *out_pac_url);

  base::Value exp_pac_data =
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptAsDataUrl, false);
  std::unique_ptr<base::Value> out_pac_data(CreateProxyConfigDict(
      ProxyPrefs::MODE_PAC_SCRIPT, false, std::string(), kSamplePacScript,
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_pac_data, *out_pac_data);

  base::Value exp_fixed =
      ProxyConfigDictionary::CreateFixedServers("foo:80", "localhost");
  std::unique_ptr<base::Value> out_fixed(CreateProxyConfigDict(
      ProxyPrefs::MODE_FIXED_SERVERS, false, std::string(), std::string(),
      "foo:80", "localhost", &error));
  EXPECT_EQ(exp_fixed, *out_fixed);

  base::Value exp_system = ProxyConfigDictionary::CreateSystem();
  std::unique_ptr<base::Value> out_system(CreateProxyConfigDict(
      ProxyPrefs::MODE_SYSTEM, false, std::string(), std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_system, *out_system);

  // Neither of them should have set an error.
  EXPECT_EQ(std::string(), error);
}

TEST(ExtensionProxyApiHelpers, GetProxyServer) {
  base::DictionaryValue proxy_server_dict;
  net::ProxyServer created;
  std::string error;
  bool bad_message = false;

  // Test simplest case, no schema nor port specified --> defaults are used.
  proxy_server_dict.SetString(keys::kProxyConfigRuleHost, "proxy_server");
  ASSERT_TRUE(
      GetProxyServer(&proxy_server_dict, net::ProxyServer::SCHEME_HTTP,
                     &created, &error, &bad_message));
  EXPECT_EQ("PROXY proxy_server:80", created.ToPacString());
  EXPECT_FALSE(bad_message);

  // Test complete case.
  proxy_server_dict.SetString(keys::kProxyConfigRuleScheme, "socks4");
  proxy_server_dict.SetInteger(keys::kProxyConfigRulePort, 1234);
  ASSERT_TRUE(
        GetProxyServer(&proxy_server_dict, net::ProxyServer::SCHEME_HTTP,
                       &created, &error, &bad_message));
  EXPECT_EQ("SOCKS proxy_server:1234", created.ToPacString());
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, JoinUrlList) {
  bool bad_message = false;
  base::ListValue list;
  list.AppendString("s1");
  list.AppendString("s2");
  list.AppendString("s3");

  std::string out;
  std::string error;
  ASSERT_TRUE(JoinUrlList(&list, ";", &out, &error, &bad_message));
  EXPECT_EQ("s1;s2;s3", out);
  EXPECT_FALSE(bad_message);
}

// This tests CreateProxyServerDict as well.
TEST(ExtensionProxyApiHelpers, CreateProxyRulesDict) {
  ProxyConfigDictionary config(ProxyConfigDictionary::CreateFixedServers(
      "http=proxy1:80;https=proxy2:80;ftp=proxy3:80;socks=proxy4:80",
      "localhost"));
  std::unique_ptr<base::DictionaryValue> extension_pref(
      CreateProxyRulesDict(config));
  ASSERT_TRUE(extension_pref.get());

  std::unique_ptr<base::DictionaryValue> expected(new base::DictionaryValue);
  expected->Set("proxyForHttp",
                CreateTestProxyServerDict("http", "proxy1", 80));
  expected->Set("proxyForHttps",
                CreateTestProxyServerDict("http", "proxy2", 80));
  expected->Set("proxyForFtp",
                CreateTestProxyServerDict("http", "proxy3", 80));
  expected->Set("fallbackProxy",
                CreateTestProxyServerDict("socks4", "proxy4", 80));
  auto bypass_list = std::make_unique<base::ListValue>();
  bypass_list->AppendString("localhost");
  expected->Set(keys::kProxyConfigBypassList, std::move(bypass_list));

  EXPECT_EQ(*expected, *extension_pref);
}

// Test multiple proxies per scheme -- expect that only the first is returned.
TEST(ExtensionProxyApiHelpers, CreateProxyRulesDictMultipleProxies) {
  ProxyConfigDictionary config(ProxyConfigDictionary::CreateFixedServers(
      "http=proxy1:80,default://;https=proxy2:80,proxy1:80;ftp=proxy3:80,"
      "https://proxy5:443;socks=proxy4:80,proxy1:80",
      "localhost"));
  std::unique_ptr<base::DictionaryValue> extension_pref(
      CreateProxyRulesDict(config));
  ASSERT_TRUE(extension_pref.get());

  std::unique_ptr<base::DictionaryValue> expected(new base::DictionaryValue);
  expected->Set("proxyForHttp",
                CreateTestProxyServerDict("http", "proxy1", 80));
  expected->Set("proxyForHttps",
                CreateTestProxyServerDict("http", "proxy2", 80));
  expected->Set("proxyForFtp",
                CreateTestProxyServerDict("http", "proxy3", 80));
  expected->Set("fallbackProxy",
                CreateTestProxyServerDict("socks4", "proxy4", 80));
  auto bypass_list = std::make_unique<base::ListValue>();
  bypass_list->AppendString("localhost");
  expected->Set(keys::kProxyConfigBypassList, std::move(bypass_list));

  EXPECT_EQ(*expected, *extension_pref);
}

// Test if a PAC script URL is specified.
TEST(ExtensionProxyApiHelpers, CreatePacScriptDictWithUrl) {
  ProxyConfigDictionary config(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptUrl, false));
  std::unique_ptr<base::DictionaryValue> extension_pref(
      CreatePacScriptDict(config));
  ASSERT_TRUE(extension_pref.get());

  std::unique_ptr<base::DictionaryValue> expected(new base::DictionaryValue);
  expected->SetString(keys::kProxyConfigPacScriptUrl, kSamplePacScriptUrl);
  expected->SetBoolean(keys::kProxyConfigPacScriptMandatory, false);

  EXPECT_EQ(*expected, *extension_pref);
}

// Test if a PAC script is encoded in a data URL.
TEST(ExtensionProxyApiHelpers, CreatePacScriptDictWidthData) {
  ProxyConfigDictionary config(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptAsDataUrl, false));
  std::unique_ptr<base::DictionaryValue> extension_pref(
      CreatePacScriptDict(config));
  ASSERT_TRUE(extension_pref.get());

  std::unique_ptr<base::DictionaryValue> expected(new base::DictionaryValue);
  expected->SetString(keys::kProxyConfigPacScriptData, kSamplePacScript);
  expected->SetBoolean(keys::kProxyConfigPacScriptMandatory, false);

  EXPECT_EQ(*expected, *extension_pref);
}

TEST(ExtensionProxyApiHelpers, TokenizeToStringList) {
  base::ListValue expected;
  expected.AppendString("s1");
  expected.AppendString("s2");
  expected.AppendString("s3");

  std::unique_ptr<base::ListValue> out(TokenizeToStringList("s1;s2;s3", ";"));
  EXPECT_EQ(expected, *out);
}

}  // namespace proxy_api_helpers
}  // namespace extensions
