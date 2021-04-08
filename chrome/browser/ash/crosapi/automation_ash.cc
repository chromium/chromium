// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/automation_ash.h"

#include "base/bind.h"
#include "base/pickle.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/common/channel_info.h"
#include "components/exo/shell_surface_base.h"
#include "components/version_info/channel.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/extension_messages.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace crosapi {

AutomationAsh::AutomationAsh() {
  ui::AXActionHandlerRegistry::GetInstance()->AddObserver(this);
}

AutomationAsh::~AutomationAsh() {
  ui::AXActionHandlerRegistry::GetInstance()->RemoveObserver(this);
}

void AutomationAsh::BindReceiverDeprecated(
    mojo::PendingReceiver<mojom::Automation> pending_receiver) {}

void AutomationAsh::BindReceiver(
    mojo::PendingReceiver<mojom::AutomationFactory> pending_receiver) {
  automation_factory_receivers_.Add(this, std::move(pending_receiver));
}

void AutomationAsh::EnableDesktop() {
  desktop_enabled_ = true;
  for (auto& client : automation_client_remotes_) {
    client->Enable();
  }
}

void AutomationAsh::EnableTree(const ui::AXTreeID& tree_id) {
  if (!tree_id.token().has_value())
    return;

  for (auto& client : automation_client_remotes_) {
    client->EnableTree(tree_id.token().value());
  }
}

void AutomationAsh::RegisterAutomationClientDeprecated(
    mojo::PendingRemote<mojom::AutomationClient> client,
    const base::UnguessableToken& token) {}

void AutomationAsh::ReceiveEventPrototype(
    const std::string& event_bundle_string,
    bool root,
    const base::UnguessableToken& token,
    const std::string& window_id) {
  // This prototype method is only implemented on developer builds of Chrome. We
  // check for this by checking that the build of Chrome is unbranded.
  if (chrome::GetChannel() != version_info::Channel::UNKNOWN)
    return;

  base::Pickle pickle(event_bundle_string.data(), event_bundle_string.size());
  base::PickleIterator iterator(pickle);
  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  bool success =
      IPC::ParamTraits<ExtensionMsg_AccessibilityEventBundleParams>::Read(
          &pickle, &iterator, &event_bundle);
  if (!success) {
    LOG(ERROR) << "ExtensionMsg_AccessibilityEventBundleParams deserialization "
                  "failure";
    return;
  }

  extensions::AutomationEventRouter::GetInstance()->DispatchAccessibilityEvents(
      event_bundle.tree_id, std::move(event_bundle.updates),
      event_bundle.mouse_location, std::move(event_bundle.events));
}

void AutomationAsh::DispatchTreeDestroyedEvent(
    const base::UnguessableToken& tree_id) {
  // This prototype method is only implemented on developer builds of Chrome. We
  // check for this by checking that the build of Chrome is unbranded.
  if (chrome::GetChannel() != version_info::Channel::UNKNOWN)
    return;

  extensions::AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
      ui::AXTreeID::FromToken(tree_id), nullptr);
}

// Forwards an action to all crosapi clients. This has no effect on production
// builds of chrome. It exists for prototyping for developers.
void AutomationAsh::PerformAction(const ui::AXTreeID& tree_id,
                                  int32_t automation_node_id,
                                  const std::string& action_type,
                                  int32_t request_id,
                                  const base::DictionaryValue& optional_args) {
  // This prototype method is only implemented on developer builds of Chrome. We
  // check for this by checking that the build of Chrome is unbranded.
  if (chrome::GetChannel() != version_info::Channel::UNKNOWN)
    return;

  if (!tree_id.token().has_value())
    return;
  for (auto& client : automation_client_remotes_) {
    client->PerformActionPrototype(tree_id.token().value(), automation_node_id,
                                   action_type, request_id,
                                   optional_args.Clone());
  }
}

void AutomationAsh::BindAutomation(
    mojo::PendingRemote<crosapi::mojom::AutomationClient> automation_client,
    mojo::PendingReceiver<crosapi::mojom::Automation> automation) {
  mojo::Remote<mojom::AutomationClient> remote(std::move(automation_client));

  if (desktop_enabled_)
    remote->Enable();

  automation_client_remotes_.Add(std::move(remote));
  automation_receivers_.Add(this, std::move(automation));
}

}  // namespace crosapi
