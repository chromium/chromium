// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/arc/arc_intent_helper_mojo_lacros.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chromeos/lacros/lacros_service.h"

namespace arc {

ArcIntentHelperMojoLacros::ArcIntentHelperMojoLacros() = default;
ArcIntentHelperMojoLacros::~ArcIntentHelperMojoLacros() = default;

bool ArcIntentHelperMojoLacros::IsArcAvailable() {
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::Arc>()) {
    LOG(WARNING) << "ARC is not supported in Lacros.";
    return false;
  }
  return true;
}

bool ArcIntentHelperMojoLacros::IsRequestUrlHandlerListAvailable() {
  auto* service = chromeos::LacrosService::Get();
  return service && service->IsAvailable<crosapi::mojom::Arc>() &&
         service->GetInterfaceVersion<crosapi::mojom::Arc>() >=
             int{crosapi::mojom::Arc::MethodMinVersions::
                     kRequestUrlHandlerListMinVersion};
}

bool ArcIntentHelperMojoLacros::IsRequestTextSelectionActionsAvailable() {
  auto* service = chromeos::LacrosService::Get();
  return service && service->IsAvailable<crosapi::mojom::Arc>() &&
         service->GetInterfaceVersion<crosapi::mojom::Arc>() >=
             int{crosapi::mojom::Arc::MethodMinVersions::
                     kRequestTextSelectionActionsMinVersion};
}

bool ArcIntentHelperMojoLacros::RequestUrlHandlerList(
    const std::string& url,
    RequestUrlHandlerListCallback callback) {
  if (!IsArcAvailable()) {
    std::move(callback).Run(std::vector<IntentHandlerInfo>());
    return false;
  }

  auto* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion<crosapi::mojom::Arc>() <
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
    std::move(callback).Run(std::vector<IntentHandlerInfo>());
    return;
  }

  std::vector<IntentHandlerInfo> converted_handlers;
  for (auto const& handler : handlers) {
    converted_handlers.emplace_back(
        handler->name, handler->package_name, handler->activity_name,
        handler->is_preferred, handler->fallback_url);
  }
  std::move(callback).Run(std::move(converted_handlers));
}

bool ArcIntentHelperMojoLacros::RequestTextSelectionActions(
    const std::string& text,
    ui::ResourceScaleFactor scale_factor,
    RequestTextSelectionActionsCallback callback) {
  if (!IsArcAvailable()) {
    std::move(callback).Run(std::vector<TextSelectionAction>());
    return false;
  }

  auto* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion<crosapi::mojom::Arc>() <
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
    std::move(callback).Run(std::vector<TextSelectionAction>());
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
  if (!IsArcAvailable())
    return false;

  auto* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion<crosapi::mojom::Arc>() <
      int{crosapi::mojom::Arc::MethodMinVersions::kHandleUrlMinVersion}) {
    LOG(WARNING) << "HandleUrl is not supported in Lacros.";
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->HandleUrl(url, package_name);
  return true;
}

bool ArcIntentHelperMojoLacros::HandleIntent(const IntentInfo& intent,
                                             const ActivityName& activity) {
  if (!IsArcAvailable())
    return false;

  auto* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion<crosapi::mojom::Arc>() <
      int{crosapi::mojom::Arc::MethodMinVersions::kHandleIntentMinVersion}) {
    LOG(WARNING) << "HandleIntent is not supported in Lacros.";
    return false;
  }

  crosapi::mojom::IntentInfoPtr converted_intent =
      crosapi::mojom::IntentInfo::New();
  converted_intent->action = intent.action;
  converted_intent->categories = intent.categories;
  converted_intent->data = intent.data;
  converted_intent->type = intent.type;
  converted_intent->ui_bypassed = intent.ui_bypassed;
  converted_intent->extras = intent.extras;
  service->GetRemote<crosapi::mojom::Arc>()->HandleIntent(
      std::move(converted_intent),
      crosapi::mojom::ActivityName::New(activity.package_name,
                                        activity.activity_name));
  return true;
}

bool ArcIntentHelperMojoLacros::AddPreferredPackage(
    const std::string& package_name) {
  if (!IsArcAvailable())
    return false;

  auto* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion<crosapi::mojom::Arc>() <
      int{crosapi::mojom::Arc::MethodMinVersions::
              kAddPreferredPackageMinVersion}) {
    LOG(WARNING) << "AddPreferredPackage is not supported in Lacros.";
    return false;
  }

  service->GetRemote<crosapi::mojom::Arc>()->AddPreferredPackage(package_name);
  return true;
}

}  // namespace arc
