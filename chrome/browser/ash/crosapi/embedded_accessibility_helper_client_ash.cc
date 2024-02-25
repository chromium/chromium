// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/embedded_accessibility_helper_client_ash.h"

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chromeos/crosapi/mojom/embedded_accessibility_helper.mojom.h"

namespace crosapi {

EmbeddedAccessibilityHelperClientAsh::EmbeddedAccessibilityHelperClientAsh() =
    default;
EmbeddedAccessibilityHelperClientAsh::~EmbeddedAccessibilityHelperClientAsh() =
    default;

void EmbeddedAccessibilityHelperClientAsh::
    BindEmbeddedAccessibilityHelperClient(
        mojo::PendingReceiver<crosapi::mojom::EmbeddedAccessibilityHelperClient>
            embeded_ax_helper_client) {
  embedded_ax_helper_receivers_.Add(this, std::move(embeded_ax_helper_client));
}

void EmbeddedAccessibilityHelperClientAsh::BindEmbeddedAccessibilityHelper(
    mojo::PendingRemote<crosapi::mojom::EmbeddedAccessibilityHelper>
        embedded_ax_helper) {
  embedded_ax_helper_remotes_.Add(std::move(embedded_ax_helper));
}

void EmbeddedAccessibilityHelperClientAsh::SpeakSelectedText() {
  ash::AccessibilityManager::Get()->OnSelectToSpeakContextMenuClick();
}

void EmbeddedAccessibilityHelperClientAsh::FocusChanged(
    const gfx::Rect& focus_bounds_in_screen) {
  if (ash::AccessibilityManager::Get()->IsFocusHighlightEnabled()) {
    ash::AccessibilityController::Get()->SetFocusHighlightRect(
        focus_bounds_in_screen);
  }
}

void EmbeddedAccessibilityHelperClientAsh::ClipboardCopyInActiveGoogleDoc(
    const std::string& url) {
  for (auto& remote : embedded_ax_helper_remotes_) {
    remote->ClipboardCopyInActiveGoogleDoc(url);
  }
}

void EmbeddedAccessibilityHelperClientAsh::
    BindEmbeddedAccessibilityHelperClientFactoryReceiver(
        mojo::PendingReceiver<
            crosapi::mojom::EmbeddedAccessibilityHelperClientFactory>
            receiver) {
  embedded_ax_helper_factory_receivers_.Add(this, std::move(receiver));
}


}  // namespace crosapi
