// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"

#include <unordered_set>
#include <variant>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/coral_item_remover.h"
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
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
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

constexpr size_t kMaxClusterCount = 2;
BirchCoralProvider* g_instance = nullptr;

bool HasValidClusterCount(size_t num_clusters) {
  return num_clusters <= kMaxClusterCount;
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

// Filters out tabs that should not be embedded/clustered.
bool IsValidTab(TabClusterUIItem* tab) {
  aura::Window* browser_window = tab->current_info().browser_window;

  // Filter out the browser window which is not on the active desk.
  if (!desks_util::BelongsToActiveDesk(browser_window)) {
    return false;
  }

  // Filter out non-browser tab info.
  if (!IsBrowserWindow(browser_window)) {
    return false;
  }

  // Filter out invalid window.
  if (!IsValidInSessionWindow(browser_window)) {
    return false;
  }
  return true;
}

// Checks whether `tab` has been meaningfully updated and we should generate
//  and cache a new embedding in the backend.
bool ShouldCreateEmbedding(TabClusterUIItem* tab) {
  return tab->current_info().title != tab->old_info().title ||
         tab->current_info().source != tab->old_info().source;
}

// Gets the data of the tabs opening on the active desk. Unordered set is used
// because we need to dedup identical tabs, but we don't need to sort them.
std::unordered_set<coral::mojom::TabPtr> GetInSessionTabData() {
  // TODO(yulunwu, zxdan) add more tab metadata, app data,
  // and handle in-session use cases.
  std::unordered_set<coral::mojom::TabPtr> tab_data;
  if (!Shell::Get()->tab_cluster_ui_controller()) {
    return tab_data;
  }
  for (const std::unique_ptr<TabClusterUIItem>& tab :
       Shell::Get()->tab_cluster_ui_controller()->tab_items()) {
    if (IsValidTab(tab.get())) {
      auto tab_mojom = coral::mojom::Tab::New();
      tab_mojom->title = tab->current_info().title;
      tab_mojom->url = GURL(tab->current_info().source);
      tab_data.insert(std::move(tab_mojom));
    }
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

    const std::string* app_id_key = window->GetProperty(kAppIDKey);
    if (!app_id_key) {
      continue;
    }

    auto app_mojom = coral::mojom::App::New();
    app_mojom->title =
        IsArcWindow(window)
            ? base::UTF16ToUTF8(window->GetTitle())
            : shell->saved_desk_delegate()->GetAppShortName(*app_id_key);
    app_mojom->id = std::move(*app_id_key);
    app_data.insert(std::move(app_mojom));
  }
  return app_data;
}

}  // namespace

BirchCoralProvider::BirchCoralProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {
  g_instance = this;
  Shell::Get()->tab_cluster_ui_controller()->AddObserver(this);
  coral_item_remover_ = std::make_unique<CoralItemRemover>();
}

BirchCoralProvider::~BirchCoralProvider() {
  Shell::Get()->tab_cluster_ui_controller()->RemoveObserver(this);
  g_instance = nullptr;
}

BirchCoralProvider* BirchCoralProvider::Get() {
  return g_instance;
}

void BirchCoralProvider::OnTabItemAdded(TabClusterUIItem* tab_item) {
  MaybeCacheTabEmbedding(tab_item);
}

void BirchCoralProvider::OnTabItemUpdated(TabClusterUIItem* tab_item) {
  MaybeCacheTabEmbedding(tab_item);
}

void BirchCoralProvider::OnTabItemRemoved(TabClusterUIItem* tab_item) {}

void BirchCoralProvider::RequestBirchDataFetch() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchFakeCoral)) {
    std::vector<GURL> page_urls{
        GURL("https://www.ikea.com/"), GURL("https://www.figma.com/"),
        GURL("https://www.notion.so/"), GURL("https://www.nhl.com/")};

    std::vector<std::string> app_ids = {"odknhmnlageboeamepcngndbggdpaobj",
                                        "fkiggjmkendpmbegkagpmagjepfkpmeb"};

    Shell::Get()->birch_model()->SetCoralItems({BirchCoralItem(
        u"CoralTitle", u"CoralText", page_urls, app_ids, /*cluster_id=*/0)});
    return;
  }

  // TODO(yulunwu) make appropriate data request, send data to backend.
  if (HasValidPostLoginData()) {
    HandlePostLoginDataRequest();
  } else {
    HandleInSessionDataRequest();
  }
}

void BirchCoralProvider::RemoveGroup(const int cluster_id) {
  CHECK(coral_item_remover_);
  for (const auto& entity : groups_[cluster_id]->entities) {
    coral_item_remover_->RemoveItem(entity);
  }
  groups_.erase(groups_.find(cluster_id));
}

void BirchCoralProvider::RemoveItem(const coral::mojom::EntityKeyPtr& key) {
  CHECK(coral_item_remover_);
  coral_item_remover_->RemoveItem(key);
}

void BirchCoralProvider::OverrideCoralResponseForTest(
    std::unique_ptr<CoralResponse> response) {
  HandleCoralResponse(std::move(response));
}

bool BirchCoralProvider::HasValidPostLoginData() const {
  InformedRestoreController* informed_restore_controller =
      Shell::Get()->informed_restore_controller();
  return informed_restore_controller &&
         !!informed_restore_controller->contents_data();
}

void BirchCoralProvider::HandlePostLoginDataRequest() {
  InformedRestoreContentsData* contents_data =
      Shell::Get()->informed_restore_controller()->contents_data();
  std::vector<CoralRequest::ContentItem> tab_app_data;

  for (const InformedRestoreContentsData::AppInfo& app_info :
       contents_data->apps_infos) {
    if (app_info.tab_urls.empty()) {
      tab_app_data.push_back(coral::mojom::Entity::NewApp(
          coral::mojom::App::New(app_info.title, app_info.app_id)));
      continue;
    }

    for (const GURL& url : app_info.tab_urls) {
      // TODO(http://b/365839465): The only title we have right now is the
      // active tab title.
      tab_app_data.push_back(coral::mojom::Entity::NewTab(
          coral::mojom::Tab::New(app_info.title, url)));
    }
  }

  // TODO(sammiequon): Implement item remover.
  request_.set_content(std::move(tab_app_data));
  Shell::Get()->coral_controller()->GenerateContentGroups(
      request_, base::BindOnce(&BirchCoralProvider::HandleCoralResponse,
                               weak_ptr_factory_.GetWeakPtr()));
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
  FilterCoralContentItems(&active_tab_app_data);
  request_.set_content(std::move(active_tab_app_data));
  Shell::Get()->coral_controller()->GenerateContentGroups(
      request_, base::BindOnce(&BirchCoralProvider::HandleCoralResponse,
                               weak_ptr_factory_.GetWeakPtr()));
}

void BirchCoralProvider::HandleCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  std::vector<BirchCoralItem> items;
  groups_.clear();
  if (!response) {
    LOG(ERROR) << "Failed to receive coral response.";
    response_.reset();
    Shell::Get()->birch_model()->SetCoralItems(items);
    return;
  }
  // TODO(yulunwu) update `birch_model_`
  response_ = std::move(response);
  CHECK(HasValidClusterCount(response_->groups().size()));

  for (size_t i = 0; i < response_->groups().size(); ++i) {
    groups_[i] = response_->groups()[i].Clone();
    std::vector<GURL> page_urls;
    std::vector<std::string> app_ids;
    for (const auto& entity : groups_[i]->entities) {
      if (entity->is_tab_url()) {
        page_urls.push_back(entity->get_tab_url());
      }
      if (entity->is_app_id()) {
        app_ids.push_back(entity->get_app_id());
      }
    }
    items.emplace_back(base::UTF8ToUTF16(groups_[i]->title),
                       /*subtitle=*/std::u16string(), page_urls, app_ids,
                       /*cluster_id=*/int(i));
  }
  Shell::Get()->birch_model()->SetCoralItems(items);
}

void BirchCoralProvider::FilterCoralContentItems(
    std::vector<coral::mojom::EntityPtr>* items) {
  CHECK(coral_item_remover_);
  coral_item_remover_->FilterRemovedItems(items);
}

void BirchCoralProvider::MaybeCacheTabEmbedding(TabClusterUIItem* tab_item) {
  if (IsValidTab(tab_item) && ShouldCreateEmbedding(tab_item)) {
    CacheTabEmbedding(tab_item);
  }
}

void BirchCoralProvider::CacheTabEmbedding(TabClusterUIItem* tab_item) {
  if (!Shell::Get()->coral_controller()) {
    return;
  }
  auto tab_mojom = coral::mojom::Tab::New();
  tab_mojom->title = tab_item->current_info().title;
  tab_mojom->url = GURL(tab_item->current_info().source);

  std::vector<CoralRequest::ContentItem> active_tab_app_data;
  active_tab_app_data.push_back(
      coral::mojom::Entity::NewTab(std::move(tab_mojom)));
  CoralRequest request;
  request.set_content(std::move(active_tab_app_data));
  Shell::Get()->coral_controller()->CacheEmbeddings(
      std::move(request),
      base::BindOnce(&BirchCoralProvider::HandleEmbeddingResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BirchCoralProvider::HandleEmbeddingResult(bool success) {
  // TODO(yulunwu) Add metrics.
}

}  // namespace ash
