// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_HOST_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_HOST_SERVICE_H_

#include <map>
#include <memory>

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/accessibility/mojom/ax_host.mojom.h"

class AXRemoteHostDelegate;

// Manages a set of remote processes that use aura and views. Renderers such as
// web content, PDF, etc. use a different path. Created when the first client
// connects over mojo.
class AXHostService : public service_manager::Service,
                      public ax::mojom::AXHost {
 public:
  AXHostService();
  ~AXHostService() override;

  // Requests AX node trees from remote clients and starts listening for remote
  // AX events. Static because the mojo service_manager creates and owns the
  // service object, but automation may be enabled before a client connects and
  // the service starts.
  static void SetAutomationEnabled(bool enabled);

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // ax::mojom::AXHost:
  void RegisterRemoteHost(ax::mojom::AXRemoteHostPtr remote_host_ptr,
                          RegisterRemoteHostCallback cb) override;
  void HandleAccessibilityEvent(const ui::AXTreeID& tree_id,
                                const std::vector<ui::AXTreeUpdate>& updates,
                                const ui::AXEvent& event) override;

  // Cleans up after a remote host disconnects.
  void OnRemoteHostDisconnected(const ui::AXTreeID& tree_id);

  void FlushForTesting();

 private:
  void AddBinding(ax::mojom::AXHostRequest request);

  // Notifies all remote trees of automation enabled state.
  void NotifyAutomationEnabled();

  static AXHostService* instance_;
  static bool automation_enabled_;

  service_manager::BinderRegistry registry_;
  mojo::BindingSet<ax::mojom::AXHost> bindings_;

  // Map from a child tree id to the remote host responsible for that tree.
  std::map<ui::AXTreeID, std::unique_ptr<AXRemoteHostDelegate>>
      remote_host_delegate_map_;

  DISALLOW_COPY_AND_ASSIGN(AXHostService);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_AX_HOST_SERVICE_H_
