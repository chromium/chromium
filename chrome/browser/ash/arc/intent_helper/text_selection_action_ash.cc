// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/text_selection_action_ash.h"

#include "ash/components/arc/mojom/scale_factor.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/barrier_closure.h"
#include "base/callback.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

namespace arc {

namespace {

// Used for converting RawIconPngData to ImageSkia.
constexpr size_t kBytesPerPixel = 4;  // BGRA
constexpr size_t kSmallIconSizeInDip = 16;
constexpr size_t kMaxIconSizeInPx = 200;

}  // namespace

TextSelectionActionAsh::TextSelectionActionAsh() = default;
TextSelectionActionAsh::~TextSelectionActionAsh() = default;

bool TextSelectionActionAsh::RequestTextSelectionActions(
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
    return false;
  }

  instance->RequestTextSelectionActions(
      text, mojom::ScaleFactor(scale_factor),
      base::BindOnce(&TextSelectionActionAsh::OnRequestTextSelectionActions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  return true;
}

void TextSelectionActionAsh::OnRequestTextSelectionActions(
    RequestTextSelectionActionsCallback callback,
    std::vector<mojom::TextSelectionActionPtr> actions) {
  size_t actions_count = actions.size();
  auto converted_actions = std::vector<TextSelectionAction*>(actions_count);
  TextSelectionAction** converted_actions_ptr = &converted_actions[0];
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

    apps::ArcRawIconPngDataToImageSkia(
        std::move(action->icon->icon_png_data), kSmallIconSizeInDip,
        base::BindOnce(&TextSelectionActionAsh::ConvertTextSelectionAction,
                       weak_ptr_factory_.GetWeakPtr(), converted_action,
                       std::move(action), barrier_closure));
  }
}

void TextSelectionActionAsh::ConvertTextSelectionAction(
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
      ActivityName(std::move(action->activity->package_name),
                   std::move(action->activity->activity_name)),
      std::move(action->title),
      IntentInfo(std::move(action->action_intent->action),
                 std::move(action->action_intent->categories),
                 std::move(action->action_intent->data),
                 std::move(action->action_intent->type),
                 action->action_intent->ui_bypassed,
                 std::move(action->action_intent->extras)));

  std::move(callback).Run();
}

}  // namespace arc
