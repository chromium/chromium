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
// Persist post-login clusters for 15 minutes.
constexpr base::TimeDelta kPostLoginClusterLifespan = base::Minutes(15);
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

  // Using a default fake response when --force-birch-fake-coral-group is
  // enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchFakeCoralGroup)) {
    auto fake_response = std::make_unique<CoralResponse>();
    // TODO(owenzhang): Remove placeholder page_urls.
    auto fake_group = coral::mojom::Group::New();
    fake_group->title = "Coral Group";
    fake_group->entities.push_back(
        coral::mojom::EntityKey::NewTabUrl(GURL("https://www.reddit.com/")));
    fake_group->entities.push_back(
        coral::mojom::EntityKey::NewTabUrl(GURL("https://www.figma.com/")));
    fake_group->entities.push_back(
        coral::mojom::EntityKey::NewTabUrl(GURL("https://www.notion.so/")));
    // OS settings.
    fake_group->entities.push_back(
        coral::mojom::EntityKey::NewAppId("odknhmnlageboeamepcngndbggdpaobj"));
    // Files.
    fake_group->entities.push_back(
        coral::mojom::EntityKey::NewAppId("fkiggjmkendpmbegkagpmagjepfkpmeb"));

    std::vector<coral::mojom::GroupPtr> fake_groups;
    fake_groups.push_back(std::move(fake_group));
    fake_response->set_groups(std::move(fake_groups));
    OverrideCoralResponseForTest(std::move(fake_response));
  }
}

BirchCoralProvider::~BirchCoralProvider() {
  Shell::Get()->tab_cluster_ui_controller()->RemoveObserver(this);
  g_instance = nullptr;
}

// static.
BirchCoralProvider* BirchCoralProvider::Get() {
  return g_instance;
}

const coral::mojom::GroupPtr& BirchCoralProvider::GetGroupById(
    int group_id) const {
  std::vector<coral::mojom::GroupPtr>& groups = response_->groups();
  CHECK_LT(group_id, static_cast<int>(groups.size()));
  return groups[group_id];
}

coral::mojom::GroupPtr BirchCoralProvider::ExtractGroupById(int group_id) {
  std::vector<coral::mojom::GroupPtr>& groups = response_->groups();
  CHECK_LT(group_id, static_cast<int>(groups.size()));
  auto group = std::move(groups[group_id]);
  groups.erase(groups.begin() + group_id);
  return group;
}

void BirchCoralProvider::RemoveGroup(int group_id) {
  CHECK(coral_item_remover_);
  coral::mojom::GroupPtr group = ExtractGroupById(group_id);
  for (const auto& entity : group->entities) {
    coral_item_remover_->RemoveItem(entity);
  }
}

void BirchCoralProvider::RemoveItem(const coral::mojom::EntityKeyPtr& key) {
  CHECK(coral_item_remover_);
  coral_item_remover_->RemoveItem(key);
}

void BirchCoralProvider::RequestBirchDataFetch() {
  // Use the customized fake response if set.
  if (fake_response_) {
    auto fake_response_copy = std::make_unique<CoralResponse>();
    std::vector<coral::mojom::GroupPtr> groups;
    // Copy groups.
    for (const auto& group : fake_response_->groups()) {
      groups.push_back(group->Clone());
    }
    fake_response_copy->set_groups(std::move(groups));
    HandleCoralResponse(std::move(fake_response_copy));
    return;
  }

  // Do not make additional requests to the backend if we have valid post login
  // clusters.
  if (HasValidPostLoginResponse()) {
    return;
  }

  // TODO(yulunwu) make appropriate data request, send data to backend.
  if (HasValidPostLoginData()) {
    HandlePostLoginDataRequest();
  } else {
    HandleInSessionDataRequest();
  }
}

void BirchCoralProvider::OnTabItemAdded(TabClusterUIItem* tab_item) {
  MaybeCacheTabEmbedding(tab_item);
}

void BirchCoralProvider::OnTabItemUpdated(TabClusterUIItem* tab_item) {
  MaybeCacheTabEmbedding(tab_item);
}

void BirchCoralProvider::OnTabItemRemoved(TabClusterUIItem* tab_item) {}

void BirchCoralProvider::OverrideCoralResponseForTest(
    std::unique_ptr<CoralResponse> response) {
  fake_response_ = std::move(response);
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
    if (app_info.tab_infos.empty()) {
      tab_app_data.push_back(coral::mojom::Entity::NewApp(
          coral::mojom::App::New(app_info.title, app_info.app_id)));
      continue;
    }

    for (const InformedRestoreContentsData::TabInfo& tab_info :
         app_info.tab_infos) {
      tab_app_data.push_back(coral::mojom::Entity::NewTab(
          coral::mojom::Tab::New(tab_info.title, tab_info.url)));
    }
  }

  request_.set_content(std::move(tab_app_data));
  Shell::Get()->coral_controller()->GenerateContentGroups(
      request_,
      base::BindOnce(&BirchCoralProvider::HandlePostLoginCoralResponse,
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
      request_,
      base::BindOnce(&BirchCoralProvider::HandleInSessionCoralResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool BirchCoralProvider::HasValidPostLoginResponse() {
  return response_ && response_->groups().size() > 0 &&
         !post_login_response_timestamp_.is_null() &&
         base::Time::Now() - post_login_response_timestamp_ <
             kPostLoginClusterLifespan;
}

void BirchCoralProvider::HandlePostLoginCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  post_login_response_timestamp_ = base::Time::Now();
  HandleCoralResponse(std::move(response_));
}

void BirchCoralProvider::HandleInSessionCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  // Do not handle in-session responses while the post-login response is still
  // valid.
  CHECK(!HasValidPostLoginResponse());
  HandleCoralResponse(std::move(response_));
}

void BirchCoralProvider::HandleCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  std::vector<BirchCoralItem> items;
  if (!response) {
    response_.reset();
    Shell::Get()->birch_model()->SetCoralItems(items);
    return;
  }

  response_ = std::move(response);
  CHECK(HasValidClusterCount(response_->groups().size()));
  for (size_t i = 0; i < response_->groups().size(); ++i) {
    items.emplace_back(base::UTF8ToUTF16(response_->groups()[i]->title),
                       /*subtitle=*/std::u16string(),
                       /*group_id=*/int(i));
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
