// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_AUTOMATION_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_AUTOMATION_MANAGER_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "extensions/browser/api/automation_internal/automation_event_router_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

// This class receives and forwards automation events to Ash. It can only be
// used on the main thread.
class AutomationManagerLacros
    : public crosapi::mojom::AutomationClient,
      public extensions::AutomationEventRouterInterface {
 public:
  AutomationManagerLacros();
  AutomationManagerLacros(const AutomationManagerLacros&) = delete;
  AutomationManagerLacros& operator=(const AutomationManagerLacros&) = delete;
  ~AutomationManagerLacros() override;

 private:
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
      const base::Optional<gfx::Rect>& rect) override;

  // AutomationClient:
  void Enable() override;
  void EnableTree(const base::UnguessableToken& token) override;
  void PerformActionPrototype(const base::UnguessableToken& tree_id,
                              int32_t automation_node_id,
                              const std::string& action_type,
                              int32_t request_id,
                              base::Value optional_args) override;

  mojo::Remote<crosapi::mojom::Automation> automation_remote_;
  mojo::Receiver<crosapi::mojom::AutomationClient> automation_client_receiver_{
      this};
  base::WeakPtrFactory<AutomationManagerLacros> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_AUTOMATION_MANAGER_LACROS_H_
