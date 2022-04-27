// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of helper functions for the Chrome Extensions Proxy Settings API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_HELPERS_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class ProxyConfigDictionary;

namespace extensions {
namespace proxy_api_helpers {

// Conversion between PAC scripts and data-encoding URLs containing these
// PAC scripts. Data-encoding URLs consist of a data:// prefix, a mime-type and
// base64 encoded text. The functions return true in case of success.
// CreatePACScriptFromDataURL should only be called on data-encoding urls
// created with CreateDataURLFromPACScript.
bool CreateDataURLFromPACScript(const std::string& pac_script,
                                std::string* pac_script_url_base64_encoded);
bool CreatePACScriptFromDataURL(
    const std::string& pac_script_url_base64_encoded,
    std::string* pac_script);

// Helper functions for extension->browser pref transformation:

// The following functions extract one piece of data from the |proxy_config|
// each. |proxy_config| is a ProxyConfig dictionary as defined in the
// extension API. All output values conform to the format expected by a
// ProxyConfigDictionary.
//
// - If there are NO entries for the respective pieces of data, the functions
//   return true.
//   The GetPacMandatoryFromExtensionPref() function sets |out| to false in this
//   case.
// - If there ARE entries and they could be parsed, the functions set |out|
//   and return true.
// - If there are entries that could not be parsed, the functions set |error|
//   and return false.
//
// The parameter |bad_message| is passed to simulate the behavior of
// EXTENSION_FUNCTION_VALIDATE. It is never NULL.
bool GetProxyModeFromExtensionPref(const base::DictionaryValue* proxy_config,
                                   ProxyPrefs::ProxyMode* out,
                                   std::string* error,
                                   bool* bad_message);
bool GetPacMandatoryFromExtensionPref(const base::DictionaryValue* proxy_config,
                                      bool* out,
                                      std::string* error,
                                      bool* bad_message);
bool GetPacUrlFromExtensionPref(const base::DictionaryValue* proxy_config,
                                std::string* out,
                                std::string* error,
                                bool* bad_message);
bool GetPacDataFromExtensionPref(const base::DictionaryValue* proxy_config,
                                 std::string* out,
                                 std::string* error,
                                 bool* bad_message);
bool GetProxyRulesStringFromExtensionPref(
    const base::DictionaryValue* proxy_config,
    std::string* out,
    std::string* error,
    bool* bad_message);
bool GetBypassListFromExtensionPref(const base::DictionaryValue* proxy_config,
                                    std::string* out,
                                    std::string* error,
                                    bool* bad_message);

// Creates and returns a ProxyConfig dictionary (as defined in the extension
// API) from the given parameters. Ownership is passed to the caller.
// Depending on the value of |mode_enum|, several of the strings may be empty.
std::unique_ptr<base::Value> CreateProxyConfigDict(
    ProxyPrefs::ProxyMode mode_enum,
    bool pac_mandatory,
    const std::string& pac_url,
    const std::string& pac_data,
    const std::string& proxy_rules_string,
    const std::string& bypass_list,
    std::string* error);

// Converts a ProxyServer dictionary instance (as defined in the extension API)
// |proxy_server| to a net::ProxyServer.
// |default_scheme| is the default scheme that is filled in, in case the
// caller did not pass one.
// Returns true if successful and sets |error| otherwise.
bool GetProxyServer(const base::DictionaryValue* proxy_server,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* out,
                    std::string* error,
                    bool* bad_message);

// Joins a list of URLs (stored as StringValues) in |list| with |joiner|
// to |out|. Returns true if successful and sets |error| otherwise.
bool JoinUrlList(base::Value::ConstListView list,
                 const std::string& joiner,
                 std::string* out,
                 std::string* error,
                 bool* bad_message);

// Helper functions for browser->extension pref transformation:

// Creates and returns a ProxyRules dictionary as defined in the extension API
// with the values of a ProxyConfigDictionary configured for fixed proxy
// servers. Returns an empty object in case of failures.
absl::optional<base::Value::Dict> CreateProxyRulesDict(
    const ProxyConfigDictionary& proxy_config);

// Creates and returns a ProxyServer dictionary as defined in the extension API
// with values from a net::ProxyServer object. Returns an empty dictionary on
// error.
base::Value::Dict CreateProxyServerDict(const net::ProxyServer& proxy);

// Creates and returns a PacScript dictionary as defined in the extension API
// with the values of a ProxyconfigDictionary configured for pac scripts.
// Returns an empty object in case of failures.
absl::optional<base::Value::Dict> CreatePacScriptDict(
    const ProxyConfigDictionary& proxy_config);

// Tokenizes the |in| at delimiters |delims| and returns a new
// base::Value::List with string values created from the tokens.
base::Value::List TokenizeToStringList(const std::string& in,
                                       const std::string& delims);

}  // namespace proxy_api_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_HELPERS_H_
