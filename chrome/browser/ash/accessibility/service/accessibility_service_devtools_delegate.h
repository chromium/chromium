// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_DEVTOOLS_DELEGATE_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_DEVTOOLS_DELEGATE_H_

#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/mojom_devtools_agent_host_delegate.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-shared.h"

namespace ash {

// AccessibilityServiceDevToolsDelegate provides information to devtools for a
// specific AT. These delegates are used to created AgentHosts which are then
// stored by AccessibilityServiceClient. This class will live as long as it's
// agent host does.
class AccessibilityServiceDevToolsDelegate
    : public content::MojomDevToolsAgentHostDelegate {
 public:
  using ConnectDevToolsAgentCallback = base::RepeatingCallback<void(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent>
          agent_receiver,
      ax::mojom::AssistiveTechnologyType)>;

  // Created using a assistive technology type such as chromevox and a callback
  // that will be used to pass a blink::mojom::DevToolsAgent receiver. The
  // callback will be called in |ConnectDevToolsAgent| at which point the passed
  // receiver and |this| type will be run with the callback.
  AccessibilityServiceDevToolsDelegate(
      ax::mojom::AssistiveTechnologyType type,
      ConnectDevToolsAgentCallback connect_devtools_callback);

  AccessibilityServiceDevToolsDelegate(
      const AccessibilityServiceDevToolsDelegate&) = delete;
  AccessibilityServiceDevToolsDelegate operator=(
      const AccessibilityServiceDevToolsDelegate&) = delete;

  ~AccessibilityServiceDevToolsDelegate() override;

  // content::MojomDevToolsAgentHostDelegate overrides:
  std::string GetType() const override;
  std::string GetTitle() const override;
  GURL GetURL() const override;
  bool Activate() override;
  bool Close() override;
  void Reload() override;
  bool ForceIOSession() override;
  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent>
          agent_receiver) override;

 private:
  ax::mojom::AssistiveTechnologyType type_;
  ConnectDevToolsAgentCallback connect_devtools_callback_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SERVICE_ACCESSIBILITY_SERVICE_DEVTOOLS_DELEGATE_H_
