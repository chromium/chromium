// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_CHROME_PROTOCOL_HANDLER_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_CHROME_PROTOCOL_HANDLER_NAVIGATION_THROTTLE_H_

#include "components/custom_handlers/protocol_handler_navigation_throttle.h"

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace custom_handlers {

class ProtocolHandler;
class ProtocolHandlerRegistry;

// Specialization of the custom_handlers::ProtocolHandlerNavigationThrottle
// class that launches a PromptDialog to confirm the protocol handler previously
// registered by an extension.
class ChromeProtocolHandlerNavigationThrottle
    : public ProtocolHandlerNavigationThrottle {
 public:
  ChromeProtocolHandlerNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      ProtocolHandlerRegistry& protocol_handler_registry);

  ~ChromeProtocolHandlerNavigationThrottle() override;

  ChromeProtocolHandlerNavigationThrottle(
      const ChromeProtocolHandlerNavigationThrottle&) = delete;
  ChromeProtocolHandlerNavigationThrottle& operator=(
      const ChromeProtocolHandlerNavigationThrottle&) = delete;

  // If there is an "unconfirmed" handler for the Navigation Requests's url, a
  // new NavigationThrottle will be created and added to interrupt the
  // navitation process to prompt the user about the registered custom handler.
  static void MaybeCreateAndAdd(
      ProtocolHandlerRegistry* registry,
      content::NavigationThrottleRegistry& protocol_handler_registry);

  // content::NavigationThrottle implementation:
  const char* GetNameForLogging() override;

  // Dialog to prompt the user to allow the use of a custom handler registered
  // by an extension to handle the ongoing navigation request.
  void RunConfirmProtocolHandlerDialog(
      content::WebContents* web_contents,
      const ProtocolHandler& handler,
      const std::optional<url::Origin>& initiating_origin,
      HandlerPermissionGrantedCallback granted_callback,
      HandlerPermissionDeniedCallback denied_callback) const override;
};

}  // namespace custom_handlers

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_CHROME_PROTOCOL_HANDLER_NAVIGATION_THROTTLE_H_
