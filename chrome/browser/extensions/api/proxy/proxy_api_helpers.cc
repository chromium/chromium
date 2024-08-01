// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

#include <iterator>
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
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/proxy_resolution/proxy_config.h"

namespace extensions {

namespace proxy_api_helpers {

// The names of the schemes to be used to build the preference value string
// for manual proxy settings.  These must be kept in sync with the SCHEME_*
// constants.
constexpr const char* scheme_name[] = {"*error*", "http", "https", "ftp",
                                       "socks"};

constexpr const char* field_name[] = {"singleProxy", "proxyForHttp",
                                      "proxyForHttps", "proxyForFtp",
                                      "fallbackProxy"};

static_assert(SCHEME_MAX == SCHEME_FALLBACK, "SCHEME_MAX is incorrect");
static_assert(std::size(field_name) == SCHEME_MAX + 1,
              "field_name array size is incorrect");
static_assert(std::size(scheme_name) == SCHEME_MAX + 1,
              "scheme_name array size is incorrect");
static_assert(SCHEME_ALL == 0, "SCHEME_ALL must be the first value");

std::string CreateDataURLFromPACScript(const std::string& pac_script) {
  // Prefix that identifies PAC-script encoding urls.
  static constexpr char kPACDataUrlPrefix[] =
      "data:application/x-ns-proxy-autoconfig;base64,";

  // Encode pac_script in base64.
  std::string pac_script_base64_encoded = base::Base64Encode(pac_script);

  // Make it a correct data url.
  return kPACDataUrlPrefix + pac_script_base64_encoded;
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

bool GetProxyModeFromExtensionPref(const base::Value::Dict& proxy_config,
                                   ProxyPrefs::ProxyMode* out,
                                   std::string* error,
                                   bool* bad_message) {
  std::string proxy_mode;

  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in the extension API JSON.
  if (const std::string* proxy_mode_ptr =
          proxy_config.FindString(proxy_api_constants::kProxyConfigMode)) {
    proxy_mode = *proxy_mode_ptr;
    DCHECK(base::IsStringASCII(proxy_mode));
  }
  if (!ProxyPrefs::StringToProxyMode(proxy_mode, out)) {
    LOG(ERROR) << "Invalid mode for proxy settings: " << proxy_mode;
    *bad_message = true;
    return false;
  }
  return true;
}

bool GetPacMandatoryFromExtensionPref(const base::Value::Dict& proxy_config,
                                      bool* out,
                                      std::string* error,
                                      bool* bad_message) {
  const base::Value::Dict* pac_dict =
      proxy_config.FindDict(proxy_api_constants::kProxyConfigPacScript);
  if (!pac_dict) {
    *out = false;
    return true;
  }

  const base::Value* mandatory_pac =
      pac_dict->Find(proxy_api_constants::kProxyConfigPacScriptMandatory);
  if (mandatory_pac) {
    if (!mandatory_pac->is_bool()) {
      LOG(ERROR) << "'pacScript.mandatory' could not be parsed.";
      *bad_message = true;
      return false;
    }
  }
  *out = mandatory_pac ? mandatory_pac->GetBool() : false;
  return true;
}

bool GetPacUrlFromExtensionPref(const base::Value::Dict& proxy_config,
                                std::string* out,
                                std::string* error,
                                bool* bad_message) {
  const base::Value::Dict* pac_dict =
      proxy_config.FindDict(proxy_api_constants::kProxyConfigPacScript);
  if (!pac_dict)
    return true;

  // TODO(battre): Handle UTF-8 URLs (http://crbug.com/72692).
  std::string pac_url;
  const base::Value* pac_url_val =
      pac_dict->Find(proxy_api_constants::kProxyConfigPacScriptUrl);
  if (pac_url_val) {
    if (!pac_url_val->is_string()) {
      LOG(ERROR) << "'pacScript.url' could not be parsed.";
      *bad_message = true;
      return false;
    }
    pac_url = pac_url_val->GetString();
  }
  if (!base::IsStringASCII(pac_url)) {
    *error = "'pacScript.url' supports only ASCII URLs "
             "(encode URLs in Punycode format).";
    return false;
  }
  *out = std::move(pac_url);
  return true;
}

bool GetPacDataFromExtensionPref(const base::Value::Dict& proxy_config,
                                 std::string* out,
                                 std::string* error,
                                 bool* bad_message) {
  const base::Value::Dict* pac_dict =
      proxy_config.FindDict(proxy_api_constants::kProxyConfigPacScript);
  if (!pac_dict)
    return true;

  std::string pac_data;
  const base::Value* pac_val =
      pac_dict->Find(proxy_api_constants::kProxyConfigPacScriptData);
  if (pac_val) {
    if (!pac_val->is_string()) {
      LOG(ERROR) << "'pacScript.data' could not be parsed.";
      *bad_message = true;
      return false;
    }
    pac_data = pac_val->GetString();
  }

  if (!base::IsStringASCII(pac_data)) {
    *error = "'pacScript.data' supports only ASCII code"
             "(encode URLs in Punycode format).";
    return false;
  }
  *out = std::move(pac_data);
  return true;
}

bool GetProxyServer(const base::Value::Dict& proxy_server,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* out,
                    std::string* error,
                    bool* bad_message) {
  std::string scheme_string;  // optional.

  // We can safely assume that this is ASCII due to the allowed enumeration
  // values specified in the extension API JSON.
  if (const std::string* scheme_string_ptr = proxy_server.FindString(
          proxy_api_constants::kProxyConfigRuleScheme)) {
    scheme_string = *scheme_string_ptr;
    DCHECK(base::IsStringASCII(scheme_string));
  }

  net::ProxyServer::Scheme scheme = net::GetSchemeFromUriScheme(scheme_string);
  if (scheme == net::ProxyServer::SCHEME_INVALID)
    scheme = default_scheme;

  // TODO(battre): handle UTF-8 in hostnames (http://crbug.com/72692).
  std::u16string host16;
  if (const std::string* ptr =
          proxy_server.FindString(proxy_api_constants::kProxyConfigRuleHost)) {
    host16 = base::UTF8ToUTF16(*ptr);
  } else {
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

  if (host.empty()) {
    *error = "Invalid 'rules.???.host' entry. Hostname cannot be empty.";
    return false;
  }

  // optional.
  int port = proxy_server.FindInt(proxy_api_constants::kProxyConfigRulePort)
                 .value_or(net::ProxyServer::GetDefaultPortForScheme(scheme));

  *out = net::ProxyServer(scheme, net::HostPortPair(host, port));

  return true;
}

bool GetProxyRulesStringFromExtensionPref(const base::Value::Dict& proxy_config,
                                          std::string* out,
                                          std::string* error,
                                          bool* bad_message) {
  const base::Value::Dict* proxy_rules =
      proxy_config.FindDict(proxy_api_constants::kProxyConfigRules);
  if (!proxy_rules)
    return true;

  // Local data into which the parameters will be parsed. has_proxy describes
  // whether a setting was found for the scheme; proxy_server holds the
  // respective ProxyServer objects containing those descriptions.
  bool has_proxy[SCHEME_MAX + 1];
  net::ProxyServer proxy_server[SCHEME_MAX + 1];

  // Looking for all possible proxy types is inefficient if we have a
  // singleProxy that will supersede per-URL proxies, but it's worth it to keep
  // the code simple and extensible.
  for (size_t i = 0; i <= SCHEME_MAX; ++i) {
    const base::Value::Dict* proxy_dict =
        proxy_rules->FindDictByDottedPath(field_name[i]);
    has_proxy[i] = proxy_dict != nullptr;
    if (has_proxy[i]) {
      net::ProxyServer::Scheme default_scheme = net::ProxyServer::SCHEME_HTTP;
      if (!GetProxyServer(*proxy_dict, default_scheme, &proxy_server[i], error,
                          bad_message)) {
        // Don't set |error| here, as GetProxyServer takes care of that.
        return false;
      }
    }
  }

  // Handle case that only singleProxy is specified.
  if (has_proxy[SCHEME_ALL]) {
    for (size_t i = 1; i <= SCHEME_MAX; ++i) {
      if (has_proxy[i]) {
        *error = ErrorUtils::FormatErrorMessage(
            "Proxy rule for * and * cannot be set at the same time.",
            field_name[SCHEME_ALL], field_name[i]);
        return false;
      }
    }
    *out = net::ProxyServerToProxyUri(proxy_server[SCHEME_ALL]);
    return true;
  }

  // Handle case that anything but singleProxy is specified.

  // Build the proxy preference string.
  std::string proxy_pref;
  for (size_t i = 1; i <= SCHEME_MAX; ++i) {
    if (has_proxy[i]) {
      // http=foopy:4010;ftp=socks5://foopy2:80
      if (!proxy_pref.empty())
        proxy_pref.append(";");
      proxy_pref.append(scheme_name[i]);
      proxy_pref.append("=");
      proxy_pref.append(net::ProxyServerToProxyUri(proxy_server[i]));
    }
  }

  *out = proxy_pref;
  return true;
}

bool JoinUrlList(const base::Value::List& list,
                 const std::string& joiner,
                 std::string* out,
                 std::string* error,
                 bool* bad_message) {
  std::string result;
  for (const auto& val : list) {
    if (!result.empty())
      result.append(joiner);

    // TODO(battre): handle UTF-8 (http://crbug.com/72692).
    const std::string* entry = val.GetIfString();
    if (!entry) {
      LOG(ERROR) << "'rules.bypassList' could not be parsed.";
      *bad_message = true;
      return false;
    }
    if (!base::IsStringASCII(*entry)) {
      *error = "'rules.bypassList' supports only ASCII URLs "
               "(encode URLs in Punycode format).";
      return false;
    }
    result.append(*entry);
  }
  *out = result;
  return true;
}

bool GetBypassListFromExtensionPref(const base::Value::Dict& proxy_config,
                                    std::string* out,
                                    std::string* error,
                                    bool* bad_message) {
  const base::Value::Dict* proxy_rules =
      proxy_config.FindDict(proxy_api_constants::kProxyConfigRules);
  if (!proxy_rules)
    return true;

  const base::Value* bypass_list =
      proxy_rules->Find(proxy_api_constants::kProxyConfigBypassList);
  if (!bypass_list) {
    *out = "";
    return true;
  }

  if (!bypass_list->is_list()) {
    LOG(ERROR) << "'rules.bypassList' could not be parsed.";
    *bad_message = true;
    return false;
  }

  return JoinUrlList(bypass_list->GetList(), ",", out, error, bad_message);
}

std::optional<base::Value::Dict> CreateProxyConfigDict(
    ProxyPrefs::ProxyMode mode_enum,
    bool pac_mandatory,
    const std::string& pac_url,
    const std::string& pac_data,
    const std::string& proxy_rules_string,
    const std::string& bypass_list,
    std::string* error) {
  switch (mode_enum) {
    case ProxyPrefs::MODE_DIRECT:
      return ProxyConfigDictionary::CreateDirect();
    case ProxyPrefs::MODE_AUTO_DETECT:
      return ProxyConfigDictionary::CreateAutoDetect();
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string url;
      if (!pac_url.empty()) {
        url = pac_url;
      } else if (!pac_data.empty()) {
        url = CreateDataURLFromPACScript(pac_data);
      } else {
        *error =
            "Proxy mode 'pac_script' requires a 'pacScript' field with "
            "either a 'url' field or a 'data' field.";
        return std::nullopt;
      }
      return ProxyConfigDictionary::CreatePacScript(url, pac_mandatory);
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      if (proxy_rules_string.empty()) {
        *error = "Proxy mode 'fixed_servers' requires a 'rules' field.";
        return std::nullopt;
      }
      return ProxyConfigDictionary::CreateFixedServers(proxy_rules_string,
                                                       bypass_list);
    }
    case ProxyPrefs::MODE_SYSTEM:
      return ProxyConfigDictionary::CreateSystem();
    case ProxyPrefs::kModeCount:
      NOTREACHED_IN_MIGRATION();
  }
  return std::nullopt;
}

std::optional<base::Value::Dict> CreateProxyRulesDict(
    const ProxyConfigDictionary& proxy_config) {
  ProxyPrefs::ProxyMode mode;
  CHECK(proxy_config.GetMode(&mode) && mode == ProxyPrefs::MODE_FIXED_SERVERS);

  std::string proxy_servers;
  if (!proxy_config.GetProxyServer(&proxy_servers)) {
    LOG(ERROR) << "Missing proxy servers in configuration.";
    return std::nullopt;
  }

  base::Value::Dict extension_proxy_rules;

  net::ProxyConfig::ProxyRules rules;
  rules.ParseFromString(proxy_servers);

  switch (rules.type) {
    case net::ProxyConfig::ProxyRules::Type::EMPTY:
      return std::nullopt;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST:
      if (!rules.single_proxies.IsEmpty()) {
        extension_proxy_rules.Set(
            field_name[SCHEME_ALL],
            CreateProxyServerDict(rules.single_proxies.First()));
      }
      break;
    case net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME:
      if (!rules.proxies_for_http.IsEmpty()) {
        extension_proxy_rules.Set(
            field_name[SCHEME_HTTP],
            CreateProxyServerDict(rules.proxies_for_http.First()));
      }
      if (!rules.proxies_for_https.IsEmpty()) {
        extension_proxy_rules.Set(
            field_name[SCHEME_HTTPS],
            CreateProxyServerDict(rules.proxies_for_https.First()));
      }
      if (!rules.proxies_for_ftp.IsEmpty()) {
        extension_proxy_rules.Set(
            field_name[SCHEME_FTP],
            CreateProxyServerDict(rules.proxies_for_ftp.First()));
      }
      if (!rules.fallback_proxies.IsEmpty()) {
        extension_proxy_rules.Set(
            field_name[SCHEME_FALLBACK],
            CreateProxyServerDict(rules.fallback_proxies.First()));
      }
      break;
  }

  // If we add a new scheme sometime, we need to also store a new dictionary
  // representing this scheme in the code above.
  static_assert(SCHEME_MAX == 4, "rules need to be updated along with schemes");

  if (proxy_config.HasBypassList()) {
    std::string bypass_list_string;
    if (!proxy_config.GetBypassList(&bypass_list_string)) {
      LOG(ERROR) << "Invalid bypassList in configuration.";
      return std::nullopt;
    }
    base::Value::List bypass_list =
        TokenizeToStringList(bypass_list_string, ",;");
    extension_proxy_rules.Set(proxy_api_constants::kProxyConfigBypassList,
                              std::move(bypass_list));
  }

  return extension_proxy_rules;
}

base::Value::Dict CreateProxyServerDict(const net::ProxyChain& proxy_chain) {
  base::Value::Dict out;
  const char* scheme = nullptr;
  CHECK(proxy_chain.is_single_proxy());
  const net::ProxyServer& proxy = proxy_chain.First();
  switch (proxy.scheme()) {
    case net::ProxyServer::SCHEME_HTTP:
      scheme = "http";
      break;
    case net::ProxyServer::SCHEME_HTTPS:
      scheme = "https";
      break;
    case net::ProxyServer::SCHEME_QUIC:
      scheme = "quic";
      break;
    case net::ProxyServer::SCHEME_SOCKS4:
      scheme = "socks4";
      break;
    case net::ProxyServer::SCHEME_SOCKS5:
      scheme = "socks5";
      break;
    case net::ProxyServer::SCHEME_INVALID:
      NOTREACHED_IN_MIGRATION();
      return out;
  }
  out.Set(proxy_api_constants::kProxyConfigRuleScheme, scheme);
  out.Set(proxy_api_constants::kProxyConfigRuleHost,
          proxy.host_port_pair().host());
  out.Set(proxy_api_constants::kProxyConfigRulePort,
          proxy.host_port_pair().port());
  return out;
}

std::optional<base::Value::Dict> CreatePacScriptDict(
    const ProxyConfigDictionary& proxy_config) {
  ProxyPrefs::ProxyMode mode;
  CHECK(proxy_config.GetMode(&mode) && mode == ProxyPrefs::MODE_PAC_SCRIPT);

  std::string pac_url;
  if (!proxy_config.GetPacUrl(&pac_url)) {
    LOG(ERROR) << "Invalid proxy configuration. Missing PAC URL.";
    return std::nullopt;
  }
  bool pac_mandatory = false;
  if (!proxy_config.GetPacMandatory(&pac_mandatory)) {
    LOG(ERROR) << "Invalid proxy configuration. Missing PAC mandatory field.";
    return std::nullopt;
  }

  base::Value::Dict pac_script_dict;
  if (base::StartsWith(pac_url, "data", base::CompareCase::SENSITIVE)) {
    std::string pac_data;
    if (!CreatePACScriptFromDataURL(pac_url, &pac_data)) {
      LOG(ERROR) << "Cannot decode base64-encoded PAC data URL: " << pac_url;
      return std::nullopt;
    }
    pac_script_dict.Set(proxy_api_constants::kProxyConfigPacScriptData,
                        pac_data);
  } else {
    pac_script_dict.Set(proxy_api_constants::kProxyConfigPacScriptUrl, pac_url);
  }
  pac_script_dict.Set(proxy_api_constants::kProxyConfigPacScriptMandatory,
                      pac_mandatory);
  return std::make_optional(std::move(pac_script_dict));
}

base::Value::List TokenizeToStringList(const std::string& in,
                                       const std::string& delims) {
  base::Value::List out;
  base::StringTokenizer entries(in, delims);
  while (entries.GetNext())
    out.Append(entries.token_piece());
  return out;
}

}  // namespace proxy_api_helpers
}  // namespace extensions
