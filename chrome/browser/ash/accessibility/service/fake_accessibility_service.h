// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_FAKE_ACCESSIBILITY_SERVICE_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_FAKE_ACCESSIBILITY_SERVICE_H_

#include "base/unguessable_token.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ash {

// A fake Chrome OS Accessibility service to use for Chrome testing.
// This class acts as an AccessibilityServiceRouter in the browser process
// and then implements service mojom to act as a mock service.
class FakeAccessibilityService
    : public ax::AccessibilityServiceRouter,
      public ax::mojom::Automation,
      public ax::mojom::AssistiveTechnologyController {
 public:
  FakeAccessibilityService();
  FakeAccessibilityService(const FakeAccessibilityService&) = delete;
  FakeAccessibilityService& operator=(const FakeAccessibilityService&) = delete;
  ~FakeAccessibilityService() override;

  // AccessibilityServiceRouter:
  void BindAutomationWithClient(
      mojo::PendingRemote<ax::mojom::AutomationClient> automation_client_remote,
      mojo::PendingReceiver<ax::mojom::Automation> automation_receiver)
      override;
  void BindAssistiveTechnologyController(
      mojo::PendingReceiver<ax::mojom::AssistiveTechnologyController>
          at_controller_receiver,
      const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features)
      override;

  // TODO(crbug.com/1355633): Override from ax::mojom::Automation:
  void DispatchTreeDestroyedEvent(const base::UnguessableToken& tree_id);
  void DispatchActionResult(const ui::AXActionData& data, bool result);
  void DispatchAccessibilityEvents(const base::UnguessableToken& tree_id,
                                   const std::vector<ui::AXTreeUpdate>& updates,
                                   const gfx::Point& mouse_location,
                                   const std::vector<ui::AXEvent>& events);
  void DispatchAccessibilityLocationChange(
      const base::UnguessableToken& tree_id,
      int node_id,
      const ui::AXRelativeBounds& bounds);

  // TODO(crbug.com/1355633): Override from
  // ax::mojom::AssistiveTechnologyController:
  void EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType type,
                                 bool enabled);

  //
  // Methods for testing.
  //

  bool IsBound();

  void WaitForATChanged();

  const std::set<ax::mojom::AssistiveTechnologyType>& GetEnabledATs() {
    return enabled_ATs_;
  }

  void EnableAutomationClient(bool enabled);

  void WaitForAutomationEvents();

 private:
  base::OnceClosure change_ATs_closure_;
  std::set<ax::mojom::AssistiveTechnologyType> enabled_ATs_;
  base::OnceClosure automation_events_closure_;
  std::vector<base::UnguessableToken> tree_destroyed_events_;
  std::vector<std::tuple<ui::AXActionData, bool>> action_results_;
  mojo::ReceiverSet<ax::mojom::Automation> automation_receivers_;
  mojo::RemoteSet<ax::mojom::AutomationClient> automation_client_remotes_;
  mojo::ReceiverSet<ax::mojom::AssistiveTechnologyController>
      at_controller_receivers_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_FAKE_ACCESSIBILITY_SERVICE_H_
