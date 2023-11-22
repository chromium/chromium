// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOMATION_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOMATION_CLIENT_IMPL_H_

#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/public/mojom/automation.mojom.h"

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

  void Bind(
      mojo::PendingAssociatedRemote<ax::mojom::Automation> automation,
      mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client);

 private:
  friend class AccessibilityServiceClientTest;

  // The following are called by the Accessibility service, passing information
  // back to the OS.
  // TODO(crbug.com/1355633): Override from ax::mojom::AutomationClient:
  using EnableCallback = base::OnceCallback<void(const ui::AXTreeID&)>;
  void Enable(EnableCallback callback);
  void Disable();
  void EnableTree(const ui::AXTreeID& tree_id);
  void PerformAction(const ui::AXActionData& data);

  // Receive accessibility information from AutomationEventRouter in ash and
  // forward it along to the service.
  // extensions::AutomationEventRouterInterface:
  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   std::vector<ui::AXTreeUpdate> updates,
                                   const gfx::Point& mouse_location,
                                   std::vector<ui::AXEvent> events) override;
  void DispatchAccessibilityLocationChange(
      const content::AXLocationChangeNotificationDetails& details) override;
  void DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) override;
  void DispatchActionResult(const ui::AXActionData& data,
                            bool result,
                            content::BrowserContext* browser_context) override;
  void DispatchGetTextLocationDataResult(
      const ui::AXActionData& data,
      const absl::optional<gfx::Rect>& rect) override;

  mojo::AssociatedRemoteSet<ax::mojom::Automation> automation_remotes_;

  // This class is the AutomationClient, receiving AutomationClient calls
  // from the AccessibilityService.
  mojo::ReceiverSet<ax::mojom::AutomationClient> automation_client_receivers_;

  bool bound_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_AUTOMATION_CLIENT_IMPL_H_
