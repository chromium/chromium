// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the Chrome Extensions Proxy Settings API.

#include "chrome/browser/extensions/api/proxy/proxy_api.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/proxy/proxy_api_constants.h"
#include "chrome/browser/extensions/api/proxy/proxy_api_helpers.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "net/base/net_errors.h"

namespace extensions {

namespace {

const char kProxyEventFatalKey[] = "fatal";
const char kProxyEventErrorKey[] = "error";
const char kProxyEventDetailsKey[] = "details";
const char kProxyEventOnProxyError[] = "proxy.onProxyError";

// Helper method to dispatch an event to a particular profile indicated by
// `profile_ptr`, but only if that profile is valid.
void DispatchEventToProfile(void* profile_ptr,
                            events::HistogramValue histogram_value,
                            const std::string& event_name,
                            base::Value::List event_args) {
  if (!g_browser_process || !g_browser_process->profile_manager()) {
    return;
  }

  if (!g_browser_process->profile_manager()->IsValidProfile(profile_ptr)) {
    return;
  }
  Profile* profile = reinterpret_cast<Profile*>(profile_ptr);

  auto* event_router = EventRouter::Get(profile);
  // The extension system may not be available in the given profile.
  if (!event_router) {
    return;
  }

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), profile);
  event_router->BroadcastEvent(std::move(event));
}

}  // anonymous namespace

// static
ProxyEventRouter* ProxyEventRouter::GetInstance() {
  return base::Singleton<ProxyEventRouter>::get();
}

ProxyEventRouter::ProxyEventRouter() {
}

ProxyEventRouter::~ProxyEventRouter() {
}

void ProxyEventRouter::OnProxyError(
    void* profile,
    int error_code) {
  base::Value::List args;
  base::Value::Dict dict;
  dict.Set(kProxyEventFatalKey, true);
  dict.Set(kProxyEventErrorKey, net::ErrorToString(error_code));
  dict.Set(kProxyEventDetailsKey, std::string());
  args.Append(base::Value(std::move(dict)));

  if (profile) {
    DispatchEventToProfile(profile, events::PROXY_ON_PROXY_ERROR,
                           kProxyEventOnProxyError, std::move(args));
  } else {
    ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
        events::PROXY_ON_PROXY_ERROR, kProxyEventOnProxyError, std::move(args),
        /*dispatch_to_off_the_record_profiles=*/false);
  }
}

void ProxyEventRouter::OnPACScriptError(void* profile,
                                        int line_number,
                                        const std::u16string& error) {
  base::Value::List args;
  base::Value::Dict dict;
  dict.Set(kProxyEventFatalKey, false);
  dict.Set(kProxyEventErrorKey, net::ErrorToString(net::ERR_PAC_SCRIPT_FAILED));
  std::string error_msg = base::UTF16ToUTF8(error);
  if (line_number != -1) {
    error_msg =
        base::StringPrintf("line: %d: %s", line_number, error_msg.c_str());
  }
  dict.Set(kProxyEventDetailsKey, error_msg);
  args.Append(base::Value(std::move(dict)));

  if (profile) {
    DispatchEventToProfile(profile, events::PROXY_ON_PROXY_ERROR,
                           kProxyEventOnProxyError, std::move(args));
  } else {
    ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
        events::PROXY_ON_PROXY_ERROR, kProxyEventOnProxyError, std::move(args),
        /*dispatch_to_off_the_record_profiles=*/false);
  }
}

ProxyPrefTransformer::ProxyPrefTransformer() = default;

ProxyPrefTransformer::~ProxyPrefTransformer() = default;

std::optional<base::Value> ProxyPrefTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  // When ExtensionToBrowserPref is called, the format of |extension_pref|
  // has been verified already by the extension API to match the schema
  // defined in the extension API JSON.
  CHECK(extension_pref.is_dict());
  const base::Value::Dict& config = extension_pref.GetDict();
  // Extract the various pieces of information passed to
  // chrome.proxy.settings.set(). Several of these strings will
  // remain blank no respective values have been passed to set().
  // If a values has been passed to set but could not be parsed, we bail
  // out and return null.
  ProxyPrefs::ProxyMode mode_enum;
  bool pac_mandatory;
  std::string pac_url;
  std::string pac_data;
  std::string proxy_rules_string;
  std::string bypass_list;
  if (!proxy_api_helpers::GetProxyModeFromExtensionPref(config, &mode_enum,
                                                        &error, &bad_message) ||
      !proxy_api_helpers::GetPacMandatoryFromExtensionPref(
          config, &pac_mandatory, &error, &bad_message) ||
      !proxy_api_helpers::GetPacUrlFromExtensionPref(config, &pac_url, &error,
                                                     &bad_message) ||
      !proxy_api_helpers::GetPacDataFromExtensionPref(config, &pac_data, &error,
                                                      &bad_message) ||
      !proxy_api_helpers::GetProxyRulesStringFromExtensionPref(
          config, &proxy_rules_string, &error, &bad_message) ||
      !proxy_api_helpers::GetBypassListFromExtensionPref(
          config, &bypass_list, &error, &bad_message)) {
    return std::nullopt;
  }

  std::optional<base::Value::Dict> result =
      proxy_api_helpers::CreateProxyConfigDict(
          mode_enum, pac_mandatory, pac_url, pac_data, proxy_rules_string,
          bypass_list, &error);

  if (!result)
    return std::nullopt;

  return base::Value(std::move(*result));
}

std::optional<base::Value> ProxyPrefTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  CHECK(browser_pref.is_dict());

  // This is a dictionary wrapper that exposes the proxy configuration stored in
  // the browser preferences.
  ProxyConfigDictionary config(browser_pref.GetDict().Clone());

  ProxyPrefs::ProxyMode mode;
  if (!config.GetMode(&mode)) {
    LOG(ERROR) << "Cannot determine proxy mode.";
    return std::nullopt;
  }

  // Build a new ProxyConfig instance as defined in the extension API.
  base::Value::Dict extension_pref;

  extension_pref.Set(proxy_api_constants::kProxyConfigMode,
                     ProxyPrefs::ProxyModeToString(mode));

  switch (mode) {
    case ProxyPrefs::MODE_DIRECT:
    case ProxyPrefs::MODE_AUTO_DETECT:
    case ProxyPrefs::MODE_SYSTEM:
      // These modes have no further parameters.
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      // A PAC URL either point to a PAC script or contain a base64 encoded
      // PAC script. In either case we build a PacScript dictionary as defined
      // in the extension API.
      std::optional<base::Value::Dict> pac_dict =
          proxy_api_helpers::CreatePacScriptDict(config);
      if (!pac_dict)
        return std::nullopt;
      extension_pref.Set(proxy_api_constants::kProxyConfigPacScript,
                         std::move(*pac_dict));
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      // Build ProxyRules dictionary according to the extension API.
      std::optional<base::Value::Dict> proxy_rules_dict =
          proxy_api_helpers::CreateProxyRulesDict(config);
      if (!proxy_rules_dict)
        return std::nullopt;
      extension_pref.Set(proxy_api_constants::kProxyConfigRules,
                         std::move(*proxy_rules_dict));
      break;
    }
    case ProxyPrefs::kModeCount:
      NOTREACHED_IN_MIGRATION();
  }
  return base::Value(std::move(extension_pref));
}

}  // namespace extensions
