// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_shortcuts_search_result.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_service/app_service_shortcut_icon_loader.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/icon_types.h"

namespace {
constexpr char kAppShortcutSearchPrefix[] = "appshortcutsearch://";
}  // namespace

namespace app_list {

AppShortcutSearchResult::AppShortcutSearchResult(
    const apps::ShortcutId& shortcut_id,
    const std::u16string& title,
    Profile* profile,
    AppListControllerDelegate* app_list_controller,
    double relevance)
    : shortcut_id_(shortcut_id),
      profile_(profile),
      app_list_controller_(app_list_controller) {
  DCHECK(profile);

  set_id(kAppShortcutSearchPrefix + shortcut_id.value());
  SetTitle(title);
  SetCategory(Category::kAppShortcuts);
  SetResultType(ash::AppListSearchResultType::kAppShortcutV2);
  SetDisplayType(ash::SearchResultDisplayType::kList);
  SetMetricsType(ash::APP_SHORTCUTS_V2);
  set_relevance(relevance);

  icon_loader_ = std::make_unique<AppServiceShortcutIconLoader>(
      profile_,
      ash::SharedAppListConfig::instance().search_list_icon_dimension(),
      ash::SharedAppListConfig::instance().search_list_badge_icon_dimension(),
      /*delegate=*/this);

  icon_loader_->FetchImage(shortcut_id_.value());
}

AppShortcutSearchResult::~AppShortcutSearchResult() = default;

void AppShortcutSearchResult::Open(int event_flags) {
  int64_t display_id = app_list_controller_->GetAppListDisplayId();
  apps::AppServiceProxyFactory::GetForProfile(profile_)->LaunchShortcut(
      shortcut_id_, display_id);
}

void AppShortcutSearchResult::OnAppImageUpdated(
    const std::string& app_id,
    const gfx::ImageSkia& image,
    bool is_placeholder_icon,
    const std::optional<gfx::ImageSkia>& badge_image) {
  SetIcon(IconInfo(
      ui::ImageModel::FromImageSkia(image),
      ash::SharedAppListConfig::instance().search_list_icon_dimension()));

  if (badge_image) {
    SetBadgeIcon(ui::ImageModel::FromImageSkia(*badge_image));
  }
}

}  // namespace app_list
