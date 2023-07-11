// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/accessibility_service_devtools_delegate.h"

#include <sstream>

#include "content/public/browser/devtools_agent_host.h"

namespace ash {

AccessibilityServiceDevToolsDelegate::AccessibilityServiceDevToolsDelegate(
    ax::mojom::AssistiveTechnologyType type,
    ConnectDevToolsAgentCallback connect_devtools_callback)
    : type_(type), connect_devtools_callback_(connect_devtools_callback) {}

AccessibilityServiceDevToolsDelegate::~AccessibilityServiceDevToolsDelegate() =
    default;

std::string AccessibilityServiceDevToolsDelegate::GetType() const {
  return content::DevToolsAgentHost::kTypeAssistiveTechnology;
}

std::string AccessibilityServiceDevToolsDelegate::GetTitle() const {
  std::ostringstream typeStringStream;
  typeStringStream << type_;
  // Drop k from name.
  return typeStringStream.str().substr(1);
}

GURL AccessibilityServiceDevToolsDelegate::GetURL() const {
  std::string title;
  base::ReplaceChars(GetTitle(), " ", "_", &title);
  return GURL("assistive-technology://" + title);
}

bool AccessibilityServiceDevToolsDelegate::Activate() {
  // NO-OP - activate doesn't mean anything for an AT.
  return false;
}

bool AccessibilityServiceDevToolsDelegate::Close() {
  // NO-OP - Do not allow closing of agent.
  return false;
}

void AccessibilityServiceDevToolsDelegate::Reload() {}

bool AccessibilityServiceDevToolsDelegate::ForceIOSession() {
  // We don't need to force IOSession for our case.
  return false;
}

void AccessibilityServiceDevToolsDelegate::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent>
        agent_receiver) {
  connect_devtools_callback_.Run(std::move(agent_receiver), type_);
}

}  // namespace ash
