// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the Chrome Extensions Proxy Settings API.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_CONSTANTS_H_

namespace extensions {
namespace proxy_api_constants {

// String literals in dictionaries used to communicate with extension.
extern const char kProxyConfigMode[];
extern const char kProxyConfigPacScript[];
extern const char kProxyConfigPacScriptMandatory[];
extern const char kProxyConfigPacScriptUrl[];
extern const char kProxyConfigPacScriptData[];
extern const char kProxyConfigRules[];
extern const char kProxyConfigRuleHost[];
extern const char kProxyConfigRulePort[];
extern const char kProxyConfigRuleScheme[];
extern const char kProxyConfigBypassList[];

extern const char kProxyEventFatal[];
extern const char kProxyEventError[];
extern const char kProxyEventDetails[];
extern const char kProxyEventOnProxyError[];

// Prefix that identifies PAC-script encoding urls.
extern const char kPACDataUrlPrefix[];

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

// The names of the schemes to be used to build the preference value string
// for manual proxy settings.  These must be kept in sync with the SCHEME_*
// constants.
extern const char* const scheme_name[];

}  // namespace proxy_api_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_CONSTANTS_H_
