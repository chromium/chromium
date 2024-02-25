// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_ROUTER_H_
#define CHROME_BROWSER_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_ROUTER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

// Used to launch the Accessibility Service.
class AccessibilityServiceRouter : public KeyedService {
 public:
  AccessibilityServiceRouter();
  AccessibilityServiceRouter(const AccessibilityServiceRouter&) = delete;
  AccessibilityServiceRouter& operator=(const AccessibilityServiceRouter&) =
      delete;
  ~AccessibilityServiceRouter() override;

  virtual void BindAccessibilityServiceClient(
      mojo::PendingRemote<mojom::AccessibilityServiceClient>
          accessibility_service_client);

  virtual void BindAssistiveTechnologyController(
      mojo::PendingReceiver<mojom::AssistiveTechnologyController>
          at_controller_receiver,
      const std::vector<mojom::AssistiveTechnologyType>& enabled_features);

  virtual void ConnectDevToolsAgent(
      ::mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      mojom::AssistiveTechnologyType type);

 private:
  void LaunchIfNotRunning();

  mojo::Remote<mojom::AccessibilityService> accessibility_service_;
};

}  // namespace ax

#endif  // CHROME_BROWSER_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_ROUTER_H_
