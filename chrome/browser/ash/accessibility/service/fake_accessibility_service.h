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
  void BindAccessibilityServiceClient(
      mojo::PendingRemote<ax::mojom::AccessibilityServiceClient>
          accessibility_service_client) override;
  void BindAssistiveTechnologyController(
      mojo::PendingReceiver<ax::mojom::AssistiveTechnologyController>
          at_controller_receiver,
      const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features)
      override;

  // TODO(crbug.com/1355633): Override from ax::mojom::Automation:
  void DispatchTreeDestroyedEvent(const ui::AXTreeID& tree_id);
  void DispatchActionResult(const ui::AXActionData& data, bool result);
  void DispatchAccessibilityEvents(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override;
  void DispatchAccessibilityLocationChange(
      const ui::AXTreeID& tree_id,
      int node_id,
      const ui::AXRelativeBounds& bounds) override;

  // ax::mojom::AssistiveTechnologyController:
  void EnableAssistiveTechnology(
      const std::vector<ax::mojom::AssistiveTechnologyType>& enabled_features)
      override;

  //
  // Methods for testing.
  //

  // Whether the service client remote is bound.
  bool IsBound() const;

  // Waits for EnableAssistiveTechnology to be called.
  void WaitForATChanged();

  // Gets the currently enabled assistive technology types.
  const std::set<ax::mojom::AssistiveTechnologyType>& GetEnabledATs() const {
    return enabled_ATs_;
  }

  // Allows tests to bind Automation multiple times, mimicking multiple
  // V8 instances in the service.
  void BindAnotherAutomation();

  // Calls ax::mojom::AutomationClient::Enable or ::Disable.
  void AutomationClientEnable(bool enabled);

  // Whats for Automation events to come in.
  void WaitForAutomationEvents();

  // Getters for automation events.
  std::vector<ui::AXTreeID> tree_destroyed_events() const {
    return tree_destroyed_events_;
  }
  std::vector<std::tuple<ui::AXActionData, bool>> action_results() const {
    return action_results_;
  }
  std::vector<ui::AXTreeID> accessibility_events() const {
    return accessibility_events_;
  }
  std::vector<ui::AXTreeID> location_changes() const {
    return location_changes_;
  }

 private:
  base::OnceClosure change_ATs_closure_;
  std::set<ax::mojom::AssistiveTechnologyType> enabled_ATs_;
  base::OnceClosure automation_events_closure_;

  std::vector<ui::AXTreeID> tree_destroyed_events_;
  std::vector<std::tuple<ui::AXActionData, bool>> action_results_;
  std::vector<ui::AXTreeID> accessibility_events_;
  std::vector<ui::AXTreeID> location_changes_;

  mojo::ReceiverSet<ax::mojom::Automation> automation_receivers_;
  mojo::RemoteSet<ax::mojom::AutomationClient> automation_client_remotes_;

  mojo::ReceiverSet<ax::mojom::AssistiveTechnologyController>
      at_controller_receivers_;
  mojo::Remote<ax::mojom::AccessibilityServiceClient>
      accessibility_service_client_remote_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_FAKE_ACCESSIBILITY_SERVICE_H_
