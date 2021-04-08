// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_AUTOMATION_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_AUTOMATION_ASH_H_

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_tree_id.h"

namespace crosapi {

// Implements the crosapi interface for automation. Lives in Ash-Chrome on
// the UI thread.
class AutomationAsh : public mojom::Automation,
                      public ui::AXActionHandlerObserver {
 public:
  AutomationAsh();
  AutomationAsh(const AutomationAsh&) = delete;
  AutomationAsh& operator=(const AutomationAsh&) = delete;
  ~AutomationAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Automation> receiver);

  // Called by ash's internal a11y implementation. Data is forwarded to Lacros.
  void EnableDesktop();
  void EnableTree(const ui::AXTreeID& tree_id);

  // crosapi::mojom::Automation:
  void RegisterAutomationClient(
      mojo::PendingRemote<mojom::AutomationClient> client,
      const base::UnguessableToken& token) override;
  void ReceiveEventPrototype(const std::string& event_bundle,
                             bool root,
                             const base::UnguessableToken& token,
                             const std::string& window_id) override;
  void DispatchTreeDestroyedEvent(
      const base::UnguessableToken& tree_id) override;

  // ui::AXActionHandlerObserver:
  void PerformAction(const ui::AXTreeID& tree_id,
                     int32_t automation_node_id,
                     const std::string& action_type,
                     int32_t request_id,
                     const base::DictionaryValue& optional_args) override;

 private:
  // Called when an AutomationClient is disconnected.
  void ClientDisconnected(const base::UnguessableToken& token);

  bool desktop_enabled_ = false;

  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::Automation> receivers_;

  // This map maintains a list of all known automation clients.
  std::map<base::UnguessableToken, mojo::Remote<mojom::AutomationClient>>
      automation_clients_;

  base::WeakPtrFactory<AutomationAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_AUTOMATION_ASH_H_
