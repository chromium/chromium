// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the Chrome Extensions Proxy Settings API.

#include "chrome/browser/extensions/api/proxy/proxy_api_constants.h"

#include <iterator>

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

const char kProxyEventFatal[] = "fatal";
const char kProxyEventError[] = "error";
const char kProxyEventDetails[] = "details";
const char kProxyEventOnProxyError[] = "proxy.onProxyError";

const char kPACDataUrlPrefix[] =
    "data:application/x-ns-proxy-autoconfig;base64,";

const char* const field_name[] = { "singleProxy",
                                   "proxyForHttp",
                                   "proxyForHttps",
                                   "proxyForFtp",
                                   "fallbackProxy" };

const char* const scheme_name[] = { "*error*",
                                    "http",
                                    "https",
                                    "ftp",
                                    "socks" };

static_assert(SCHEME_MAX == SCHEME_FALLBACK,
              "SCHEME_MAX is incorrect");
static_assert(std::size(field_name) == SCHEME_MAX + 1,
              "field_name array size is incorrect");
static_assert(std::size(scheme_name) == SCHEME_MAX + 1,
              "scheme_name array size is incorrect");
static_assert(SCHEME_ALL == 0, "SCHEME_ALL must be the first value");

}  // namespace proxy_api_constants
}  // namespace extensions
