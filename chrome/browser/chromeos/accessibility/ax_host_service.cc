// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/ax_host_service.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/accessibility/ax_remote_host_delegate.h"
#include "ui/accessibility/ax_tree_id.h"

AXHostService* AXHostService::instance_ = nullptr;

bool AXHostService::automation_enabled_ = false;

AXHostService::AXHostService() {
  DCHECK(!instance_);
  instance_ = this;
  registry_.AddInterface<ax::mojom::AXHost>(
      base::BindRepeating(&AXHostService::AddBinding, base::Unretained(this)));
}

AXHostService::~AXHostService() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
void AXHostService::SetAutomationEnabled(bool enabled) {
  automation_enabled_ = enabled;
  if (instance_)
    instance_->NotifyAutomationEnabled();
}

void AXHostService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void AXHostService::RegisterRemoteHost(
    ax::mojom::AXRemoteHostPtr remote_host_ptr,
    RegisterRemoteHostCallback cb) {
  // Create the AXRemoteHostDelegate first so a tree ID will be assigned.
  auto remote_host_delegate =
      std::make_unique<AXRemoteHostDelegate>(this, std::move(remote_host_ptr));
  ui::AXTreeID tree_id = remote_host_delegate->tree_id();
  DCHECK_NE(ui::AXTreeIDUnknown(), tree_id);
  DCHECK(!base::ContainsKey(remote_host_delegate_map_, tree_id));
  remote_host_delegate_map_[tree_id] = std::move(remote_host_delegate);

  // Inform the remote process of the tree ID.
  std::move(cb).Run(tree_id, automation_enabled_);
}

void AXHostService::HandleAccessibilityEvent(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const ui::AXEvent& event) {
  auto it = remote_host_delegate_map_.find(tree_id);
  if (it == remote_host_delegate_map_.end())
    return;
  AXRemoteHostDelegate* delegate = it->second.get();
  delegate->HandleAccessibilityEvent(tree_id, updates, event);
}

void AXHostService::OnRemoteHostDisconnected(const ui::AXTreeID& tree_id) {
  // AXRemoteHostDelegate notified the extension that the tree was destroyed.
  // Delete the AXRemoteHostDelegate.
  remote_host_delegate_map_.erase(tree_id);
}

void AXHostService::FlushForTesting() {
  for (const auto& pair : remote_host_delegate_map_) {
    AXRemoteHostDelegate* delegate = pair.second.get();
    delegate->FlushForTesting();
  }
}

void AXHostService::AddBinding(ax::mojom::AXHostRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AXHostService::NotifyAutomationEnabled() {
  for (const auto& pair : remote_host_delegate_map_) {
    AXRemoteHostDelegate* delegate = pair.second.get();
    delegate->OnAutomationEnabled(automation_enabled_);
  }
}
