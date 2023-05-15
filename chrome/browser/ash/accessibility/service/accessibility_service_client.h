// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_CLIENT_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_CLIENT_H_

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace content {
class BrowserContext;
}

namespace ash {
class AutomationClientImpl;

// The AccessibilityServiceClient in the Browser process interacts with the
// AccessibilityService process over mojom. It is responsible for communicating
// to the service which features are running and binding helper classes for the
// service.
// TODO(crbug.com/1355633): Move to ash/accessibility/service.
class AccessibilityServiceClient
    : public ax::mojom::AccessibilityServiceClient {
 public:
  AccessibilityServiceClient();
  AccessibilityServiceClient(const AccessibilityServiceClient&) = delete;
  AccessibilityServiceClient& operator=(const AccessibilityServiceClient&) =
      delete;
  ~AccessibilityServiceClient() override;

  // ax::mojom::AccessibilityServiceClient:
  void BindAutomation(mojo::PendingRemote<ax::mojom::Automation> automation,
                      mojo::PendingReceiver<ax::mojom::AutomationClient>
                          automation_client) override;

  void SetProfile(content::BrowserContext* profile);

  // Enables or disables accessibility features in the service.
  void SetChromeVoxEnabled(bool enabled);
  void SetSelectToSpeakEnabled(bool enabled);
  void SetSwitchAccessEnabled(bool enabled);
  void SetAutoclickEnabled(bool enabled);
  void SetMagnifierEnabled(bool enabled);
  void SetDictationEnabled(bool enabled);

 private:
  friend class AccessibilityServiceClientTest;

  // Called when the profile changes or on destruction. Disconnects all mojom
  // endpoints.
  void Reset();

  void EnableAssistiveTechnology(ax::mojom::AssistiveTechnologyType type,
                                 bool enabled);

  void LaunchAccessibilityServiceAndBind();

  std::unique_ptr<AutomationClientImpl> automation_client_;

  // Track the currently enabled features in case we disconnect from the service
  // and need to reconnect, for example when the profile changes.
  std::vector<ax::mojom::AssistiveTechnologyType> enabled_features_;

  raw_ptr<content::BrowserContext, ExperimentalAsh> profile_ = nullptr;

  // Here is the remote to the AT Controller, used to toggle features.
  mojo::Remote<ax::mojom::AssistiveTechnologyController> at_controller_;

  // This class receives mojom requests from the service via the interface
  // AccessibilityServiceClient.
  mojo::Receiver<ax::mojom::AccessibilityServiceClient> service_client_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_CLIENT_H_
