// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"

#include <unordered_set>
#include <variant>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_switches.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/coral_util.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/coral/coral_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/wm/core/window_util.h"

// Implement custom hash for TabPtr because GURL doesn't support hash.
// We can dedup by possibly_invalid_spec() as it's how we transform GURL
// back to strings.
namespace std {
template <>
struct hash<coral::mojom::TabPtr> {
  inline size_t operator()(const coral::mojom::TabPtr& tab) const {
    std::size_t h1 = std::hash<std::string>{}(tab->title);
    std::size_t h2 = std::hash<std::string>{}(tab->url.possibly_invalid_spec());
    return h1 ^ (h2 << 1);
  }
};
}  // namespace std

namespace ash {
namespace {

bool HasValidClusterCount(size_t num_clusters) {
  return num_clusters <= 2;
}

bool IsBrowserWindow(aura::Window* window) {
  return window->GetProperty(chromeos::kAppTypeKey) ==
         chromeos::AppType::BROWSER;
}

bool IsValidInSessionWindow(aura::Window* window) {
  auto* delegate = Shell::Get()->saved_desk_delegate();

  // We should guarantee the window can be launched in saved desk template.
  if (!delegate->IsWindowSupportedForSavedDesk(window)) {
    return false;
  }

  // The window should belongs to the current active user.
  if (auto* window_manager = MultiUserWindowManagerImpl::Get()) {
    const AccountId& window_owner = window_manager->GetWindowOwner(window);
    const AccountId& active_owner =
        Shell::Get()->session_controller()->GetActiveAccountId();
    if (window_owner.is_valid() && active_owner != window_owner) {
      return false;
    }
  }
  return true;
}

// Gets the data of the tabs opening on the active desk. Unordered set is used
// because we need to dedup identical tabs, but we don't need to sort them.
std::unordered_set<coral::mojom::TabPtr> GetInSessionTabData() {
  // TODO(yulunwu, zxdan) add more tab metadata, app data,
  // and handle in-session use cases.
  std::unordered_set<coral::mojom::TabPtr> tab_data;
  for (const std::unique_ptr<TabClusterUIItem>& tab :
       Shell::Get()->tab_cluster_ui_controller()->tab_items()) {
    aura::Window* browser_window = tab->current_info().browser_window;

    // Filter out the browser window which is not on the active desk.
    if (!desks_util::BelongsToActiveDesk(browser_window)) {
      continue;
    }

    // Filter out non-browser tab info.
    if (!IsBrowserWindow(browser_window)) {
      continue;
    }

    // Filter out invalid window.
    if (!IsValidInSessionWindow(browser_window)) {
      continue;
    }
    auto tab_mojom = coral::mojom::Tab::New();
    tab_mojom->title = tab->current_info().title;
    tab_mojom->url = GURL(tab->current_info().source);
    tab_data.insert(std::move(tab_mojom));
  }

  return tab_data;
}

// Gets the data of the apps opening on the active desk. Unordered set is used
// because we need to dedup identical apps, but we don't need to sort them.
std::unordered_set<coral::mojom::AppPtr> GetInSessionAppData() {
  std::unordered_set<coral::mojom::AppPtr> app_data;

  auto* const shell = Shell::Get();
  auto mru_windows =
      shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (aura::Window* window : mru_windows) {
    // Skip transient windows.
    if (wm::GetTransientParent(window)) {
      continue;
    }

    // Skip browser windows.
    if (IsBrowserWindow(window)) {
      continue;
    }

    // Skip invalid windows.
    if (!IsValidInSessionWindow(window)) {
      continue;
    }

    // Skip windows that do not associate with a full restore app id.
    const std::string app_id = saved_desk_util::GetAppId(window);
    if (app_id.empty()) {
      continue;
    }

    const std::string* app_id_key = window->GetProperty(kAppIDKey);
    auto app_mojom = coral::mojom::App::New();
    app_mojom->title =
        (!app_id_key || IsArcWindow(window))
            ? base::UTF16ToUTF8(window->GetTitle())
            : shell->saved_desk_delegate()->GetAppShortName(*app_id_key);
    app_mojom->id = std::move(app_id);
    app_data.insert(std::move(app_mojom));
  }
  return app_data;
}

}  // namespace

BirchCoralProvider::BirchCoralProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {
  if (features::IsTabClusterUIEnabled()) {
    Shell::Get()->tab_cluster_ui_controller()->AddObserver(this);
  }
}

BirchCoralProvider::~BirchCoralProvider() {
  if (features::IsTabClusterUIEnabled()) {
    Shell::Get()->tab_cluster_ui_controller()->RemoveObserver(this);
  }
}

void BirchCoralProvider::OnTabItemAdded(TabClusterUIItem* tab_item) {
  // TODO(yulunwu) stream tab item metadata to backend for async embedding
}

void BirchCoralProvider::OnTabItemUpdated(TabClusterUIItem* tab_item) {
  // TODO(yulunwu) stream tab item metadata to backend for async embedding
}

void BirchCoralProvider::OnTabItemRemoved(TabClusterUIItem* tab_item) {}

void BirchCoralProvider::RequestBirchDataFetch() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchFakeCoral)) {
    // TODO(owenzhang): Remove placeholder page_urls.
    std::vector<GURL> page_urls;
    page_urls.emplace_back(("https://www.reddit.com/"));
    page_urls.emplace_back(("https://www.figma.com/"));
    page_urls.emplace_back(("https://www.notion.so/"));

    std::vector<std::string> app_ids;
    app_ids.emplace_back("lgnggepjiihbfdbedefdhcffnmhcahbm");

    Shell::Get()->birch_model()->SetCoralItems(
        {BirchCoralItem(u"CoralTitle", u"CoralText", page_urls, app_ids)});
    return;
  }

  // TODO(yulunwu) make appropriate data request, send data to backend.
  if (HasValidPostLoginData()) {
    HandlePostLoginDataRequest();
  } else {
    HandleInSessionDataRequest();
  }
}

bool BirchCoralProvider::HasValidPostLoginData() const {
  // TODO(sammiequon) add check for valid post login data.
  return false;
}

void BirchCoralProvider::HandlePostLoginDataRequest() {
  // TODO(sammiequon) handle post-login use case.
}

void BirchCoralProvider::HandleInSessionDataRequest() {
  // TODO(yulunwu, zxdan) add more tab metadata, app data,
  // and handle in-session use cases.
  std::vector<CoralRequest::ContentItem> active_tab_app_data;
  std::unordered_set<coral::mojom::TabPtr> tabs = GetInSessionTabData();
  while (!tabs.empty()) {
    auto tab = std::move(tabs.extract(tabs.begin()).value());
    active_tab_app_data.push_back(coral::mojom::Entity::NewTab(std::move(tab)));
  }

  std::unordered_set<coral::mojom::AppPtr> apps = GetInSessionAppData();
  while (!apps.empty()) {
    auto app = std::move(apps.extract(apps.begin()).value());
    active_tab_app_data.push_back(coral::mojom::Entity::NewApp(std::move(app)));
  }

  request_.set_content(std::move(active_tab_app_data));
  Shell::Get()->coral_controller()->GenerateContentGroups(
      request_, base::BindOnce(&BirchCoralProvider::HandleCoralResponse,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BirchCoralProvider::HandleCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  if (!response) {
    return;
  }
  // TODO(yulunwu) update `birch_model_`
  response_ = std::move(response);
  CHECK(HasValidClusterCount(response_->groups().size()));
  std::vector<BirchCoralItem> items;
  // TODO(owenzhang): Remove placeholder page_urls.
  std::vector<GURL> page_urls;
  page_urls.emplace_back(("https://chromeunboxed.com/"));
  page_urls.emplace_back(("https://www.unrealengine.com/"));
  page_urls.emplace_back(("https://godotengine.org/"));

  std::vector<std::string> app_ids;
  app_ids.emplace_back("lgnggepjiihbfdbedefdhcffnmhcahbm");
  app_ids.emplace_back("lgnggepjiihbfdbedefdhcffnmhcahbm");
  app_ids.emplace_back("lgnggepjiihbfdbedefdhcffnmhcahbm");

  for (const auto& group : response_->groups()) {
    items.emplace_back(base::UTF8ToUTF16(group->title),
                       /*subtitle=*/std::u16string(), page_urls, app_ids);
  }
  Shell::Get()->birch_model()->SetCoralItems(items);
}

}  // namespace ash
