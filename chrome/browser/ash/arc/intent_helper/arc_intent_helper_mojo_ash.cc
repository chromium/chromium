// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/intent_helper/arc_intent_helper_mojo_ash.h"

#include "ash/components/arc/mojom/scale_factor.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"

namespace arc {

namespace {

// Used for converting RawIconPngData to ImageSkia.
constexpr size_t kBytesPerPixel = 4;  // BGRA
constexpr size_t kSmallIconSizeInDip = 16;
constexpr size_t kMaxIconSizeInPx = 200;

}  // namespace

ArcIntentHelperMojoAsh::ArcIntentHelperMojoAsh() = default;
ArcIntentHelperMojoAsh::~ArcIntentHelperMojoAsh() = default;

bool ArcIntentHelperMojoAsh::IsArcAvailable() {
  auto* arc_service_manager = ArcServiceManager::Get();
  return arc_service_manager && arc_service_manager->arc_bridge_service()
                                    ->intent_helper()
                                    ->IsConnected();
}

bool ArcIntentHelperMojoAsh::IsRequestUrlHandlerListAvailable() {
  auto* arc_service_manager = ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        RequestUrlHandlerList);
  }
  return instance;
}

bool ArcIntentHelperMojoAsh::IsRequestTextSelectionActionsAvailable() {
  auto* arc_service_manager = ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        RequestTextSelectionActions);
  }
  return instance;
}

bool ArcIntentHelperMojoAsh::RequestUrlHandlerList(
    const std::string& url,
    RequestUrlHandlerListCallback callback) {
  auto* arc_service_manager = ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        RequestUrlHandlerList);
  }
  if (!instance) {
    LOG(ERROR) << "Failed to get instance for RequestUrlHandlerList().";
    std::move(callback).Run(std::vector<IntentHandlerInfo>());
    return false;
  }

  instance->RequestUrlHandlerList(
      url, base::BindOnce(&ArcIntentHelperMojoAsh::OnRequestUrlHandlerList,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void ArcIntentHelperMojoAsh::OnRequestUrlHandlerList(
    RequestUrlHandlerListCallback callback,
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  std::vector<IntentHandlerInfo> converted_handlers;
  for (auto const& handler : handlers) {
    converted_handlers.push_back(IntentHandlerInfo(
        handler->name, handler->package_name, handler->activity_name,
        handler->is_preferred, handler->fallback_url));
  }
  std::move(callback).Run(std::move(converted_handlers));
}

bool ArcIntentHelperMojoAsh::RequestTextSelectionActions(
    const std::string& text,
    ui::ResourceScaleFactor scale_factor,
    RequestTextSelectionActionsCallback callback) {
  auto* arc_service_manager = ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        RequestTextSelectionActions);
  }
  if (!instance) {
    LOG(ERROR) << "Failed to get instance for RequestTextSelectionActions().";
    std::move(callback).Run(std::vector<TextSelectionAction>());
    return false;
  }

  instance->RequestTextSelectionActions(
      text, mojom::ScaleFactor(scale_factor),
      base::BindOnce(&ArcIntentHelperMojoAsh::OnRequestTextSelectionActions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void ArcIntentHelperMojoAsh::OnRequestTextSelectionActions(
    RequestTextSelectionActionsCallback callback,
    std::vector<mojom::TextSelectionActionPtr> actions) {
  size_t actions_count = actions.size();
  auto converted_actions = std::vector<TextSelectionAction*>(actions_count);
  TextSelectionAction** converted_actions_ptr = converted_actions.data();
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      actions_count, base::BindOnce(
                         [](std::vector<TextSelectionAction*> actions,
                            RequestTextSelectionActionsCallback cb) {
                           std::vector<TextSelectionAction> actions_value;
                           for (auto* const action : actions) {
                             actions_value.push_back(std::move(*action));
                             // Delete each action explicitly to avoid a memory
                             // leak.
                             delete action;
                           }
                           std::move(cb).Run(std::move(actions_value));
                         },
                         std::move(converted_actions), std::move(callback)));

  for (size_t idx = 0; idx < actions_count; ++idx) {
    auto action = std::move(actions[idx]);
    TextSelectionAction** converted_action = &converted_actions_ptr[idx];

    // If action->icon doesn't meet the size condition, skip generating image.
    if (action->icon->width > kMaxIconSizeInPx ||
        action->icon->height > kMaxIconSizeInPx || action->icon->width == 0 ||
        action->icon->height == 0 ||
        action->icon->icon.size() !=
            (action->icon->width * action->icon->height * kBytesPerPixel)) {
      ConvertTextSelectionAction(converted_action, std::move(action),
                                 barrier_closure, gfx::ImageSkia());
      continue;
    }

    auto icon_png_data = std::move(action->icon->icon_png_data);
    apps::ArcRawIconPngDataToImageSkia(
        std::move(icon_png_data), kSmallIconSizeInDip,
        base::BindOnce(&ArcIntentHelperMojoAsh::ConvertTextSelectionAction,
                       weak_ptr_factory_.GetWeakPtr(), converted_action,
                       std::move(action), barrier_closure));
  }
}

void ArcIntentHelperMojoAsh::ConvertTextSelectionAction(
    TextSelectionAction** converted_action,
    mojom::TextSelectionActionPtr action,
    base::OnceClosure callback,
    const gfx::ImageSkia& image) {
  // Convert actions to crosapi::mojom::TextSelectionActionPtr from
  // arc::mojom::TextSelectionActionPtr and ImageSkia icon.

  // Generate app_id by looking up ArcAppListPrefs.
  Profile* profile = ArcSessionManager::Get()->profile();
  std::string app_id = ArcAppListPrefs::Get(profile)->GetAppIdByPackageName(
      action->activity->package_name);

  // The memory for this value is released at the barrier closure in
  // OnRequestTextSelectionAction() by explicitly deleting it.
  *converted_action = new TextSelectionAction(
      std::move(app_id), image,
      ActivityName(
          std::move(action->activity->package_name),
          std::move(action->activity->activity_name.value_or(std::string()))),
      std::move(action->title),
      IntentInfo(std::move(action->action_intent->action),
                 std::move(action->action_intent->categories),
                 std::move(action->action_intent->data),
                 std::move(action->action_intent->type),
                 action->action_intent->ui_bypassed,
                 std::move(action->action_intent->extras)));

  std::move(callback).Run();
}

bool ArcIntentHelperMojoAsh::HandleUrl(const std::string& url,
                                       const std::string& package_name) {
  auto* arc_service_manager = ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  }
  if (!instance) {
    LOG(ERROR) << "Failed to get instance for HandleUrl().";
    return false;
  }

  instance->HandleUrl(url, package_name);
  return true;
}

bool ArcIntentHelperMojoAsh::HandleIntent(const IntentInfo& intent,
                                          const ActivityName& activity) {
  auto* arc_service_manager = ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        HandleIntent);
  }
  if (!instance) {
    LOG(ERROR) << "Failed to get instance for HandleIntent().";
    return false;
  }

  mojom::IntentInfoPtr converted_intent = mojom::IntentInfo::New();
  converted_intent->action = intent.action;
  converted_intent->categories = intent.categories;
  converted_intent->data = intent.data;
  converted_intent->type = intent.type;
  converted_intent->ui_bypassed = intent.ui_bypassed;
  converted_intent->extras = intent.extras;
  instance->HandleIntent(
      std::move(converted_intent),
      mojom::ActivityName::New(activity.package_name, activity.activity_name));
  return true;
}

bool ArcIntentHelperMojoAsh::AddPreferredPackage(
    const std::string& package_name) {
  auto* arc_service_manager = ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;

  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(),
        AddPreferredPackage);
  }
  if (!instance) {
    LOG(ERROR) << "Failed to get instance for AddPreferedPackage().";
    return false;
  }

  instance->AddPreferredPackage(package_name);
  return true;
}

}  // namespace arc
