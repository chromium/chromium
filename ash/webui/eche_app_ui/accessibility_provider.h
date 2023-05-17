// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_PROVIDER_H_
#define ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_PROVIDER_H_

#include "ash/webui/eche_app_ui/accessibility_tree_converter.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/ax_action_data.h"

namespace ash::eche_app {
class AccessibilityProvider : public mojom::AccessibilityProvider {
 public:
  AccessibilityProvider();
  ~AccessibilityProvider() override;
  // Proto from ash/webui/eche_app_ui/proto/accessibility_mojom.proto.
  void HandleAccessibilityEventReceived(
      const std::vector<uint8_t>& serialized_proto) override;
  void SetAccessibilityObserver(
      ::mojo::PendingRemote<mojom::AccessibilityObserver> observer) override;
  void Bind(mojo::PendingReceiver<mojom::AccessibilityProvider> receiver);

  void PerformAction(const ui::AXActionData& action);

 private:
  mojo::Receiver<mojom::AccessibilityProvider> receiver_{this};
  mojo::Remote<mojom::AccessibilityObserver> observer_remote_;
};
}  // namespace ash::eche_app
#endif  // ASH_WEBUI_ECHE_APP_UI_ACCESSIBILITY_PROVIDER_H_
