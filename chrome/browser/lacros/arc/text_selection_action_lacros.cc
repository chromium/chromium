// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/arc/text_selection_action_lacros.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/arc/start_smart_selection_action_menu.h"
#include "chromeos/lacros/lacros_service.h"

namespace arc {

TextSelectionActionLacros::TextSelectionActionLacros() = default;
TextSelectionActionLacros::~TextSelectionActionLacros() = default;

bool TextSelectionActionLacros::RequestTextSelectionActions(
    const std::string& text,
    ui::ResourceScaleFactor scale_factor,
    RequestTextSelectionActionsCallback callback) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return false;
  }

  if (service->GetInterfaceVersion(crosapi::mojom::Arc::Uuid_) <
      int{crosapi::mojom::Arc::MethodMinVersions::
              kRequestTextSelectionActionsMinVersion}) {
    LOG(WARNING) << "RequestTextSelectionActions is not supported in Lacros.";
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->RequestTextSelectionActions(
      text, crosapi::mojom::ScaleFactor(scale_factor),
      base::BindOnce(&TextSelectionActionLacros::OnRequestTextSelectionActions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void TextSelectionActionLacros::OnRequestTextSelectionActions(
    RequestTextSelectionActionsCallback callback,
    crosapi::mojom::RequestTextSelectionActionsStatus status,
    std::vector<crosapi::mojom::TextSelectionActionPtr> actions) {
  if (status ==
      crosapi::mojom::RequestTextSelectionActionsStatus::kArcNotAvailable) {
    LOG(WARNING) << "Failed to connect to ARC in ash-chrome.";
    std::move(callback).Run(std::vector<TextSelectionAction>());
    return;
  }

  std::vector<TextSelectionAction> converted_actions;
  for (const auto& action : actions) {
    converted_actions.emplace_back(TextSelectionAction(
        std::move(action->app_id), std::move(action->icon),
        ActivityName(std::move(action->activity->package_name),
                     std::move(action->activity->activity_name)),
        std::move(action->title),
        IntentInfo(std::move(action->action_intent->action),
                   std::move(action->action_intent->categories),
                   std::move(action->action_intent->data),
                   std::move(action->action_intent->type),
                   action->action_intent->ui_bypassed,
                   std::move(action->action_intent->extras))));
  }
  std::move(callback).Run(std::move(converted_actions));
}

}  // namespace arc
