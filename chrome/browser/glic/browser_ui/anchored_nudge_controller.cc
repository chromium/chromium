// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/anchored_nudge_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/actions.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/menus/simple_menu_model.h"

namespace glic {

AnchoredNudgeController::AnchoredNudgeController(
    BrowserWindowInterface& browser_window_interface)
    : browser_window_interface_(browser_window_interface) {}

AnchoredNudgeController::~AnchoredNudgeController() = default;

void AnchoredNudgeController::OnTriggerGlicNudgeUI(NudgeParams params) {
  if (params.label.empty()) {
    return;
  }

  auto* controller = GetPageActionController();
  if (!controller) {
    return;
  }

  // Find the ActionItem for contextual cueing, registered at browser
  // initialization in BrowserActions::InitializeBrowserActions().
  auto* action =
      actions::ActionManager::Get().FindAction(kActionGlicContextualCueing);
  if (!action) {
    return;
  }

  // Set the chip text to the cue label.
  action->SetText(base::UTF8ToUTF16(params.label));
  action->SetImage(ui::ImageModel::FromVectorIcon(
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON),
      ui::kColorSysOnSurface, 18));
  action->SetEnabled(true);
  action->SetVisible(true);
  action->SetInvokeActionCallback(base::BindRepeating(
      [](base::WeakPtr<BrowserWindowInterface> bwi,
         std::optional<std::string> prompt, actions::ActionItem* item,
         actions::ActionInvocationContext context) {
        if (!bwi) {
          return;
        }
        if (auto* glic_service =
                glic::GlicKeyedService::Get(bwi->GetProfile())) {
          if (tabs::TabInterface* tab = bwi->GetActiveTabInterface()) {
            glic::GlicInvokeOptions options(
                glic::Target(tab),
                glic::mojom::InvocationSource::kAnchoredContextualCue);
            if (prompt.has_value()) {
              options.prompts.emplace_back(std::move(*prompt));
            }
            glic_service->InvokeWithAutoSubmit(
                glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
                std::move(options));
          }
        }
      },
      browser_window_interface_->GetWeakPtr(),
      std::move(params.prompt_suggestion)));

  anchored_message_subscription_ =
      controller->CreateActionItemSubscription(action);
  controller->Show(kActionGlicContextualCueing);
  // The secondary label becomes the anchored message bubble text.
  controller->SetAnchoredMessageText(
      kActionGlicContextualCueing,
      base::UTF8ToUTF16(params.anchored_message_text));
  gfx::ImageSkia* icon =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_GLIC_BUTTON_ALT_ICON);
  controller->SetAnchoredMessageIcon(
      kActionGlicContextualCueing,
      icon ? ui::ImageModel::FromImageSkia(*icon) : ui::ImageModel());
  controller->SetAnchoredMessageAction(
      kActionGlicContextualCueing,
      page_actions::AnchoredMessageActionIconType::kClose, /*model=*/nullptr);
  controller->ShowAnchoredMessage(
      kActionGlicContextualCueing,
      {.priority = page_actions::PageActionPriorityCategory::kContextualCue});
}

void AnchoredNudgeController::OnHideGlicNudgeUI() {
  anchored_message_subscription_ = {};
  if (auto* controller = GetPageActionController()) {
    controller->Hide(kActionGlicContextualCueing);
  }
}

bool AnchoredNudgeController::GetIsShowingGlicNudge() {
  return !!anchored_message_subscription_;
}

page_actions::PageActionController*
AnchoredNudgeController::GetPageActionController() {
  if (auto* active_tab = browser_window_interface_->GetActiveTabInterface()) {
    if (auto* tab_features = active_tab->GetTabFeatures()) {
      return tab_features->page_action_controller();
    }
  }
  return nullptr;
}

}  // namespace glic
