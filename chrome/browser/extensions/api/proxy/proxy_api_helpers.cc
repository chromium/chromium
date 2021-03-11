// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of helper functions for the Chrome Extensions Proxy Settings
// API.
//
// Throughout this code, we report errors to the user by setting an |error|
// parameter, if and only if these errors can be cause by invalid input
// from the extension and we cannot expect that the extensions API has
// caught this error before. In all other cases we are dealing with internal
// errors and log to LOG(ERROR).

#include "chrome/browser/extensions/api/proxy/proxy_api_helpers.h"

#include <stddef.h>

#include <utility>

#include "base/base64.h"
#include "base/notreached.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/proxy/proxy_api_constants.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "extensions/common/error_utils.h"
#include "net/base/data_url.h"
#include "net/proxy_resolution/proxy_config.h"

namespace extensions {

namespace proxy_api_helpers {

bool CreateDataURLFromPACScript(const std::string& pac_script,
                                std::string* pac_script_url_base64_encoded) {
  // Encode pac_script in base64.
  std::string pac_script_base64_encoded;
  base::Base64Encode(pac_script, &pac_script_base64_encoded);

  // Make it a correct data url.
  *pac_script_url_base64_encoded =
      std::string(proxy_api_constants::kPACDataUrlPrefix) +
      pac_script_base64_encoded;
  return true;
}

bool CreatePACScriptFromDataURL(
    const std::string& pac_script_url_base64_encoded,
    std::string* pac_script) {
  GURL url(pac_script_url_base64_encoded);
  if (!url.is_valid())
    return false;

  std::string mime_type;
  std::string charset;
  return net::DataURL::Parse(url, &mime_type, &charset, pac_script);
}

// Extension Pref -> Browser Pref conversion.

bool GetProxyModeFromExtensionPref(const base::DictionaryValue* proxy_config,
                                   ProxyPrefs::ProxyMode* out,
                                   std::string* error,
                                   bool* bad_message) {
  std::string proxy_mode;

  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in the extension API JSON.
  proxy_config->GetStringASCII(proxy_api_constants::kProxyConfigMode,
                               &proxy_mode);
  if (!ProxyPrefs::StringToProxyMode(proxy_mode, out)) {
    LOG(ERROR) << "Invalid mode for proxy settings: " << proxy_mode;
    *bad_message = true;
    return false;
  }
  return true;
}

bool GetPacMandatoryFromExtensionPref(const base::DictionaryValue* proxy_config,
                                      bool* out,
                                      std::string* error,
                                      bool* bad_message) {
  const base::DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(proxy_api_constants::kProxyConfigPacScript,
                              &pac_dict);
  if (!pac_dict) {
    *out = false;
    return true;
  }

  bool mandatory_pac = false;
  if (pac_dict->HasKey(proxy_api_constants::kProxyConfigPacScriptMandatory) &&
      !pac_dict->GetBoolean(proxy_api_constants::kProxyConfigPacScriptMandatory,
                            &mandatory_pac)) {
    LOG(ERROR) << "'pacScript.mandatory' could not be parsed.";
    *bad_message = true;
    return false;
  }
  *out = mandatory_pac;
  return true;
}

bool GetPacUrlFromExtensionPref(const base::DictionaryValue* proxy_config,
                                std::string* out,
                                std::string* error,
                                bool* bad_message) {
  const base::DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(proxy_api_constants::kProxyConfigPacScript,
                              &pac_dict);
  if (!pac_dict)
    return true;

  // TODO(battre): Handle UTF-8 URLs (http://crbug.com/72692).
  std::u16string pac_url16;
  if (pac_dict->HasKey(proxy_api_constants::kProxyConfigPacScriptUrl) &&
      !pac_dict->GetString(proxy_api_constants::kProxyConfigPacScriptUrl,
                           &pac_url16)) {
    LOG(ERROR) << "'pacScript.url' could not be parsed.";
    *bad_message = true;
    return false;
  }
  if (!base::IsStringASCII(pac_url16)) {
    *error = "'pacScript.url' supports only ASCII URLs "
             "(encode URLs in Punycode format).";
    return false;
  }
  *out = base::UTF16ToASCII(pac_url16);
  return true;
}

bool GetPacDataFromExtensionPref(const base::DictionaryValue* proxy_config,
                                 std::string* out,
                                 std::string* error,
                                 bool* bad_message) {
  const base::DictionaryValue* pac_dict = NULL;
  proxy_config->GetDictionary(proxy_api_constants::kProxyConfigPacScript,
                              &pac_dict);
  if (!pac_dict)
    return true;

  std::u16string pac_data16;
  if (pac_dict->HasKey(proxy_api_constants::kProxyConfigPacScriptData) &&
      !pac_dict->GetString(proxy_api_constants::kProxyConfigPacScriptData,
                           &pac_data16)) {
    LOG(ERROR) << "'pacScript.data' could not be parsed.";
    *bad_message = true;
    return false;
  }
  if (!base::IsStringASCII(pac_data16)) {
    *error = "'pacScript.data' supports only ASCII code"
             "(encode URLs in Punycode format).";
    return false;
  }
  *out = base::UTF16ToASCII(pac_data16);
  return true;
}

bool GetProxyServer(const base::DictionaryValue* proxy_server,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* out,
                    std::string* error,
                    bool* bad_message) {
  std::string scheme_string;  // optional.

  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in the extension API JSON.
  proxy_server->GetStringASCII(proxy_api_constants::kProxyConfigRuleScheme,
                               &scheme_string);

  net::ProxyServer::Scheme scheme =
      net::ProxyServer::GetSchemeFromURI(scheme_string);
  if (scheme == net::ProxyServer::SCHEME_INVALID)
    scheme = default_scheme;

  // TODO(battre): handle UTF-8 in hostnames (http://crbug.com/72692).
  std::u16string host16;
  if (!proxy_server->GetString(proxy_api_constants::kProxyConfigRuleHost,
                               &host16)) {
    LOG(ERROR) << "Could not parse a 'rules.*.host' entry.";
    *bad_message = true;
    return false;
  }
  if (!base::IsStringASCII(host16)) {
    *error = ErrorUtils::FormatErrorMessage(
        "Invalid 'rules.???.host' entry '*'. 'host' field supports only ASCII "
        "URLs (encode URLs in Punycode format).",
        base::UTF16ToUTF8(host16));
    return false;
  }
  std::string host = base::UTF16ToASCII(host16);

  int port;  // optional.
  if (!proxy_server->GetInteger(proxy_api_constants::kProxyConfigRulePort,
                                &port))
    port = net::ProxyServer::GetDefaultPortForScheme(scheme);

  *out = net::ProxyServer(scheme, net::HostPortPair(host, port));

  return true;
}

bool GetProxyRulesStringFromExtensionPref(
    const base::DictionaryValue* proxy_config,
    std::string* out,
    std::string* error,
    bool* bad_message) {
  const base::DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(proxy_api_constants::kProxyConfigRules,
                              &proxy_rules);
  if (!proxy_rules)
    return true;

  // Local data into which the parameters will be parsed. has_proxy describes
  // whether a setting was found for the scheme; proxy_server holds the
  // respective ProxyServer objects containing those descriptions.
  bool has_proxy[proxy_api_constants::SCHEME_MAX + 1];
  net::ProxyServer proxy_server[proxy_api_constants::SCHEME_MAX + 1];

  // Looking for all possible proxy types is inefficient if we have a
  // singleProxy that will supersede per-URL proxies, but it's worth it to keep
  // the code simple and extensible.
  for (size_t i = 0; i <= proxy_api_constants::SCHEME_MAX; ++i) {
    const base::DictionaryValue* proxy_dict = NULL;
    has_proxy[i] = proxy_rules->GetDictionary(
        proxy_api_constants::field_name[i], &proxy_dict);
    if (has_proxy[i]) {
      net::ProxyServer::Scheme default_scheme = net::ProxyServer::SCHEME_HTTP;
      if (!GetProxyServer(proxy_dict, default_scheme,
                          &proxy_server[i], error, bad_message)) {
        // Don't set |error| here, as GetProxyServer takes care of that.
        return false;
      }
    }
  }

  static_assert(proxy_api_constants::SCHEME_ALL == 0,
                "SCHEME_ALL must be the first value");

  // Handle case that only singleProxy is specified.
  if (has_proxy[proxy_api_constants::SCHEME_ALL]) {
    for (size_t i = 1; i <= proxy_api_constants::SCHEME_MAX; ++i) {
      if (has_proxy[i]) {
        *error = ErrorUtils::FormatErrorMessage(
            "Proxy rule for * and * cannot be set at the same time.",
            proxy_api_constants::field_name[proxy_api_constants::SCHEME_ALL],
            proxy_api_constants::field_name[i]);
        return false;
      }
    }
    *out = proxy_server[proxy_api_constants::SCHEME_ALL].ToURI();
    return true;
  }

  // Handle case that anything but singleProxy is specified.

  // Build the proxy preference string.
  std::string proxy_pref;
  for (size_t i = 1; i <= proxy_api_constants::SCHEME_MAX; ++i) {
    if (has_proxy[i]) {
      // http=foopy:4010;ftp=socks5://foopy2:80
      if (!proxy_pref.empty())
        proxy_pref.append(";");
      proxy_pref.append(proxy_api_constants::scheme_name[i]);
      proxy_pref.append("=");
      proxy_pref.append(proxy_server[i].ToURI());
    }
  }

  *out = proxy_pref;
  return true;
}

bool JoinUrlList(const base::ListValue* list,
                 const std::string& joiner,
                 std::string* out,
                 std::string* error,
                 bool* bad_message) {
  std::string result;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    if (!result.empty())
      result.append(joiner);

    // TODO(battre): handle UTF-8 (http://crbug.com/72692).
    std::u16string entry;
    if (!list->GetString(i, &entry)) {
      LOG(ERROR) << "'rules.bypassList' could not be parsed.";
      *bad_message = true;
      return false;
    }
    if (!base::IsStringASCII(entry)) {
      *error = "'rules.bypassList' supports only ASCII URLs "
               "(encode URLs in Punycode format).";
      return false;
    }
    result.append(base::UTF16ToASCII(entry));
  }
  *out = result;
  return true;
}

bool GetBypassListFromExtensionPref(const base::DictionaryValue* proxy_config,
                                    std::string* out,
                                    std::string* error,
                                    bool* bad_message) {
  const base::DictionaryValue* proxy_rules = NULL;
  proxy_config->GetDictionary(proxy_api_constants::kProxyConfigRules,
                              &proxy_rules);
  if (!proxy_rules)
    return true;

  if (!proxy_rules->HasKey(proxy_api_constants::kProxyConfigBypassList)) {
    *out = "";
    return true;
  }
  const base::ListValue* bypass_list = NULL;
  if (!proxy_rules->GetList(proxy_api_constants::kProxyConfigBypassList,
                            &bypass_list)) {
    LOG(ERROR) << "'rules.bypassList' could not be parsed.";
    *bad_message = true;
    return false;
  }

  return JoinUrlList(bypass_list, ",", out, error, bad_message);
}

std::unique_ptr<base::Value> CreateProxyConfigDict(
    ProxyPrefs::ProxyMode mode_enum,
    bool pac_mandatory,
    const std::string& pac_url,
    const std::string& pac_data,
    const std::string& proxy_rules_string,
    const std::string& bypass_list,
    std::string* error) {
  switch (mode_enum) {
    case ProxyPrefs::MODE_DIRECT:
      return std::make_unique<base::Value>(
          ProxyConfigDictionary::CreateDirect());
    case ProxyPrefs::MODE_AUTO_DETECT:
      return std::make_unique<base::Value>(
          ProxyConfigDictionary::CreateAutoDetect());
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string url;
      if (!pac_url.empty()) {
        url = pac_url;
      } else if (!pac_data.empty()) {
        if (!CreateDataURLFromPACScript(pac_data, &url)) {
          *error = "Internal error, at base64 encoding of 'pacScript.data'.";
          return nullptr;
        }
      } else {
        *error =
            "Proxy mode 'pac_script' requires a 'pacScript' field with "
            "either a 'url' field or a 'data' field.";
        return nullptr;
      }
      return std::make_unique<base::Value>(
          ProxyConfigDictionary::CreatePacScript(url, pac_mandatory));
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      if (proxy_rules_string.empty()) {
        *error = "Proxy mode 'fixed_servers' requires a 'rules' field.";
        return nullptr;
      }
      return std::make_unique<base::Value>(
          ProxyConfigDictionary::CreateFixedServers(proxy_rules_string,
                                                    bypass_list));
    }
    case ProxyPrefs::MODE_SYSTEM:
      return std::make_unique<base::Value>(
          ProxyConfigDictionary::CreateSystem());
    case ProxyPrefs::kModeCount:
      NOTREACHED();
  }
  return nullptr;
}

std::unique_ptr<base::DictionaryValue> CreateProxyRulesDict(
    const ProxyConfigDictionary& proxy_config) {
  ProxyPrefs::ProxyMode mode;
  CHECK(proxy_config.GetMode(&mode) && mode == ProxyPrefs::MODE_FIXED_SERVERS);

  auto extension_proxy_rules = std::make_unique<base::DictionaryValue>();

  std::string proxy_servers;
  if (!proxy_config.GetProxyServer(&proxy_servers)) {
    LOG(ERROR) << "Missing proxy servers in configuration.";
    return NULL;
  }

  net::ProxyConfig::ProxyRules rules;
  rules.ParseFromString(proxy_servers);

  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return NULL;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      if (!rules.single_proxies.IsEmpty()) {
        extension_proxy_rules->Set(
            proxy_api_constants::field_name[proxy_api_constants::SCHEME_ALL],
            CreateProxyServerDict(rules.single_proxies.Get()));
      }
      break;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      if (!rules.proxies_for_http.IsEmpty()) {
        extension_proxy_rules->Set(
            proxy_api_constants::field_name[proxy_api_constants::SCHEME_HTTP],
            CreateProxyServerDict(rules.proxies_for_http.Get()));
      }
      if (!rules.proxies_for_https.IsEmpty()) {
        extension_proxy_rules->Set(
            proxy_api_constants::field_name[proxy_api_constants::SCHEME_HTTPS],
            CreateProxyServerDict(rules.proxies_for_https.Get()));
      }
      if (!rules.proxies_for_ftp.IsEmpty()) {
        extension_proxy_rules->Set(
            proxy_api_constants::field_name[proxy_api_constants::SCHEME_FTP],
            CreateProxyServerDict(rules.proxies_for_ftp.Get()));
      }
      if (!rules.fallback_proxies.IsEmpty()) {
        extension_proxy_rules->Set(
            proxy_api_constants::field_name
                [proxy_api_constants::SCHEME_FALLBACK],
            CreateProxyServerDict(rules.fallback_proxies.Get()));
      }
      break;
  }

  // If we add a new scheme sometime, we need to also store a new dictionary
  // representing this scheme in the code above.
  static_assert(proxy_api_constants::SCHEME_MAX == 4,
                "rules need to be updated along with schemes");

  if (proxy_config.HasBypassList()) {
    std::string bypass_list_string;
    if (!proxy_config.GetBypassList(&bypass_list_string)) {
      LOG(ERROR) << "Invalid bypassList in configuration.";
      return NULL;
    }
    std::unique_ptr<base::ListValue> bypass_list =
        TokenizeToStringList(bypass_list_string, ",;");
    extension_proxy_rules->Set(proxy_api_constants::kProxyConfigBypassList,
                               std::move(bypass_list));
  }

  return extension_proxy_rules;
}

std::unique_ptr<base::DictionaryValue> CreateProxyServerDict(
    const net::ProxyServer& proxy) {
  auto out = std::make_unique<base::DictionaryValue>();
  switch (proxy.scheme()) {
    case net::ProxyServer::SCHEME_HTTP:
      out->SetString(proxy_api_constants::kProxyConfigRuleScheme, "http");
      break;
    case net::ProxyServer::SCHEME_HTTPS:
      out->SetString(proxy_api_constants::kProxyConfigRuleScheme, "https");
      break;
    case net::ProxyServer::SCHEME_QUIC:
      out->SetString(proxy_api_constants::kProxyConfigRuleScheme, "quic");
      break;
    case net::ProxyServer::SCHEME_SOCKS4:
      out->SetString(proxy_api_constants::kProxyConfigRuleScheme, "socks4");
      break;
    case net::ProxyServer::SCHEME_SOCKS5:
      out->SetString(proxy_api_constants::kProxyConfigRuleScheme, "socks5");
      break;
    case net::ProxyServer::SCHEME_DIRECT:
    case net::ProxyServer::SCHEME_INVALID:
      NOTREACHED();
      return NULL;
  }
  out->SetString(proxy_api_constants::kProxyConfigRuleHost,
                 proxy.host_port_pair().host());
  out->SetInteger(proxy_api_constants::kProxyConfigRulePort,
                  proxy.host_port_pair().port());
  return out;
}

std::unique_ptr<base::DictionaryValue> CreatePacScriptDict(
    const ProxyConfigDictionary& proxy_config) {
  ProxyPrefs::ProxyMode mode;
  CHECK(proxy_config.GetMode(&mode) && mode == ProxyPrefs::MODE_PAC_SCRIPT);

  auto pac_script_dict = std::make_unique<base::DictionaryValue>();
  std::string pac_url;
  if (!proxy_config.GetPacUrl(&pac_url)) {
    LOG(ERROR) << "Invalid proxy configuration. Missing PAC URL.";
    return NULL;
  }
  bool pac_mandatory = false;
  if (!proxy_config.GetPacMandatory(&pac_mandatory)) {
    LOG(ERROR) << "Invalid proxy configuration. Missing PAC mandatory field.";
    return NULL;
  }

  if (base::StartsWith(pac_url, "data", base::CompareCase::SENSITIVE)) {
    std::string pac_data;
    if (!CreatePACScriptFromDataURL(pac_url, &pac_data)) {
      LOG(ERROR) << "Cannot decode base64-encoded PAC data URL: " << pac_url;
      return NULL;
    }
    pac_script_dict->SetString(proxy_api_constants::kProxyConfigPacScriptData,
                               pac_data);
  } else {
    pac_script_dict->SetString(proxy_api_constants::kProxyConfigPacScriptUrl,
                               pac_url);
  }
  pac_script_dict->SetBoolean(
      proxy_api_constants::kProxyConfigPacScriptMandatory, pac_mandatory);
  return pac_script_dict;
}

std::unique_ptr<base::ListValue> TokenizeToStringList(
    const std::string& in,
    const std::string& delims) {
  auto out = std::make_unique<base::ListValue>();
  base::StringTokenizer entries(in, delims);
  while (entries.GetNext())
    out->AppendString(entries.token());
  return out;
}

}  // namespace proxy_api_helpers
}  // namespace extensions
