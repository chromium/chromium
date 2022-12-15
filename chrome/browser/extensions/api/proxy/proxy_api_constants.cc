// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the Chrome Extensions Proxy Settings API.

#include "chrome/browser/extensions/api/proxy/proxy_api_constants.h"

namespace extensions {
namespace proxy_api_constants {

const char kProxyConfigMode[] = "mode";
const char kProxyConfigPacScript[] = "pacScript";
const char kProxyConfigPacScriptMandatory[] = "mandatory";
const char kProxyConfigPacScriptUrl[] = "url";
const char kProxyConfigPacScriptData[] = "data";
const char kProxyConfigRules[] = "rules";
const char kProxyConfigRuleHost[] = "host";
const char kProxyConfigRulePort[] = "port";
const char kProxyConfigRuleScheme[] = "scheme";
const char kProxyConfigBypassList[] = "bypassList";

}  // namespace proxy_api_constants
}  // namespace extensions
