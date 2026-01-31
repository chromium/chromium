// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/chrome_protocol_handler_navigation_throttle.h"

#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "content/public/browser/navigation_handle.h"

namespace custom_handlers {

ChromeProtocolHandlerNavigationThrottle::
    ChromeProtocolHandlerNavigationThrottle(
        content::NavigationThrottleRegistry& registry,
        ProtocolHandlerRegistry& protocol_handler_registry)
    : ProtocolHandlerNavigationThrottle(registry, protocol_handler_registry) {}

ChromeProtocolHandlerNavigationThrottle::
    ~ChromeProtocolHandlerNavigationThrottle() = default;

// static
void ChromeProtocolHandlerNavigationThrottle::MaybeCreateAndAdd(
    ProtocolHandlerRegistry* protocol_handler_registry,
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  const GURL& url = handle.GetURL();

  // TODO(crbug.com/40482153): We should use scheme_piece instead, which would
  // imply adapting the ProtocolHandlerRegistry code to use std::string_view.
  if (!protocol_handler_registry ||
      !protocol_handler_registry->IsHandledProtocol(url.scheme()) ||
      protocol_handler_registry->IsProtocolHandlerConfirmed(url.scheme())) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<ChromeProtocolHandlerNavigationThrottle>(
          registry, *protocol_handler_registry));
}

const char* ChromeProtocolHandlerNavigationThrottle::GetNameForLogging() {
  return "ChromeProtocolHandlerNavigationThrottle";
}

void ChromeProtocolHandlerNavigationThrottle::RunConfirmProtocolHandlerDialog(
    content::WebContents* web_contents,
    const ProtocolHandler& handler,
    const std::optional<url::Origin>& initiating_origin,
    HandlerPermissionGrantedCallback granted_callback,
    HandlerPermissionDeniedCallback denied_callback) const {
  extensions::ShowConfirmProtocolHandlerDialog(
      web_contents, handler, initiating_origin, std::move(granted_callback),
      std::move(denied_callback));
}

}  // namespace custom_handlers
