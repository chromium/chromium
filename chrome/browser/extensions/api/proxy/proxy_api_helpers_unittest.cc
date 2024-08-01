// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Unit tests for helper functions for the Chrome Extensions Proxy Settings API.

#include "chrome/browser/extensions/api/proxy/proxy_api_helpers.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/api/proxy/proxy_api_constants.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
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
base::Value::Dict CreateTestProxyServerDict(const std::string& host) {
  base::Value::Dict dict;
  dict.Set(keys::kProxyConfigRuleHost, host);
  return dict;
}

// Helper function to create a ProxyServer dictionary as defined in the
// extension API.
base::Value::Dict CreateTestProxyServerDict(const std::string& schema,
                                            const std::string& host,
                                            int port) {
  base::Value::Dict dict;
  dict.Set(keys::kProxyConfigRuleScheme, schema);
  dict.Set(keys::kProxyConfigRuleHost, host);
  dict.Set(keys::kProxyConfigRulePort, port);
  return dict;
}

}  // namespace

namespace proxy_api_helpers {

TEST(ExtensionProxyApiHelpers, CreateDataURLFromPACScript) {
  EXPECT_EQ(kSamplePacScriptAsDataUrl,
            CreateDataURLFromPACScript(kSamplePacScript));
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
  base::Value::Dict proxy_config;
  ProxyPrefs::ProxyMode mode;
  std::string error;
  bool bad_message = false;

  // Test positive case.
  proxy_config.Set(keys::kProxyConfigMode,
                   ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_DIRECT));
  ASSERT_TRUE(
      GetProxyModeFromExtensionPref(proxy_config, &mode, &error, &bad_message));
  EXPECT_EQ(ProxyPrefs::MODE_DIRECT, mode);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  // Test negative case.
  proxy_config.Set(keys::kProxyConfigMode, "foobar");
  EXPECT_FALSE(
      GetProxyModeFromExtensionPref(proxy_config, &mode, &error, &bad_message));
  EXPECT_TRUE(bad_message);

  // Do not test |error|, as an invalid enumeration value is considered an
  // internal error. It should be filtered by the extensions API.
}

TEST(ExtensionProxyApiHelpers, GetPacUrlFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::Value::Dict proxy_config;
  proxy_config.Set(keys::kProxyConfigMode,
                   ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Currently we are still missing a PAC script entry.
  // This is silently ignored.
  ASSERT_TRUE(
      GetPacUrlFromExtensionPref(proxy_config, &out, &error, &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  // Set up a pac script.
  base::Value::Dict pacScriptDict;
  pacScriptDict.Set(keys::kProxyConfigPacScriptUrl, kSamplePacScriptUrl);
  proxy_config.Set(keys::kProxyConfigPacScript, std::move(pacScriptDict));

  ASSERT_TRUE(
      GetPacUrlFromExtensionPref(proxy_config, &out, &error, &bad_message));
  EXPECT_EQ(kSamplePacScriptUrl, out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, GetPacDataFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::Value::Dict proxy_config;
  proxy_config.Set(keys::kProxyConfigMode,
                   ProxyPrefs::ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

  // Currently we are still missing a PAC data entry. This is silently ignored.
  ASSERT_TRUE(
      GetPacDataFromExtensionPref(proxy_config, &out, &error, &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  // Set up a PAC script.
  base::Value::Dict pacScriptDict;
  pacScriptDict.Set(keys::kProxyConfigPacScriptData, kSamplePacScript);
  proxy_config.Set(keys::kProxyConfigPacScript, std::move(pacScriptDict));

  ASSERT_TRUE(
      GetPacDataFromExtensionPref(proxy_config, &out, &error, &bad_message));
  EXPECT_EQ(kSamplePacScript, out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, GetProxyRulesStringFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::Value::Dict proxy_config;
  proxy_config.Set(keys::kProxyConfigMode, ProxyPrefs::ProxyModeToString(
                                               ProxyPrefs::MODE_FIXED_SERVERS));

  // Currently we are still missing a proxy config entry.
  // This is silently ignored.
  ASSERT_TRUE(GetProxyRulesStringFromExtensionPref(proxy_config, &out, &error,
                                                   &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);

  base::Value::Dict proxy_rules;
  proxy_rules.Set(proxy_api_helpers::field_name[1],
                  CreateTestProxyServerDict("proxy1"));
  proxy_rules.Set(proxy_api_helpers::field_name[2],
                  CreateTestProxyServerDict("proxy2"));
  proxy_config.Set(keys::kProxyConfigRules, std::move(proxy_rules));

  ASSERT_TRUE(GetProxyRulesStringFromExtensionPref(proxy_config, &out, &error,
                                                   &bad_message));
  EXPECT_EQ("http=proxy1:80;https=proxy2:80", out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, GetProxyRulesStringFromExtensionPrefInvalid) {
  static constexpr char kSingleProxy[] = "singleProxy";
  std::string out;
  std::string error;
  bool bad_message = false;

  base::Value::Dict proxy_config;
  proxy_config.Set(keys::kProxyConfigMode, ProxyPrefs::ProxyModeToString(
                                               ProxyPrefs::MODE_FIXED_SERVERS));
  base::Value::Dict invalidManualProxy;
  invalidManualProxy.Set(keys::kProxyConfigRuleHost, std::string());
  base::Value::Dict singleProxy;
  singleProxy.Set(kSingleProxy, std::move(invalidManualProxy));
  proxy_config.Set(keys::kProxyConfigRules, std::move(singleProxy));

  ASSERT_FALSE(GetProxyRulesStringFromExtensionPref(proxy_config, &out, &error,
                                                    &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ("Invalid 'rules.???.host' entry. Hostname cannot be empty.", error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, GetBypassListFromExtensionPref) {
  std::string out;
  std::string error;
  bool bad_message = false;

  base::Value::Dict proxy_config;
  proxy_config.Set(keys::kProxyConfigMode, ProxyPrefs::ProxyModeToString(
                                               ProxyPrefs::MODE_FIXED_SERVERS));

  // Currently we are still missing a proxy config entry.
  // This is silently ignored.
  ASSERT_TRUE(
      GetBypassListFromExtensionPref(proxy_config, &out, &error, &bad_message));
  EXPECT_EQ(std::string(), out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);

  base::Value::List bypass_list;
  bypass_list.Append("host1");
  bypass_list.Append("host2");
  base::Value::Dict proxy_rules;
  proxy_rules.Set(keys::kProxyConfigBypassList, std::move(bypass_list));
  proxy_config.Set(keys::kProxyConfigRules, std::move(proxy_rules));

  ASSERT_TRUE(
      GetBypassListFromExtensionPref(proxy_config, &out, &error, &bad_message));
  EXPECT_EQ("host1,host2", out);
  EXPECT_EQ(std::string(), error);
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, CreateProxyConfigDict) {
  std::string error;
  base::Value::Dict exp_direct = ProxyConfigDictionary::CreateDirect();
  std::optional<base::Value::Dict> out_direct(CreateProxyConfigDict(
      ProxyPrefs::MODE_DIRECT, false, std::string(), std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_direct, *out_direct);

  base::Value::Dict exp_auto = ProxyConfigDictionary::CreateAutoDetect();
  std::optional<base::Value::Dict> out_auto(CreateProxyConfigDict(
      ProxyPrefs::MODE_AUTO_DETECT, false, std::string(), std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_auto, *out_auto);

  base::Value::Dict exp_pac_url =
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptUrl, false);
  std::optional<base::Value::Dict> out_pac_url(CreateProxyConfigDict(
      ProxyPrefs::MODE_PAC_SCRIPT, false, kSamplePacScriptUrl, std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_pac_url, *out_pac_url);

  base::Value::Dict exp_pac_data =
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptAsDataUrl, false);
  std::optional<base::Value::Dict> out_pac_data(CreateProxyConfigDict(
      ProxyPrefs::MODE_PAC_SCRIPT, false, std::string(), kSamplePacScript,
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_pac_data, *out_pac_data);

  base::Value::Dict exp_fixed =
      ProxyConfigDictionary::CreateFixedServers("foo:80", "localhost");
  std::optional<base::Value::Dict> out_fixed(CreateProxyConfigDict(
      ProxyPrefs::MODE_FIXED_SERVERS, false, std::string(), std::string(),
      "foo:80", "localhost", &error));
  EXPECT_EQ(exp_fixed, *out_fixed);

  base::Value::Dict exp_system = ProxyConfigDictionary::CreateSystem();
  std::optional<base::Value::Dict> out_system(CreateProxyConfigDict(
      ProxyPrefs::MODE_SYSTEM, false, std::string(), std::string(),
      std::string(), std::string(), &error));
  EXPECT_EQ(exp_system, *out_system);

  // Neither of them should have set an error.
  EXPECT_EQ(std::string(), error);
}

TEST(ExtensionProxyApiHelpers, GetProxyServer) {
  base::Value::Dict proxy_server_dict;
  net::ProxyServer created;
  std::string error;
  bool bad_message = false;

  // Test simplest case, no schema nor port specified --> defaults are used.
  proxy_server_dict.Set(keys::kProxyConfigRuleHost, "proxy_server");
  ASSERT_TRUE(GetProxyServer(proxy_server_dict, net::ProxyServer::SCHEME_HTTP,
                             &created, &error, &bad_message));
  EXPECT_EQ("PROXY proxy_server:80",
            net::ProxyServerToPacResultElement(created));
  EXPECT_FALSE(bad_message);

  // Test complete case.
  proxy_server_dict.Set(keys::kProxyConfigRuleScheme, "socks4");
  proxy_server_dict.Set(keys::kProxyConfigRulePort, 1234);
  ASSERT_TRUE(GetProxyServer(proxy_server_dict, net::ProxyServer::SCHEME_HTTP,
                             &created, &error, &bad_message));
  EXPECT_EQ("SOCKS proxy_server:1234",
            net::ProxyServerToPacResultElement(created));
  EXPECT_FALSE(bad_message);
}

TEST(ExtensionProxyApiHelpers, JoinUrlList) {
  bool bad_message = false;
  base::Value::List list;
  list.Append("s1");
  list.Append("s2");
  list.Append("s3");

  std::string out;
  std::string error;
  ASSERT_TRUE(JoinUrlList(list, ";", &out, &error, &bad_message));
  EXPECT_EQ("s1;s2;s3", out);
  EXPECT_FALSE(bad_message);
}

// This tests CreateProxyServerDict as well.
TEST(ExtensionProxyApiHelpers, CreateProxyRulesDict) {
  ProxyConfigDictionary config(ProxyConfigDictionary::CreateFixedServers(
      "http=proxy1:80;https=proxy2:80;ftp=proxy3:80;socks=proxy4:80",
      "localhost"));
  std::optional<base::Value::Dict> extension_pref =
      CreateProxyRulesDict(config);
  ASSERT_TRUE(extension_pref);

  base::Value::Dict expected;
  expected.Set("proxyForHttp", CreateTestProxyServerDict("http", "proxy1", 80));
  expected.Set("proxyForHttps",
               CreateTestProxyServerDict("http", "proxy2", 80));
  expected.Set("proxyForFtp", CreateTestProxyServerDict("http", "proxy3", 80));
  expected.Set("fallbackProxy",
               CreateTestProxyServerDict("socks4", "proxy4", 80));
  base::Value::List bypass_list;
  bypass_list.Append("localhost");
  expected.Set(keys::kProxyConfigBypassList, std::move(bypass_list));

  EXPECT_EQ(expected, *extension_pref);
}

// Test multiple proxies per scheme -- expect that only the first is returned.
TEST(ExtensionProxyApiHelpers, CreateProxyRulesDictMultipleProxies) {
  ProxyConfigDictionary config(ProxyConfigDictionary::CreateFixedServers(
      "http=proxy1:80,default://;https=proxy2:80,proxy1:80;ftp=proxy3:80,"
      "https://proxy5:443;socks=proxy4:80,proxy1:80",
      "localhost"));
  std::optional<base::Value::Dict> extension_pref =
      CreateProxyRulesDict(config);
  ASSERT_TRUE(extension_pref);

  base::Value::Dict expected;
  expected.Set("proxyForHttp", CreateTestProxyServerDict("http", "proxy1", 80));
  expected.Set("proxyForHttps",
               CreateTestProxyServerDict("http", "proxy2", 80));
  expected.Set("proxyForFtp", CreateTestProxyServerDict("http", "proxy3", 80));
  expected.Set("fallbackProxy",
               CreateTestProxyServerDict("socks4", "proxy4", 80));
  base::Value::List bypass_list;
  bypass_list.Append("localhost");
  expected.Set(keys::kProxyConfigBypassList, std::move(bypass_list));

  EXPECT_EQ(expected, *extension_pref);
}

// Test if a PAC script URL is specified.
TEST(ExtensionProxyApiHelpers, CreatePacScriptDictWithUrl) {
  ProxyConfigDictionary config(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptUrl, false));
  std::optional<base::Value::Dict> extension_pref = CreatePacScriptDict(config);
  ASSERT_TRUE(extension_pref);

  base::Value::Dict expected;
  expected.Set(keys::kProxyConfigPacScriptUrl, kSamplePacScriptUrl);
  expected.Set(keys::kProxyConfigPacScriptMandatory, false);

  EXPECT_EQ(expected, *extension_pref);
}

// Test if a PAC script is encoded in a data URL.
TEST(ExtensionProxyApiHelpers, CreatePacScriptDictWidthData) {
  ProxyConfigDictionary config(
      ProxyConfigDictionary::CreatePacScript(kSamplePacScriptAsDataUrl, false));
  std::optional<base::Value::Dict> extension_pref = CreatePacScriptDict(config);
  ASSERT_TRUE(extension_pref);

  base::Value::Dict expected;
  expected.Set(keys::kProxyConfigPacScriptData, kSamplePacScript);
  expected.Set(keys::kProxyConfigPacScriptMandatory, false);

  EXPECT_EQ(expected, *extension_pref);
}

TEST(ExtensionProxyApiHelpers, TokenizeToStringList) {
  base::Value::List expected;
  expected.Append("s1");
  expected.Append("s2");
  expected.Append("s3");

  base::Value::List out = TokenizeToStringList("s1;s2;s3", ";");
  EXPECT_EQ(expected, out);
}

}  // namespace proxy_api_helpers
}  // namespace extensions
