// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/service/accessibility_service_router.h"

#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/buildflags.h"

namespace ax {

AccessibilityServiceRouter::AccessibilityServiceRouter() = default;
AccessibilityServiceRouter::~AccessibilityServiceRouter() = default;

void AccessibilityServiceRouter::BindAccessibilityServiceClient(
    mojo::PendingRemote<mojom::AccessibilityServiceClient>
        accessibility_service_client) {
  LaunchIfNotRunning();

  if (accessibility_service_.is_bound()) {
    accessibility_service_->BindAccessibilityServiceClient(
        std::move(accessibility_service_client));
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

void AccessibilityServiceRouter::ConnectDevToolsAgent(
    ::mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
    mojom::AssistiveTechnologyType type) {
#if BUILDFLAG(ENABLE_ACCESSIBILITY_SERVICE)
  LaunchIfNotRunning();
  // Check to make sure the service was actually launched.
  CHECK(accessibility_service_.is_bound());
  accessibility_service_->ConnectDevToolsAgent(std::move(agent), type);
#endif
}

void AccessibilityServiceRouter::LaunchIfNotRunning() {
  if (accessibility_service_.is_bound())
    return;

#if BUILDFLAG(ENABLE_ACCESSIBILITY_SERVICE)
  content::ServiceProcessHost::Launch(
      accessibility_service_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("Accessibility Service")
          .Pass());
#else   // !BUILDFLAG(ENABLE_ACCESSIBILITY_SERVICE)
  LOG(ERROR)
      << "Accessibility service is missing but should have been launched. Have "
         "you set the buildflag, `enable_accessibility_service=true`?";
#endif  // !BUILDFLAG(ENABLE_ACCESSIBILITY_SERVICE)
}

}  // namespace ax
