// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOMATION_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOMATION_CLIENT_IMPL_H_

#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {
class AccessibilityServiceRouter;
}  // namespace ax

namespace ash {

// The AutomationClientImpl forwards accessibility events between the service
// and the browser process AutomationEventRouter.
class AutomationClientImpl : public ax::mojom::AutomationClient,
                             public extensions::AutomationEventRouterInterface {
 public:
  AutomationClientImpl();
  AutomationClientImpl(const AutomationClientImpl&) = delete;
  AutomationClientImpl& operator=(const AutomationClientImpl&) = delete;
  ~AutomationClientImpl() override;

  void Bind(ax::AccessibilityServiceRouter* router);

 private:
  // The following are called by the Accessibility service, passing information
  // back to the OS.
  // TODO(crbug.com/1355633): Override from ax::mojom::AutomationClient:
  void Enable();
  void Disable();
  void EnableTree(const base::UnguessableToken& tree_id);
  void PerformAction(const ui::AXActionData& data);

  // Receive accessibility information from AutomationEventRouter in ash and
  // forward it along to the service.
  // extensions::AutomationEventRouterInterface:
  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override;
  void DispatchAccessibilityLocationChange(
      const ExtensionMsg_AccessibilityLocationChangeParams& params) override;
  void DispatchTreeDestroyedEvent(
      ui::AXTreeID tree_id,
      content::BrowserContext* browser_context) override;
  void DispatchActionResult(const ui::AXActionData& data,
                            bool result,
                            content::BrowserContext* browser_context) override;
  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const absl::optional<gfx::Rect>& rect) override;

  // Here is the remote to Automation in the service.
  mojo::Remote<ax::mojom::Automation> automation_;

  // This class is the AutomationClient, receiving AutomationClient calls
  // from the AccessibilityService, therefore it is the Receiver.
  mojo::Receiver<ax::mojom::AutomationClient> automation_client_receiver_{this};

  bool bound_ = false;
};
}  // namespace ash
#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_CLIENT_IMPL_H_
