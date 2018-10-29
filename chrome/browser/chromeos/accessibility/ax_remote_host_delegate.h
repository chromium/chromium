// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_REMOTE_HOST_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_REMOTE_HOST_DELEGATE_H_

#include <vector>

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "ui/accessibility/ax_host_delegate.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/mojom/ax_host.mojom.h"

namespace ui {
struct AXEvent;
class AXTreeID;
}  // namespace ui

class AXHostService;

// Forwards accessibility events from a remote process that uses aura and views
// (e.g. the Chrome OS keyboard shortcut_viewer) to accessibility extensions.
// Renderers, PDF, etc. use a different path. Created when the app connects over
// mojo. Implements AXHostDelegate to route actions over mojo to the remote
// process.
class AXRemoteHostDelegate : public ui::AXHostDelegate {
 public:
  // |host_service| owns this object. |remote_host_ptr| is the mojo interface
  // for the remote app.
  AXRemoteHostDelegate(AXHostService* host_service,
                       ax::mojom::AXRemoteHostPtr remote_host_ptr);
  ~AXRemoteHostDelegate() override;

  // Requests AX node trees from remote clients and starts listening for remote
  // AX events. Static because the mojo service_manager creates and owns the
  // service object, but automation may be enabled before a client connects and
  // the service starts.
  void OnAutomationEnabled(bool enabled);

  // Handles an accessibility event from a remote host.
  void HandleAccessibilityEvent(const ui::AXTreeID& tree_id,
                                const std::vector<ui::AXTreeUpdate>& updates,
                                const ui::AXEvent& event);

  // ui::AXHostDelegate:
  void PerformAction(const ui::AXActionData& data) override;

  void FlushForTesting();

 private:
  // Cleans up the extension's AX tree when the remote app disconnects.
  void OnRemoteHostDisconnected();

  // The owning AXHostService.
  AXHostService* host_service_;

  // Connection to the remote host.
  mojo::InterfacePtr<ax::mojom::AXRemoteHost> remote_host_ptr_;

  DISALLOW_COPY_AND_ASSIGN(AXRemoteHostDelegate);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_REMOTE_HOST_DELEGATE_H_
