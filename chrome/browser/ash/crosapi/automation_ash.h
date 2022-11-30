// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_AUTOMATION_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_AUTOMATION_ASH_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_tree_id.h"

namespace crosapi {

// Implements the crosapi interface for automation. Lives in Ash-Chrome on
// the UI thread.
class AutomationAsh : public mojom::Automation,
                      public ui::AXActionHandlerObserver,
                      public mojom::AutomationFactory,
                      public extensions::AutomationEventRouterObserver {
 public:
  AutomationAsh();
  AutomationAsh(const AutomationAsh&) = delete;
  AutomationAsh& operator=(const AutomationAsh&) = delete;
  ~AutomationAsh() override;

  void BindReceiverDeprecated(
      mojo::PendingReceiver<mojom::Automation> receiver);
  void BindReceiver(mojo::PendingReceiver<mojom::AutomationFactory> receiver);

  // Called by ash's internal a11y implementation. Data is forwarded to Lacros.
  void EnableDesktop();
  void EnableTree(const ui::AXTreeID& tree_id);
  void Disable();

  // crosapi::mojom::Automation:
  void RegisterAutomationClientDeprecated(
      mojo::PendingRemote<mojom::AutomationClient> client,
      const base::UnguessableToken& token) override {}
  void ReceiveEventPrototypeDeprecated(const std::string& event_bundle,
                                       bool root,
                                       const base::UnguessableToken& token,
                                       const std::string& window_id) override {}
  void DispatchAccessibilityEvents(
      const base::UnguessableToken& tree_id,
      const std::vector<ui::AXTreeUpdate>& updates,
      const gfx::Point& mouse_location,
      const std::vector<ui::AXEvent>& events) override;
  void DispatchAccessibilityLocationChange(
      const base::UnguessableToken& tree_id,
      int32_t node_id,
      const ui::AXRelativeBounds& bounds) override;
  void DispatchTreeDestroyedEvent(
      const base::UnguessableToken& tree_id) override;
  void DispatchActionResult(const ui::AXActionData& already_handled_action_data,
                            bool result) override;

  // ui::AXActionHandlerObserver:
  void PerformAction(const ui::AXActionData& action_data) override;

  // crosapi::mojom::AutomationFactory:
  void BindAutomation(
      mojo::PendingRemote<crosapi::mojom::AutomationClient> automation_client,
      mojo::PendingReceiver<crosapi::mojom::Automation> automation) override;

  // AutomationEventRouterObserver:
  void AllAutomationExtensionsGone() override;
  void ExtensionListenerAdded() override;

 private:
  bool desktop_enabled_ = false;

  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::AutomationFactory> automation_factory_receivers_;

  // This set maintains a list of all remotes from automation tree sources.
  mojo::ReceiverSet<mojom::Automation> automation_receivers_;

  // This set maintains a list of all known automation clients.
  mojo::RemoteSet<mojom::AutomationClient> automation_client_remotes_;

  base::ScopedObservation<extensions::AutomationEventRouter,
                          extensions::AutomationEventRouterObserver>
      automation_event_router_observer_{this};

  base::WeakPtrFactory<AutomationAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_AUTOMATION_ASH_H_
