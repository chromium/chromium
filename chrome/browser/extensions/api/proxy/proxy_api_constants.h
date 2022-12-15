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

}  // namespace proxy_api_constants
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_API_CONSTANTS_H_
