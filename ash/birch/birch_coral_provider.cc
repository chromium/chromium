// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_provider.h"

#include <unordered_set>
#include <variant>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/coral_item_remover.h"
#include "ash/birch/coral_util.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/coral/coral_controller.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
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
constexpr base::TimeDelta kPostLoginClustersLifespan = base::Minutes(15);
// Persist second post-login cluster for 10 minutes after restoring the first
// cluster.
constexpr base::TimeDelta kPostLoginSecondClusterLifespan = base::Minutes(10);
BirchCoralProvider* g_instance = nullptr;

constexpr char16_t kTitlePlaceholder[] = u"Suggested Group";

bool HasValidClusterCount(size_t num_clusters) {
  return num_clusters <= kMaxClusterCount;
}

bool IsBrowserWindow(aura::Window* window) {
  return window->GetProperty(chromeos::kAppTypeKey) ==
         chromeos::AppType::BROWSER;
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

  // Filter out browser window whose tabs cannot move to the new desk.
  if (!coral_util::CanMoveToNewDesk(browser_window)) {
    return false;
  }

  // Filter out the unloaded tab.
  if (tab->current_info().is_loading) {
    return false;
  }

  return true;
}

// Filters out apps that should not be grouped.
bool IsValidApp(aura::Window* window) {
  // Skip transient windows.
  if (wm::GetTransientParent(window)) {
    return false;
  }

  // Skip browser windows.
  if (IsBrowserWindow(window)) {
    return false;
  }

  // Skip the window that cannot move to the new desk.
  if (!coral_util::CanMoveToNewDesk(window)) {
    return false;
  }

  // Skip the window that has no app ID.
  if (!window->GetProperty(kAppIDKey)) {
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
  // TODO(zxdan) add more tab metadata, app data,
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
    if (!IsValidApp(window)) {
      continue;
    }

    const std::string* app_id_key = window->GetProperty(kAppIDKey);
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

// Checks if we should show the response on Glanceables bar.
bool ShouldShowResponse(CoralResponse* response) {
  if (!response) {
    return false;
  }

  // If we got only one group from an in-session response whose name and content
  // are exactly same as the active desk which was created from a coral group,
  // we won't show it.
  const auto& groups = response->groups();
  if (response->source() == CoralSource::kPostLogin ||
      DesksController::Get()->active_desk()->type() != Desk::Type::kCoral ||
      groups.size() != 1) {
    return true;
  }

  // Since the non-duplicated entities in the group is a subset of the tabs and
  // apps on the active desk, we only need to check if the number of group
  // entities equals to the total number of tabs and apps on the active desk.
  Shell* shell = Shell::Get();
  const size_t tab_num = base::ranges::count_if(
      shell->tab_cluster_ui_controller()->tab_items(),
      [](const auto& tab_item) {
        aura::Window* window = tab_item->current_info().browser_window;
        return IsBrowserWindow(window) &&
               desks_util::BelongsToActiveDesk(window);
      });
  const size_t app_num = base::ranges::count_if(
      shell->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
      [](const auto& window) {
        return !wm::GetTransientParent(window) && !IsBrowserWindow(window);
      });

  return groups[0]->entities.size() != (tab_num + app_num);
}

}  // namespace

BirchCoralProvider::BirchCoralProvider(BirchModel* birch_model)
    : birch_model_(birch_model) {
  g_instance = this;
  Shell* shell = Shell::Get();
  shell->tab_cluster_ui_controller()->AddObserver(this);
  overview_observation_.Observe(shell->overview_controller());
  coral_item_remover_ = std::make_unique<CoralItemRemover>();

  // Using a default fake response when --force-birch-fake-coral-group is
  // enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchFakeCoralGroup)) {
    auto fake_group = coral::mojom::Group::New();
    fake_group->id = base::Token(7, 1);
    fake_group->entities.push_back(coral::mojom::Entity::NewTab(
        coral::mojom::Tab::New("Reddit", GURL("https://www.reddit.com/"))));
    fake_group->entities.push_back(coral::mojom::Entity::NewTab(
        coral::mojom::Tab::New("Figma", GURL("https://www.figma.com/"))));
    fake_group->entities.push_back(coral::mojom::Entity::NewTab(
        coral::mojom::Tab::New("Notion", GURL("https://www.notion.so/"))));
    // nba.com PWA.
    fake_group->entities.push_back(coral::mojom::Entity::NewApp(
        coral::mojom::App::New("NBA", "ikemcggffkeigegkomkifdbhddiognji")));
    // OS settings.
    fake_group->entities.push_back(
        coral::mojom::Entity::NewApp(coral::mojom::App::New(
            "Settings", "odknhmnlageboeamepcngndbggdpaobj")));
    // Files.
    fake_group->entities.push_back(coral::mojom::Entity::NewApp(
        coral::mojom::App::New("Files", "fkiggjmkendpmbegkagpmagjepfkpmeb")));
    // ARC playstore.
    fake_group->entities.push_back(
        coral::mojom::Entity::NewApp(coral::mojom::App::New(
            "Playstore", "cnbgggchhmkkdmeppjobngjoejnihlei")));

    std::vector<coral::mojom::GroupPtr> fake_groups;
    fake_groups.push_back(std::move(fake_group));

    auto fake_response = std::make_unique<CoralResponse>();
    fake_response->set_groups(std::move(fake_groups));
    OverrideCoralResponseForTest(std::move(fake_response));
  } else {
    shell->coral_controller()->PrepareResource();
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
    const base::Token& group_id) const {
  std::vector<coral::mojom::GroupPtr>& groups = response_->groups();
  auto iter = std::find_if(
      groups.begin(), groups.end(),
      [&group_id](const auto& group) { return group->id == group_id; });
  CHECK(iter != groups.end());
  return *iter;
}

coral::mojom::GroupPtr BirchCoralProvider::ExtractGroupById(
    const base::Token& group_id) {
  std::vector<coral::mojom::GroupPtr>& groups = response_->groups();
  auto iter = std::find_if(
      groups.begin(), groups.end(),
      [&group_id](const auto& group) { return group->id == group_id; });
  CHECK(iter != groups.end());
  auto group = std::move(*iter);
  groups.erase(iter);
  return group;
}

void BirchCoralProvider::RemoveGroup(const base::Token& group_id) {
  CHECK(coral_item_remover_);
  coral::mojom::GroupPtr group = ExtractGroupById(group_id);
  for (const coral::mojom::EntityPtr& entity : group->entities) {
    coral_item_remover_->RemoveItem(entity);
  }
}

void BirchCoralProvider::RemoveItemFromGroup(const base::Token& group_id,
                                             const std::string& identifier) {
  CHECK(coral_item_remover_);
  auto& group = GetGroupById(group_id);

  group->entities.erase(
      std::remove_if(group->entities.begin(), group->entities.end(),
                     [identifier](const coral::mojom::EntityPtr& entity) {
                       return coral_util::GetIdentifier(entity) == identifier;
                     }),
      group->entities.end());

  coral_item_remover_->RemoveItem(identifier);
}

void BirchCoralProvider::OnPostLoginClusterRestored() {
  post_login_response_expiration_timestamp_ =
      base::TimeTicks::Now() + kPostLoginSecondClusterLifespan;
}

mojo::PendingRemote<coral::mojom::TitleObserver>
BirchCoralProvider::BindRemote() {
  receiver_.reset();
  return receiver_.BindNewPipeAndPassRemote();
}

void BirchCoralProvider::RequestBirchDataFetch() {
  // Use the customized fake response if set.
  if (fake_response_) {
    auto fake_response_copy = std::make_unique<CoralResponse>();
    std::vector<coral::mojom::GroupPtr> groups;
    // Copy groups.
    for (const auto& group : fake_response_->groups()) {
      // Simulate title change so we can test skottie animation and dynamic
      // title changes.
      coral::mojom::GroupPtr new_group = group->Clone();
      if (new_group->id == base::Token(7, 1)) {
        new_group->title = std::nullopt;
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, base::BindOnce([]() {
              if (auto* coral_provider = BirchCoralProvider::Get()) {
                coral_provider->TitleUpdated(base::Token(7, 1), "Coral group");
              }
            }),
            base::Seconds(5));
      }
      groups.push_back(std::move(new_group));
    }

    switch (fake_response_->source()) {
      case CoralSource::kUnknown:
        fake_response_copy->set_source(HasValidPostLoginData()
                                           ? CoralSource::kPostLogin
                                           : CoralSource::kInSession);
        break;
      case CoralSource::kInSession:
        fake_response_copy->set_source(CoralSource::kInSession);
        break;
      case CoralSource::kPostLogin:
        fake_response_copy->set_source(CoralSource::kPostLogin);
        break;
    }
    fake_response_copy->set_groups(std::move(groups));
    HandleCoralResponse(std::move(fake_response_copy));
    return;
  }

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

void BirchCoralProvider::OnTabItemRemoved(TabClusterUIItem* tab_item) {
  // Modify in-session groups when a valid associated tab being observed by
  // window observation is removed.
  if (!response_ || response_->source() == CoralSource::kPostLogin ||
      response_->groups().empty() ||
      !windows_observation_.IsObservingSource(
          tab_item->current_info().browser_window)) {
    return;
  }

  OnTabRemovedFromActiveDesk(tab_item);
}

void BirchCoralProvider::TitleUpdated(const base::Token& id,
                                      const std::string& title) {
  // `response_` may be cleared upon exiting overview.
  if (!response_) {
    return;
  }

  for (coral::mojom::GroupPtr& group : response_->groups()) {
    if (group->id == id) {
      group->title = title;
      if (auto* bar_controller = BirchBarController::Get()) {
        bar_controller->OnCoralGroupUpdated(group->id);
      }
      return;
    }
  }
}

void BirchCoralProvider::OnWindowDestroyed(aura::Window* window) {
  if (!IsBrowserWindow(window)) {
    OnAppWindowRemovedFromActiveDesk(window);
  }

  // Note, we should remove the window from observing list after modifying the
  // response.
  windows_observation_.RemoveObservation(window);
}

void BirchCoralProvider::OnWindowParentChanged(aura::Window* window,
                                               aura::Window* parent) {
  // If an observed window is moved to another desk, remove the associated
  // entities from the `response_`. When parent is null, the window may be in
  // the middle of changing parent.
  if (!parent || desks_util::BelongsToActiveDesk(window)) {
    return;
  }

  if (IsBrowserWindow(window)) {
    // Removes the entities corresponding to the tabs in the moved browser
    // window from `response_`.
    for (const auto& tab_item :
         Shell::Get()->tab_cluster_ui_controller()->tab_items()) {
      if (tab_item->current_info().browser_window == window) {
        OnTabRemovedFromActiveDesk(tab_item.get());
      }
    }
  } else {
    OnAppWindowRemovedFromActiveDesk(window);
  }

  // Note, we should remove the window from observing list after modifying the
  // response.
  windows_observation_.RemoveObservation(window);
}

void BirchCoralProvider::OnOverviewModeEnded() {
  // Clear the in-session `response_` and reset the app windows observation.
  if (response_ && response_->source() == CoralSource::kInSession) {
    response_.reset();
    windows_observation_.RemoveAllObservations();
  }
}

void BirchCoralProvider::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Clear stale items on login.
  if (state == session_manager::SessionState::ACTIVE) {
    response_.reset();
  }
}

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
  if (response_) {
    HandleCoralResponse(std::move(response_));
    return;
  }

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

  request_.set_source(CoralSource::kPostLogin);
  request_.set_content(std::move(tab_app_data));
  Shell::Get()->coral_controller()->GenerateContentGroups(
      request_, BindRemote(),
      base::BindOnce(&BirchCoralProvider::HandlePostLoginCoralResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BirchCoralProvider::HandleInSessionDataRequest() {
  // TODO(zxdan) add more tab metadata, app data,
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
  request_.set_source(CoralSource::kInSession);
  request_.set_content(std::move(active_tab_app_data));
  Shell::Get()->coral_controller()->GenerateContentGroups(
      request_, BindRemote(),
      base::BindOnce(&BirchCoralProvider::HandleInSessionCoralResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool BirchCoralProvider::HasValidPostLoginResponse() {
  return response_ && response_->source() == CoralSource::kPostLogin &&
         response_->groups().size() > 0 &&
         !post_login_response_expiration_timestamp_.is_null() &&
         base::TimeTicks::Now() < post_login_response_expiration_timestamp_;
}

void BirchCoralProvider::HandlePostLoginCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  // If response_ is not null, it may be a previously arrived post-login or
  // in-session response. Skip handling the newly arrived response in this case.
  if (response_) {
    return;
  }

  post_login_response_expiration_timestamp_ =
      base::TimeTicks::Now() + kPostLoginClustersLifespan;
  HandleCoralResponse(std::move(response));
}

void BirchCoralProvider::HandleInSessionCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  const bool non_empty = response && response->groups().size();
  if (HasValidPostLoginResponse() && !non_empty) {
    HandleCoralResponse(std::move(response_));
    return;
  }

  // Invalid post login response if a valid in-session response is generated.
  post_login_response_expiration_timestamp_ = base::TimeTicks();
  HandleCoralResponse(std::move(response));
}

void BirchCoralProvider::HandleCoralResponse(
    std::unique_ptr<CoralResponse> response) {
  std::vector<BirchCoralItem> items;
  response_ = std::move(response);
  if (!ShouldShowResponse(response_.get())) {
    windows_observation_.RemoveAllObservations();
    Shell::Get()->birch_model()->SetCoralItems(items);
    return;
  }

  CHECK(HasValidClusterCount(response_->groups().size()));
  for (size_t i = 0; i < response_->groups().size(); ++i) {
    const auto& group = response_->groups()[i];
    // Set a placeholder to item title. The chip title will be directly fetched
    // from group title.
    // TODO(zxdan): Localize the strings.
    std::u16string subtitle;
    switch (response_->source()) {
      case CoralSource::kPostLogin:
        subtitle = u"Resume suggested group";
        break;
      case CoralSource::kInSession:
        subtitle = u"Organize in a new desk";
        break;
      case CoralSource::kUnknown:
        break;
    }
    items.emplace_back(/*title=*/kTitlePlaceholder,
                       /*subtitle=*/subtitle, response_->source(),
                       /*group_id=*/group->id);
  }
  Shell::Get()->birch_model()->SetCoralItems(items);

  ObserveAllWindowsInResponse();
}

void BirchCoralProvider::FilterCoralContentItems(
    std::vector<coral::mojom::EntityPtr>* items) {
  CHECK(coral_item_remover_);
  coral_item_remover_->FilterRemovedItems(items);
}

void BirchCoralProvider::MaybeCacheTabEmbedding(TabClusterUIItem* tab_item) {
  // Only cache tab embeddings for the primary user.
  auto* session_controller = Shell::Get()->session_controller();
  if (session_controller->IsUserPrimary() &&
      session_controller->GetPrimaryUserPrefService() &&
      session_controller->GetPrimaryUserPrefService()->GetBoolean(
          prefs::kBirchUseCoral) &&
      IsValidTab(tab_item) && ShouldCreateEmbedding(tab_item)) {
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
  // TODO(conniekxu) Add metrics.
}

void BirchCoralProvider::ObserveAllWindowsInResponse() {
  // Reset windows observation.
  windows_observation_.RemoveAllObservations();

  // Only observe the windows in in-session response.
  if (response_->source() != CoralSource::kInSession) {
    return;
  }

  // Find all urls and app ids in the response.
  base::flat_set<std::string> urls;
  base::flat_set<std::string> app_ids;
  for (const auto& group : response_->groups()) {
    for (const auto& entity : group->entities) {
      if (entity->is_tab()) {
        urls.emplace(entity->get_tab()->url.possibly_invalid_spec());
      } else if (entity->is_app()) {
        app_ids.emplace(entity->get_app()->id);
      }
    }
  }

  // Observe browser windows containing the tabs with the same urls in the
  // response.
  base::ranges::for_each(
      Shell::Get()->tab_cluster_ui_controller()->tab_items(),
      [&](const auto& tab_item) {
        if (IsValidTab(tab_item.get()) &&
            urls.contains(tab_item->current_info().source)) {
          const auto& window = tab_item->current_info().browser_window;
          if (!windows_observation_.IsObservingSource(window)) {
            windows_observation_.AddObservation(window);
          }
        }
      });

  // Observe all the apps with the app id in the response.
  base::ranges::for_each(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
      [&](const auto& window) {
        if (IsValidApp(window) &&
            app_ids.contains(*(window->GetProperty(kAppIDKey)))) {
          windows_observation_.AddObservation(window);
        }
      });
}

void BirchCoralProvider::OnTabRemovedFromActiveDesk(
    TabClusterUIItem* tab_item) {
  const std::string url = tab_item->current_info().source;

  // Don't modify the groups if there are multiple tabs with the same url to be
  // removed.
  if (base::ranges::count_if(
          Shell::Get()->tab_cluster_ui_controller()->tab_items(),
          [&](const auto& tab) {
            return windows_observation_.IsObservingSource(
                       tab->current_info().browser_window) &&
                   tab->current_info().source == url;
          }) == 1) {
    RemoveEntity(url);
  }
}

void BirchCoralProvider::OnAppWindowRemovedFromActiveDesk(
    aura::Window* app_window) {
  CHECK(!IsBrowserWindow(app_window));

  // Don't modify groups if there are multiple of the same app on the active
  // desk.
  const std::string app_id = *(app_window->GetProperty(kAppIDKey));
  if (base::ranges::count_if(
          windows_observation_.sources(), [&app_id](const auto& window) {
            return *(window->GetProperty(kAppIDKey)) == app_id;
          }) == 1) {
    RemoveEntity(app_id);
  }
}

void BirchCoralProvider::RemoveEntity(std::string_view entity_identifier) {
  CHECK(response_);
  CHECK_EQ(response_->source(), CoralSource::kInSession);

  auto& groups = response_->groups();
  for (auto group_iter = groups.begin(); group_iter != groups.end();) {
    const coral::mojom::GroupPtr& group = *group_iter;
    // Check if the entity is included in the group.
    auto entity_iter = std::find_if(
        group->entities.begin(), group->entities.end(),
        [&entity_identifier](coral::mojom::EntityPtr& entity) {
          return coral_util::GetIdentifier(entity) == entity_identifier;
        });

    // If the `entity` is included in the group, remove it. After removing, if
    // the group becomes empty, remove the group.
    if (entity_iter != group->entities.end()) {
      group->entities.erase(entity_iter);
      if (group->entities.empty()) {
        // TODO(zxdan|sammiequon): Consider making coral provider observers.
        if (auto* birch_bar_controller = BirchBarController::Get()) {
          birch_bar_controller->OnCoralGroupRemoved(group->id);
        }

        if (auto* birch_model = Shell::Get()->birch_model()) {
          birch_model->OnCoralGroupRemoved(group->id);
        }
        group_iter = groups.erase(group_iter);
        continue;
      }
      if (auto* birch_bar_controller = BirchBarController::Get()) {
        birch_bar_controller->OnCoralEntityRemoved(group->id,
                                                   entity_identifier);
      }
    }
    group_iter++;
  }
}

}  // namespace ash
