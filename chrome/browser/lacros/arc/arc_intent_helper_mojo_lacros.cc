// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/arc/arc_intent_helper_mojo_lacros.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/lacros/lacros_service.h"

namespace arc {

ArcIntentHelperMojoLacros::ArcIntentHelperMojoLacros() = default;
ArcIntentHelperMojoLacros::~ArcIntentHelperMojoLacros() = default;

bool ArcIntentHelperMojoLacros::RequestUrlHandlerList(
    const std::string& url,
    RequestUrlHandlerListCallback callback) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    std::move(callback).Run(std::vector<IntentHandlerInfo>());
    return false;
  }

  if (service->GetInterfaceVersion(crosapi::mojom::Arc::Uuid_) <
      int{crosapi::mojom::Arc::MethodMinVersions::
              kRequestUrlHandlerListMinVersion}) {
    LOG(WARNING) << "RequestUrlHandlerList is not supported in Lacros.";
    std::move(callback).Run(std::vector<IntentHandlerInfo>());
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->RequestUrlHandlerList(
      url, base::BindOnce(&ArcIntentHelperMojoLacros::OnRequestUrlHandlerList,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void ArcIntentHelperMojoLacros::OnRequestUrlHandlerList(
    RequestUrlHandlerListCallback callback,
    std::vector<crosapi::mojom::IntentHandlerInfoPtr> handlers,
    crosapi::mojom::RequestUrlHandlerListStatus status) {
  if (status == crosapi::mojom::RequestUrlHandlerListStatus::kArcNotAvailable) {
    LOG(WARNING) << "Faild to connect to ARC in ash-chrome.";
    return;
  }

  std::vector<IntentHandlerInfo> converted_handlers;
  for (auto const& handler : handlers) {
    converted_handlers.emplace_back(handler->name, handler->package_name,
                                    handler->activity_name);
  }
  std::move(callback).Run(std::move(converted_handlers));
}

bool ArcIntentHelperMojoLacros::RequestTextSelectionActions(
    const std::string& text,
    ui::ResourceScaleFactor scale_factor,
    RequestTextSelectionActionsCallback callback) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    std::move(callback).Run(std::vector<TextSelectionAction>());
    return false;
  }

  if (service->GetInterfaceVersion(crosapi::mojom::Arc::Uuid_) <
      int{crosapi::mojom::Arc::MethodMinVersions::
              kRequestTextSelectionActionsMinVersion}) {
    LOG(WARNING) << "RequestTextSelectionActions is not supported in Lacros.";
    std::move(callback).Run(std::vector<TextSelectionAction>());
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->RequestTextSelectionActions(
      text, crosapi::mojom::ScaleFactor(scale_factor),
      base::BindOnce(&ArcIntentHelperMojoLacros::OnRequestTextSelectionActions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void ArcIntentHelperMojoLacros::OnRequestTextSelectionActions(
    RequestTextSelectionActionsCallback callback,
    crosapi::mojom::RequestTextSelectionActionsStatus status,
    std::vector<crosapi::mojom::TextSelectionActionPtr> actions) {
  if (status ==
      crosapi::mojom::RequestTextSelectionActionsStatus::kArcNotAvailable) {
    LOG(WARNING) << "Failed to connect to ARC in ash-chrome.";
    return;
  }

  std::vector<TextSelectionAction> converted_actions;
  for (const auto& action : actions) {
    converted_actions.emplace_back(TextSelectionAction(
        std::move(action->app_id), std::move(action->icon),
        ActivityName(
            std::move(action->activity->package_name),
            std::move(action->activity->activity_name.value_or(std::string()))),
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

bool ArcIntentHelperMojoLacros::HandleUrl(const std::string& url,
                                          const std::string& package_name) {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return false;
  }

  if (service->GetInterfaceVersion(crosapi::mojom::Arc::Uuid_) <
      int{crosapi::mojom::Arc::MethodMinVersions::kHandleUrlMinVersion}) {
    LOG(WARNING) << "HandleUrl is not supported in Lacros.";
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->HandleUrl(url, package_name);
  return true;
}

}  // namespace arc
