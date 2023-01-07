// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/service/accessibility_service_router.h"

#include "content/public/browser/service_process_host.h"

namespace ax {

AccessibilityServiceRouter::AccessibilityServiceRouter() = default;
AccessibilityServiceRouter::~AccessibilityServiceRouter() = default;

void AccessibilityServiceRouter::BindAutomationWithClient(
    mojo::PendingRemote<mojom::AutomationClient> automation_client_remote,
    mojo::PendingReceiver<mojom::Automation> automation_receiver) {
  LaunchIfNotRunning();

  if (accessibility_service_.is_bound()) {
    accessibility_service_->BindAutomation(std::move(automation_client_remote),
                                           std::move(automation_receiver));
  }
}

void AccessibilityServiceRouter::BindAssistiveTechnologyController(
    mojo::PendingReceiver<mojom::AssistiveTechnologyController>
        at_controller_receiver,
    const std::vector<mojom::AssistiveTechnologyType>& enabled_features) {
  LaunchIfNotRunning();

  if (accessibility_service_.is_bound()) {
    accessibility_service_->BindAssistiveTechnologyController(
        std::move(at_controller_receiver), enabled_features);
  }
}

void AccessibilityServiceRouter::LaunchIfNotRunning() {
  if (accessibility_service_.is_bound())
    return;

  content::ServiceProcessHost::Launch(
      accessibility_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Accessibility Service")
          .Pass());
}

}  // namespace ax
