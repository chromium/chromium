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
#include "content/public/browser/browser_context.h"
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
// `browser_context_ptr`, but only if that BrowserContext is valid.
void DispatchEventToContext(void* browser_context_ptr,
                            events::HistogramValue histogram_value,
                            const std::string& event_name,
                            base::Value::List event_args) {
  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context_ptr)) {
    return;
  }

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_ptr);

  auto* event_router = EventRouter::Get(context);
  // The extension system may not be available in the given profile.
  if (!event_router) {
    return;
  }

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(event_args), context);
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

void ProxyEventRouter::OnProxyError(void* browser_context, int error_code) {
  base::Value::List args;
  base::Value::Dict dict;
  dict.Set(kProxyEventFatalKey, true);
  dict.Set(kProxyEventErrorKey, net::ErrorToString(error_code));
  dict.Set(kProxyEventDetailsKey, std::string());
  args.Append(base::Value(std::move(dict)));

  if (browser_context) {
    DispatchEventToContext(browser_context, events::PROXY_ON_PROXY_ERROR,
                           kProxyEventOnProxyError, std::move(args));
  } else {
    ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
        events::PROXY_ON_PROXY_ERROR, kProxyEventOnProxyError, std::move(args),
        /*dispatch_to_off_the_record_profiles=*/false);
  }
}

void ProxyEventRouter::OnPACScriptError(void* browser_context,
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

  if (browser_context) {
    DispatchEventToContext(browser_context, events::PROXY_ON_PROXY_ERROR,
                           kProxyEventOnProxyError, std::move(args));
  } else {
    ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
        events::PROXY_ON_PROXY_ERROR, kProxyEventOnProxyError, std::move(args),
        /*dispatch_to_off_the_record_profiles=*/false);
  }
}

}  // namespace extensions
