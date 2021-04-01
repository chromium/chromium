// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/start_smart_selection_action_menu.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/models/image_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace arc {

// The maximum number of smart actions to show.
constexpr size_t kMaxMainMenuCommands = 5;

constexpr size_t kSmallIconSizeInDip = 16;
constexpr size_t kMaxIconSizeInPx = 200;

StartSmartSelectionActionMenu::StartSmartSelectionActionMenu(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {}

StartSmartSelectionActionMenu::~StartSmartSelectionActionMenu() = default;

void StartSmartSelectionActionMenu::InitMenu(
    const content::ContextMenuParams& params) {
  const std::string converted_text = base::UTF16ToUTF8(params.selection_text);
  if (converted_text.empty())
    return;

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestTextSelectionActions);
  if (!instance)
    return;

  base::RecordAction(base::UserMetricsAction("Arc.SmartTextSelection.Request"));
  instance->RequestTextSelectionActions(
      converted_text, mojom::ScaleFactor(ui::GetSupportedScaleFactors().back()),
      base::BindOnce(&StartSmartSelectionActionMenu::HandleTextSelectionActions,
                     weak_ptr_factory_.GetWeakPtr()));

  // Add placeholder items.
  for (size_t i = 0; i < kMaxMainMenuCommands; ++i) {
    proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 + i,
                        /*title=*/std::u16string());
  }
}

bool StartSmartSelectionActionMenu::IsCommandIdSupported(int command_id) {
  return command_id >= IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 &&
         command_id <= IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION_LAST;
}

bool StartSmartSelectionActionMenu::IsCommandIdChecked(int command_id) {
  return false;
}

bool StartSmartSelectionActionMenu::IsCommandIdEnabled(int command_id) {
  return true;
}

void StartSmartSelectionActionMenu::ExecuteCommand(int command_id) {
  if (!IsCommandIdSupported(command_id))
    return;

  size_t index = command_id - IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1;
  if (actions_.size() <= index)
    return;

  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleIntent);
  if (!instance)
    return;

  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);

  Profile* profile = ArcSessionManager::Get()->profile();
  apps::AppServiceProxyFactory::GetForProfile(profile)->LaunchAppWithIntent(
      ArcAppListPrefs::Get(profile)->GetAppIdByPackageName(
          actions_[index]->activity->package_name),
      ui::EF_NONE,
      apps_util::CreateIntentForArcIntentAndActivity(
          std::move(actions_[index]->action_intent),
          std::move(actions_[index]->activity)),
      apps::mojom::LaunchSource::kFromSmartTextContextMenu,
      apps::MakeWindowInfo(display.id()));
}

void StartSmartSelectionActionMenu::HandleTextSelectionActions(
    std::vector<mojom::TextSelectionActionPtr> actions) {
  actions_ = std::move(actions);

  for (size_t i = 0; i < actions_.size(); ++i) {
    proxy_->UpdateMenuItem(
        IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 + i,
        /*enabled=*/true,
        /*hidden=*/false,
        /*title=*/base::UTF8ToUTF16(actions_[i]->title));

    if (actions_[i]->icon) {
      UpdateMenuIcon(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 + i,
                     std::move(actions_[i]->icon));
    }
  }

  for (size_t i = actions_.size(); i < kMaxMainMenuCommands; ++i) {
    // There were fewer actions returned than placeholder slots, remove the
    // empty menu item.
    proxy_->RemoveMenuItem(IDC_CONTENT_CONTEXT_START_SMART_SELECTION_ACTION1 +
                           i);
  }

  // The asynchronous nature of adding smart actions means that sometimes,
  // depending on whether actions were found and if extensions menu items were
  // added synchronously, there could be extra (adjacent) separators in the
  // context menu that must be removed once we've finished loading everything.
  proxy_->RemoveAdjacentSeparators();
}

void StartSmartSelectionActionMenu::UpdateMenuIcon(
    int command_id,
    mojom::ActivityIconPtr icon) {
  constexpr size_t kBytesPerPixel = 4;  // BGRA
  if (icon->width > kMaxIconSizeInPx || icon->height > kMaxIconSizeInPx ||
      icon->width == 0 || icon->height == 0 ||
      icon->icon.size() != (icon->width * icon->height * kBytesPerPixel)) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon)) {
    DCHECK(icon->icon_png_data);
    apps::ArcRawIconPngDataToImageSkia(
        std::move(icon->icon_png_data), kSmallIconSizeInDip,
        base::BindOnce(&StartSmartSelectionActionMenu::SetMenuIcon,
                       weak_ptr_factory_.GetWeakPtr(), command_id));
    return;
  }

  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(icon->width, icon->height));
  if (!bitmap.getPixels())
    return;

  DCHECK_GE(bitmap.computeByteSize(), icon->icon.size());
  memcpy(bitmap.getPixels(), &icon->icon.front(), icon->icon.size());

  gfx::ImageSkia original(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  gfx::ImageSkia icon_small(gfx::ImageSkiaOperations::CreateResizedImage(
      original, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kSmallIconSizeInDip, kSmallIconSizeInDip)));

  SetMenuIcon(command_id, icon_small);
}

void StartSmartSelectionActionMenu::SetMenuIcon(int command_id,
                                                const gfx::ImageSkia& image) {
  proxy_->UpdateMenuIcon(command_id, ui::ImageModel::FromImageSkia(image));
}

}  // namespace arc
