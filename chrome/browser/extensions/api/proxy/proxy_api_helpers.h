// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of helper functions for the Chrome Extensions Proxy Settings API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_HELPERS_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/proxy_resolution/proxy_config.h"

class ProxyConfigDictionary;

namespace extensions {
namespace proxy_api_helpers {

// The scheme for which to use a manually specified proxy, not of the proxy URI
// itself.
enum {
  SCHEME_ALL = 0,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FTP,
  SCHEME_FALLBACK,
  SCHEME_MAX = SCHEME_FALLBACK  // Keep this value up to date.
};

// The names of the JavaScript properties to extract from the proxy_rules.
// These must be kept in sync with the SCHEME_* constants.
extern const char* const field_name[];

// Conversion between PAC scripts and data-encoding URLs containing these
// PAC scripts. Data-encoding URLs consist of a data:// prefix, a mime-type and
// base64 encoded text. CreatePACScriptFromDataURL should only be called on
// data-encoding urls created with CreateDataURLFromPACScript.
std::string CreateDataURLFromPACScript(const std::string& pac_script);
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
bool GetProxyModeFromExtensionPref(const base::Value::Dict& proxy_config,
                                   ProxyPrefs::ProxyMode* out,
                                   std::string* error,
                                   bool* bad_message);
bool GetPacMandatoryFromExtensionPref(const base::Value::Dict& proxy_config,
                                      bool* out,
                                      std::string* error,
                                      bool* bad_message);
bool GetPacUrlFromExtensionPref(const base::Value::Dict& proxy_config,
                                std::string* out,
                                std::string* error,
                                bool* bad_message);
bool GetPacDataFromExtensionPref(const base::Value::Dict& proxy_config,
                                 std::string* out,
                                 std::string* error,
                                 bool* bad_message);
bool GetProxyRulesStringFromExtensionPref(const base::Value::Dict& proxy_config,
                                          std::string* out,
                                          std::string* error,
                                          bool* bad_message);
bool GetBypassListFromExtensionPref(const base::Value::Dict& proxy_config,
                                    std::string* out,
                                    std::string* error,
                                    bool* bad_message);

// Creates and returns a ProxyConfig dictionary (as defined in the extension
// API) from the given parameters. Ownership is passed to the caller.
// Depending on the value of |mode_enum|, several of the strings may be empty.
std::optional<base::Value::Dict> CreateProxyConfigDict(
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
bool GetProxyServer(const base::Value::Dict& proxy_server,
                    net::ProxyServer::Scheme default_scheme,
                    net::ProxyServer* out,
                    std::string* error,
                    bool* bad_message);

// Joins a list of URLs (stored as StringValues) in |list| with |joiner|
// to |out|. Returns true if successful and sets |error| otherwise.
bool JoinUrlList(const base::Value::List& list,
                 const std::string& joiner,
                 std::string* out,
                 std::string* error,
                 bool* bad_message);

// Helper functions for browser->extension pref transformation:

// Creates and returns a ProxyRules dictionary as defined in the extension API
// with the values of a ProxyConfigDictionary configured for fixed proxy
// servers. Returns an empty object in case of failures.
std::optional<base::Value::Dict> CreateProxyRulesDict(
    const ProxyConfigDictionary& proxy_config);

// Creates and returns a ProxyServer dictionary as defined in the extension API
// with values from a net::ProxyChain object. Returns an empty dictionary on
// error.
base::Value::Dict CreateProxyServerDict(const net::ProxyChain& proxy);

// Creates and returns a PacScript dictionary as defined in the extension API
// with the values of a ProxyconfigDictionary configured for pac scripts.
// Returns an empty object in case of failures.
std::optional<base::Value::Dict> CreatePacScriptDict(
    const ProxyConfigDictionary& proxy_config);

// Tokenizes the |in| at delimiters |delims| and returns a new
// base::Value::List with string values created from the tokens.
base::Value::List TokenizeToStringList(const std::string& in,
                                       const std::string& delims);

}  // namespace proxy_api_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_HELPERS_H_
