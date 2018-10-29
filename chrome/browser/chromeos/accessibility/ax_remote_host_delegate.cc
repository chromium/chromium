// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/ax_remote_host_delegate.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/accessibility/ax_host_service.h"
#include "chrome/browser/extensions/api/automation_internal/automation_event_router.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/aura/env.h"

AXRemoteHostDelegate::AXRemoteHostDelegate(AXHostService* host_service,
                                           ax::mojom::AXRemoteHostPtr ptr)
    : host_service_(host_service), remote_host_ptr_(std::move(ptr)) {
  DCHECK(host_service_);
  DCHECK(remote_host_ptr_);

  // AX tree ID is automatically assigned.
  DCHECK_NE(tree_id(), ui::AXTreeIDUnknown());

  // Handle both clean and unclean shutdown of the remote app.
  remote_host_ptr_.set_connection_error_handler(base::BindOnce(
      &AXRemoteHostDelegate::OnRemoteHostDisconnected, base::Unretained(this)));
}

AXRemoteHostDelegate::~AXRemoteHostDelegate() = default;

void AXRemoteHostDelegate::OnAutomationEnabled(bool enabled) {
  remote_host_ptr_->OnAutomationEnabled(enabled);
}

void AXRemoteHostDelegate::HandleAccessibilityEvent(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const ui::AXEvent& event) {
  CHECK_EQ(tree_id, this->tree_id());
  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id;
  for (const ui::AXTreeUpdate& update : updates)
    event_bundle.updates.push_back(update);
  event_bundle.events.push_back(event);
  event_bundle.mouse_location = aura::Env::GetInstance()->last_mouse_location();

  // Forward the tree updates and the event to the accessibility extension.
  extensions::AutomationEventRouter::GetInstance()->DispatchAccessibilityEvents(
      event_bundle);
}

void AXRemoteHostDelegate::PerformAction(const ui::AXActionData& data) {
  // Send to remote host.
  remote_host_ptr_->PerformAction(data);
}

void AXRemoteHostDelegate::FlushForTesting() {
  remote_host_ptr_.FlushForTesting();
}

void AXRemoteHostDelegate::OnRemoteHostDisconnected() {
  extensions::AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
      tree_id(), nullptr /* browser_context */);
  host_service_->OnRemoteHostDisconnected(tree_id());
  // This object is now deleted.
}
