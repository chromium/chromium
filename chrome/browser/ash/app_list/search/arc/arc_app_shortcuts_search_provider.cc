// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/arc/arc_app_shortcuts_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/search/arc/arc_app_shortcut_search_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace app_list {

namespace {
using ::ash::string_matching::TokenizedString;
}  // namespace

ArcAppShortcutsSearchProvider::ArcAppShortcutsSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : SearchProvider(SearchCategory::kAppShortcuts),
      max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller) {}

ArcAppShortcutsSearchProvider::~ArcAppShortcutsSearchProvider() = default;

ash::AppListSearchResultType ArcAppShortcutsSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kArcAppShortcut;
}

void ArcAppShortcutsSearchProvider::Start(const std::u16string& query) {
  DCHECK(!query.empty());

  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetAppShortcutGlobalQueryItems)
          : nullptr;

  if (!app_instance)
    return;
  last_query_ = query;

  // Invalidate the weak ptr to prevent previous callback run.
  weak_ptr_factory_.InvalidateWeakPtrs();
  app_instance->GetAppShortcutGlobalQueryItems(
      base::UTF16ToUTF8(query), max_results_,
      base::BindOnce(
          &ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems,
          weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppShortcutsSearchProvider::StopQuery() {
  last_query_.clear();

  // Invalidate the weak ptr to prevent previous callback run.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems(
    std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);

  TokenizedString tokenized_query(last_query_, TokenizedString::Mode::kWords);

  SearchProvider::Results search_results;
  for (auto& item : shortcut_items) {
    const std::string app_id =
        arc_prefs->GetAppIdByPackageName(item->package_name.value());
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    // Ignore shortcuts for apps that are not present in the launcher.
    if (!app_info || !app_info->show_in_launcher)
      continue;
    search_results.emplace_back(std::make_unique<ArcAppShortcutSearchResult>(
        std::move(item), profile_, list_controller_,
        false /*is_recommendation*/, tokenized_query, app_info->name));
  }
  SwapResults(&search_results);
}

}  // namespace app_list
