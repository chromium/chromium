// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/user_interface_impl.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "content/public/common/color_parser.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/accessibility/public/mojom/user_interface.mojom-shared.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

UserInterfaceImpl::UserInterfaceImpl() = default;

UserInterfaceImpl::~UserInterfaceImpl() = default;

void UserInterfaceImpl::Bind(
    mojo::PendingReceiver<ax::mojom::UserInterface> ui_receiver) {
  ui_receivers_.Add(this, std::move(ui_receiver));
}

void UserInterfaceImpl::DarkenScreen(bool darken) {
  AccessibilityManager::Get()->SetDarkenScreen(darken);
}

void UserInterfaceImpl::OpenSettingsSubpage(const std::string& subpage) {
  AccessibilityManager::Get()->OpenSettingsSubpage(subpage);
}

void UserInterfaceImpl::ShowConfirmationDialog(
    const std::string& title,
    const std::string& description,
    const std::optional<std::string>& cancel_name,
    ShowConfirmationDialogCallback callback) {
  // If a dialog is already being shown we do not show a new one.
  // Instead, return false through the callback on the new dialog
  // to indicate it was closed without the user taking any action.
  // This is consistent with AcceleratorController, and matches
  // the implementation in accessibility_controller_impl.cc.
  if (show_confirmation_dialog_callback_) {
    std::move(callback).Run(false);
    return;
  }
  show_confirmation_dialog_callback_ = std::move(callback);
  ash::AccessibilityController::Get()->ShowConfirmationDialog(
      base::UTF8ToUTF16(title), base::UTF8ToUTF16(description),
      l10n_util::GetStringUTF16(IDS_APP_CONTINUE),
      cancel_name ? base::UTF8ToUTF16(cancel_name.value())
                  : l10n_util::GetStringUTF16(IDS_APP_CANCEL),
      base::BindOnce(&UserInterfaceImpl::OnDialogResult,
                     weak_factory_.GetWeakPtr(), /* confirmed */ true),
      base::BindOnce(&UserInterfaceImpl::OnDialogResult,
                     weak_factory_.GetWeakPtr(), /* not confirmed */ false),
      base::BindOnce(&UserInterfaceImpl::OnDialogResult,
                     weak_factory_.GetWeakPtr(), /* not confirmed */ false));
}

void UserInterfaceImpl::OnDialogResult(bool confirmed) {
  std::move(show_confirmation_dialog_callback_).Run(confirmed);
  show_confirmation_dialog_callback_.Reset();
}

void UserInterfaceImpl::SetFocusRings(
    std::vector<ax::mojom::FocusRingInfoPtr> focus_rings,
    ax::mojom::AssistiveTechnologyType at_type) {
  auto* accessibility_manager = AccessibilityManager::Get();
  for (auto& focus_ring_info : focus_rings) {
    auto focus_ring = std::make_unique<ash::AccessibilityFocusRingInfo>();
    focus_ring->behavior = ash::FocusRingBehavior::PERSIST;

    for (const gfx::Rect& rect : focus_ring_info->rects) {
      focus_ring->rects_in_screen.emplace_back(rect);
    }
    const std::string id = accessibility_manager->GetFocusRingId(
        at_type,
        focus_ring_info->id.has_value() ? focus_ring_info->id.value() : "");
    if (focus_ring_info->color.has_value()) {
      focus_ring->color = focus_ring_info->color.value();
    }
    if (focus_ring_info->secondary_color.has_value()) {
      focus_ring->secondary_color = focus_ring_info->secondary_color.value();
    }
    if (focus_ring_info->background_color.has_value()) {
      focus_ring->background_color = focus_ring_info->background_color.value();
    }
    switch (focus_ring_info->type) {
      case ax::mojom::FocusType::kSolid:
        focus_ring->type = ash::FocusRingType::SOLID;
        break;
      case ax::mojom::FocusType::kDashed:
        focus_ring->type = ash::FocusRingType::DASHED;
        break;
      case ax::mojom::FocusType::kGlow:
        focus_ring->type = ash::FocusRingType::GLOW;
        break;
    }
    if (focus_ring_info->stacking_order.has_value()) {
      switch (focus_ring_info->stacking_order.value()) {
        case ax::mojom::FocusRingStackingOrder::kAboveAccessibilityBubbles:
          focus_ring->stacking_order =
              ash::FocusRingStackingOrder::ABOVE_ACCESSIBILITY_BUBBLES;
          break;
        case ax::mojom::FocusRingStackingOrder::kBelowAccessibilityBubbles:
          focus_ring->stacking_order =
              ash::FocusRingStackingOrder::BELOW_ACCESSIBILITY_BUBBLES;
          break;
      }
    }
    // Update the touch exploration controller so that synthesized touch events
    // are anchored within the focused object.
    // NOTE: The final anchor point will be determined by the first rect of the
    // final focus ring.
    if (!focus_ring->rects_in_screen.empty()) {
      accessibility_manager->SetTouchAccessibilityAnchorPoint(
          focus_ring->rects_in_screen[0].CenterPoint());
    }

    accessibility_manager->SetFocusRing(id, std::move(focus_ring));
  }
}

void UserInterfaceImpl::SetHighlights(const std::vector<gfx::Rect>& rects,
                                      SkColor color) {
  AccessibilityManager::Get()->SetHighlights(rects, color);
}

void UserInterfaceImpl::SetVirtualKeyboardVisible(bool is_visible) {
  AccessibilityController::Get()->SetVirtualKeyboardVisible(is_visible);
}

}  // namespace ash
