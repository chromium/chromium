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
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_controller.h"
#include "ash/public/cpp/tab_cluster/tab_cluster_ui_item.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/locale_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/wm/coral/coral_controller.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "ash/wm/window_restore/informed_restore_controller.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/wm/core/window_util.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

// Implement custom hash for EntityPtr because GURL doesn't support hash.
// We can dedup by possibly_invalid_spec() as it's how we transform GURL
// back to strings.
namespace std {
template <>
struct hash<coral::mojom::EntityPtr> {
  inline size_t operator()(const coral::mojom::EntityPtr& entity) const {
    if (entity->is_app()) {
      return std::hash<coral::mojom::AppPtr>{}(entity->get_app());
    }

    const coral::mojom::TabPtr& tab = entity->get_tab();
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

// The minimum number of entities in a group that allows user to remove an
// entity.
constexpr size_t kMinGroupSizeToRemove = 3;

bool HasValidClusterCount(size_t num_clusters) {
  return num_clusters <= kMaxClusterCount;
}

bool IsBrowserWindow(aura::Window* window) {
  return window->GetProperty(chromeos::kAppTypeKey) ==
         chromeos::AppType::BROWSER;
}

bool IsWebAppWindow(aura::Window* window) {
  const chromeos::AppType app_type = window->GetProperty(chromeos::kAppTypeKey);
  return app_type == chromeos::AppType::CHROME_APP ||
         app_type == chromeos::AppType::SYSTEM_APP;
}

bool IsNonWebAppWindow(aura::Window* window) {
  return !IsBrowserWindow(window) && !IsWebAppWindow(window);
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
         tab->current_info().source != tab->old_info().source ||
         tab->current_info().is_loading != tab->old_info().is_loading;
}

// Creates an AppPtr from given `window` with app title and app ID.
coral::mojom::AppPtr GetBasicAppInfoFromWindow(aura::Window* window) {
  CHECK(IsValidApp(window));

  const std::string* app_id_key = window->GetProperty(kAppIDKey);
  auto app_mojom = coral::mojom::App::New();
  app_mojom->title =
      IsArcWindow(window)
          ? base::UTF16ToUTF8(window->GetTitle())
          : Shell::Get()->saved_desk_delegate()->GetAppShortName(*app_id_key);
  app_mojom->id = std::move(*app_id_key);
  return app_mojom;
}

// Gets the data of the tabs, PWAs, and SWAs opened on the active desk.
void GetInSessionTabAndWebAppData(
    std::vector<coral::mojom::EntityPtr>& entities) {
  const TabClusterUIController* tab_cluster_ui_controller =
      Shell::Get()->tab_cluster_ui_controller();
  if (!tab_cluster_ui_controller) {
    return;
  }

  for (const std::unique_ptr<TabClusterUIItem>& tab :
       tab_cluster_ui_controller->tab_items()) {
    const TabClusterUIItem::Info& item_info = tab->current_info();
    if (IsValidTab(tab.get())) {
      auto tab_entity = coral::mojom::Entity::NewTab(coral::mojom::Tab::New(
          /*title=*/item_info.title, /*url=*/GURL(item_info.source)));
      entities.push_back(std::move(tab_entity));
    } else if (IsValidApp(item_info.browser_window) &&
               IsWebAppWindow(item_info.browser_window)) {
      coral::mojom::AppPtr app_mojom =
          GetBasicAppInfoFromWindow(item_info.browser_window);
      // Use the tab title as the app title for web apps, since they are more
      // descriptive.
      app_mojom->title = item_info.title;
      entities.push_back(coral::mojom::Entity::NewApp(std::move(app_mojom)));
    }
  }
}

// Gets the data of the non-web apps opened on the active desk.
void GetInSessionNonWebAppData(std::vector<coral::mojom::EntityPtr>& entities) {
  for (aura::Window* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!IsValidApp(window) || !IsNonWebAppWindow(window)) {
      continue;
    }

    entities.push_back(
        coral::mojom::Entity::NewApp(GetBasicAppInfoFromWindow(window)));
  }
}

// Returns the pref service to use for coral policy prefs.
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetPrimaryUserPrefService();
}

// Checks if the given `language` is supported by Coral.
bool IsLanguageSupported(std::string_view language) {
  static constexpr auto kSupportedLanguages =
      base::MakeFixedFlatSet<std::string_view>({"en", "ja", "fr", "de", "da",
                                                "es", "fi", "it", "nl", "no",
                                                "pt", "sv"});
  if (!base::FeatureList::IsEnabled(
          ash::features::kCoralFeatureMultiLanguage)) {
    return language == "en";
  }
  return base::Contains(kSupportedLanguages, language);
}

// Gets the total number of entities corresponding to the given `identifier`
// from the `response`.
int GetNumOfEntities(std::string_view identifier,
                     const CoralResponse* response) {
  int entity_num = 0;
  for (const auto& group : response->groups()) {
    entity_num += std::ranges::count_if(
        group->entities, [&](const coral::mojom::EntityPtr& entity) {
          return coral_util::GetIdentifier(entity) == identifier;
        });
  }

  return entity_num;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BirchCoralProvider::Observer:
BirchCoralProvider::Observer::Observer() {
  if (auto* coral_provider = BirchCoralProvider::Get()) {
    coral_provider->AddObserver(this);
  }
}

BirchCoralProvider::Observer::~Observer() {
  if (auto* coral_provider = BirchCoralProvider::Get()) {
    coral_provider->RemoveObserver(this);
  }
  CHECK(!IsInObserverList());
}

void BirchCoralProvider::Observer::OnCoralGroupRemoved(
    const base::Token& group_id) {}

void BirchCoralProvider::Observer::OnCoralEntityRemoved(
    const base::Token& group_id,
    std::string_view identifier) {}

void BirchCoralProvider::Observer::OnCoralGroupTitleUpdated(
    const base::Token& group_id,
    const std::string& title) {}

////////////////////////////////////////////////////////////////////////////////
// BirchCoralProvider:
BirchCoralProvider::BirchCoralProvider() {
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

// static
void BirchCoralProvider::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCoralGenAIAgeAllowed, false);
}

const coral::mojom::GroupPtr& BirchCoralProvider::GetGroupById(
    const base::Token& group_id) const {
  // Add crash keys here to track the crash of crbug.com/395130742.
  SCOPED_CRASH_KEY_BOOL("395130742", "response_", !!response_);
  if (response_) {
    SCOPED_CRASH_KEY_NUMBER("395130742", "group num",
                            response_->groups().size());
    if (!response_->groups().empty()) {
      SCOPED_CRASH_KEY_BOOL("395130742", "first group",
                            !!(*response_->groups().begin()));
    }
  }

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
  // Clear the `in_session_source_desk_` when there is no groups to avoid
  // dangling ptr and reset the window observer.
  if (groups.empty()) {
    in_session_source_desk_ = nullptr;
    windows_observation_.RemoveAllObservations();
  }
  observers_.Notify(&Observer::OnCoralGroupRemoved, group->id);
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

  // The group should not be modified when there are less than
  // `kMinGroupSizeToRemove` entities.
  CHECK_GE(group->entities.size(), kMinGroupSizeToRemove);
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

void BirchCoralProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BirchCoralProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool BirchCoralProvider::IsCoralServiceAvailable() {
  return !Shell::Get()->session_controller()->IsUserPublicAccount() &&
         coral_util::IsCoralAllowedByPolicy(GetPrefService()) &&
         GetAndCheckLanguageAvailability() && GetGenAIAvailability();
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

  if (!IsCoralServiceAvailable()) {
    HandleCoralResponse(nullptr);
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

  OnTabRemovedFromSourceDesk(tab_item);
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
      observers_.Notify(&Observer::OnCoralGroupTitleUpdated, group->id, title);
      return;
    }
  }
}

void BirchCoralProvider::OnWindowDestroyed(aura::Window* window) {
  if (!IsBrowserWindow(window)) {
    OnAppWindowRemovedFromSourceDesk(window);
  }

  // Note, we should remove the window from observing list after modifying the
  // response.
  windows_observation_.RemoveObservation(window);
}

void BirchCoralProvider::OnWindowParentChanged(aura::Window* window,
                                               aura::Window* parent) {
  // Reset the observations when `response_` or `in_session_source_desk_` are
  // null. This can occur when launching the last group which resets the
  // `in_session_source_desk_`.
  // TODO(crbug.com/383770356): Still need to find out the reason why the window
  // is still being observed when the `response_` has been reset.
  if (!response_ || !in_session_source_desk_) {
    Reset();
    return;
  }

  // If an observed window is moved to another desk, remove the associated
  // entities from the `response_`. When parent is null, the window may be in
  // the middle of changing parent.
  if (!parent || desks_util::BelongsToDesk(window, in_session_source_desk_)) {
    return;
  }

  if (IsBrowserWindow(window)) {
    // Removes the entities corresponding to the tabs in the moved browser
    // window from `response_`.
    for (const auto& tab_item :
         Shell::Get()->tab_cluster_ui_controller()->tab_items()) {
      if (tab_item->current_info().browser_window == window) {
        OnTabRemovedFromSourceDesk(tab_item.get());
      }
    }
  } else {
    OnAppWindowRemovedFromSourceDesk(window);
  }

  // Note, we should remove the window from observing list after modifying the
  // response.
  windows_observation_.RemoveObservation(window);
}

void BirchCoralProvider::OnOverviewModeEnded() {
  // Clear the in-session `response_` and reset the in-session source desk and
  // the app windows observation.
  if (response_ && response_->source() == CoralSource::kInSession) {
    Reset();
  }
}

void BirchCoralProvider::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Clear stale items on login.
  if (state == session_manager::SessionState::ACTIVE) {
    Reset();
    is_gen_ai_age_availability_checked_ = false;
    is_gen_ai_location_allow_.reset();
    system_language_.reset();
  }
}

void BirchCoralProvider::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  Reset();
  is_gen_ai_age_availability_checked_ = false;
  is_gen_ai_location_allow_.reset();
  system_language_.reset();
}

void BirchCoralProvider::OverrideCoralResponseForTest(
    std::unique_ptr<CoralResponse> response) {
  fake_response_ = std::move(response);
}

bool BirchCoralProvider::GetGenAIAvailability() {
  // Return true, if using a fake backend or group.
  auto* current_process = base::CommandLine::ForCurrentProcess();
  if (current_process->HasSwitch(switches::kForceBirchFakeCoralBackend) ||
      current_process->HasSwitch(switches::kForceBirchFakeCoralGroup)) {
    return true;
  }

  auto* coral_delegate = Shell::Get()->coral_delegate();
  if (!is_gen_ai_location_allow_.has_value()) {
    is_gen_ai_location_allow_ = coral_delegate->GetGenAILocationAvailability();
    if (!(*is_gen_ai_location_allow_)) {
      VLOG(1) << "Coral: location is restricted by GenAI";
    }
  }

  if (!(*is_gen_ai_location_allow_)) {
    return false;
  }

  // If age availability is not checked and the checking result will be returned
  // asynchronously, use the pref value.
  if (!is_gen_ai_age_availability_checked_) {
    coral_delegate->CheckGenAIAgeAvailability(
        base::BindOnce(&BirchCoralProvider::OnGenAIAgeAvailabilityReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  return (*is_gen_ai_location_allow_) &&
         GetPrefService()->GetBoolean(prefs::kCoralGenAIAgeAllowed);
}

bool BirchCoralProvider::GetAndCheckLanguageAvailability() {
  // Use "en" as system language for test.
  auto* current_process = base::CommandLine::ForCurrentProcess();
  if (current_process->HasSwitch(switches::kForceBirchFakeCoralBackend) ||
      current_process->HasSwitch(switches::kForceBirchFakeCoralGroup)) {
    system_language_ = "en";
    return true;
  }

  if (!system_language_.has_value()) {
    system_language_ = Shell::Get()->coral_delegate()->GetSystemLanguage();
    // Only output log on first checking.
    if (!IsLanguageSupported(*system_language_)) {
      VLOG(1) << "Current language is not supported by Coral.";
      return false;
    }
  }
  return IsLanguageSupported(*system_language_);
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
          coral::mojom::Tab::New(tab_info.title, tab_info.virtual_url)));
    }
  }

  FilterCoralContentItems(&tab_app_data, CoralSource::kPostLogin);
  request_.set_source(CoralSource::kPostLogin);
  request_.set_content(std::move(tab_app_data));
  CHECK(system_language_.has_value());
  request_.set_language(*system_language_);
  Shell::Get()->coral_controller()->GenerateContentGroups(
      request_, BindRemote(),
      base::BindOnce(&BirchCoralProvider::HandlePostLoginCoralResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BirchCoralProvider::HandleInSessionDataRequest() {
  // TODO(zxdan) add more tab metadata, app data,
  // and handle in-session use cases.
  std::vector<CoralRequest::ContentItem> active_tab_app_data;
  GetInSessionTabAndWebAppData(active_tab_app_data);
  GetInSessionNonWebAppData(active_tab_app_data);
  FilterCoralContentItems(&active_tab_app_data, CoralSource::kInSession);
  request_.set_source(CoralSource::kInSession);
  request_.set_content(std::move(active_tab_app_data));
  request_.set_suppression_context(
      mojo::Clone(DesksController::Get()->active_desk()->tab_app_entities()));
  if (!system_language_.has_value()) {
    GetAndCheckLanguageAvailability();
  }
  request_.set_language(*system_language_);
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
  if (!response_) {
    windows_observation_.RemoveAllObservations();
    Shell::Get()->birch_model()->SetCoralItems(items);
    return;
  }

  CHECK(HasValidClusterCount(response_->groups().size()));
  for (size_t i = 0; i < response_->groups().size(); ++i) {
    const auto& group = response_->groups()[i];
    // Set a placeholder to item title. The chip title will be directly fetched
    // from group title.
    int subtitle_id;
    switch (response_->source()) {
      case CoralSource::kPostLogin:
        subtitle_id = IDS_ASH_BIRCH_CORAL_RESTORE_CHIP_SUBTITLE;
        break;
      case CoralSource::kInSession:
        subtitle_id = IDS_ASH_BIRCH_CORAL_IN_SESSION_CHIP_SUBTITLE;
        break;
      case CoralSource::kUnknown:
        NOTREACHED() << "Unknown response type.";
    }

    // If the group title is null/empty, we use a placeholder title.
    bool is_non_empty = group->title.has_value() && !group->title->empty();
    items.emplace_back(
        is_non_empty
            ? base::UTF8ToUTF16(*group->title)
            : l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CORAL_SUGGESTION_NAME),
        l10n_util::GetStringUTF16(subtitle_id), response_->source(), group->id);
  }
  Shell::Get()->birch_model()->SetCoralItems(items);

  if (response_->source() == CoralSource::kInSession) {
    in_session_source_desk_ = DesksController::Get()->active_desk();
  }

  ObserveAllWindowsInResponse();
}

void BirchCoralProvider::FilterCoralContentItems(
    std::vector<coral::mojom::EntityPtr>* items,
    CoralSource source) {
  CHECK(coral_item_remover_);
  coral_item_remover_->FilterRemovedItems(items);

  // Remove the items with an empty title.
  auto removed = std::ranges::remove_if(
      *items, [source](const coral::mojom::EntityPtr& entity) {
        if (entity->is_tab() && entity->get_tab()->title.empty()) {
          VLOG(1) << "An empty titled tab with url: "
                  << entity->get_tab()->url.possibly_invalid_spec();
          base::UmaHistogramEnumeration("Ash.Birch.Coral.TabInfoWithEmptyTitle",
                                        source);
          return true;
        }
        if (entity->is_app() && entity->get_app()->title.empty()) {
          VLOG(1) << "An empty titled app with id: " << entity->get_app()->id;
          base::UmaHistogramEnumeration("Ash.Birch.Coral.AppInfoWithEmptyTitle",
                                        source);
          return true;
        }
        return false;
      });

  items->erase(removed.begin(), removed.end());
}

void BirchCoralProvider::MaybeCacheTabEmbedding(TabClusterUIItem* tab_item) {
  // Only cache tab embeddings for the primary user.
  auto* session_controller = Shell::Get()->session_controller();
  if (session_controller->IsUserPrimary() &&
      session_controller->GetPrimaryUserPrefService() &&
      session_controller->GetPrimaryUserPrefService()->GetBoolean(
          prefs::kBirchUseCoral) &&
      IsCoralServiceAvailable() && IsValidTab(tab_item) &&
      ShouldCreateEmbedding(tab_item)) {
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
  if (!system_language_.has_value()) {
    GetAndCheckLanguageAvailability();
  }
  request.set_language(*system_language_);
  Shell::Get()->coral_controller()->CacheEmbeddings(std::move(request));
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
  std::ranges::for_each(
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
  std::ranges::for_each(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
      [&](const auto& window) {
        if (IsValidApp(window) &&
            app_ids.contains(*(window->GetProperty(kAppIDKey)))) {
          windows_observation_.AddObservation(window);
        }
      });
}

void BirchCoralProvider::OnTabRemovedFromSourceDesk(
    TabClusterUIItem* tab_item) {
  const TabClusterUIItem::Info& removed_tab_info = tab_item->current_info();
  const std::string url = removed_tab_info.source;
  const std::string title = removed_tab_info.title;

  // If the count of currently opened tabs sharing the same to be removed tab's
  // URL equals to the count of corresponding tab entities in the groups, remove
  // a matching entity from the groups.
  const int tab_num = std::ranges::count_if(
      Shell::Get()->tab_cluster_ui_controller()->tab_items(),
      [&](const auto& tab) {
        const TabClusterUIItem::Info& info = tab->current_info();
        return windows_observation_.IsObservingSource(info.browser_window) &&
               info.source == url;
      });
  if (tab_num == GetNumOfEntities(url, response_.get())) {
    RemoveEntity(url);
  }
}

void BirchCoralProvider::OnAppWindowRemovedFromSourceDesk(
    aura::Window* app_window) {
  CHECK(!IsBrowserWindow(app_window));

  // If the count of currently opened apps sharing the same to be removed app's
  // ID equals to the count of corresponding app entities in the groups, remove
  // a matching entity from the groups.
  const std::string app_id = *(app_window->GetProperty(kAppIDKey));
  const int app_num = std::ranges::count_if(
      windows_observation_.sources(), [&app_id](const auto& window) {
        return *(window->GetProperty(kAppIDKey)) == app_id;
      });

  if (app_num == GetNumOfEntities(app_id, response_.get())) {
    RemoveEntity(app_id);
  }
}

void BirchCoralProvider::RemoveEntity(std::string_view entity_identifier) {
  CHECK(response_);
  CHECK_EQ(response_->source(), CoralSource::kInSession);

  auto& groups = response_->groups();
  for (auto group_iter = groups.begin(); group_iter != groups.end();
       group_iter++) {
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
        const base::Token group_id = group->id;
        group_iter = groups.erase(group_iter);
        // Clear the `in_session_source_desk_` when there is no groups since the
        // source desk may be in the process of removal.
        if (groups.empty()) {
          in_session_source_desk_ = nullptr;
        }
        observers_.Notify(&Observer::OnCoralGroupRemoved, group_id);
        return;
      }

      observers_.Notify(&Observer::OnCoralEntityRemoved, group->id,
                        entity_identifier);
      return;
    }
  }
}

void BirchCoralProvider::Reset() {
  // Clear the groups in observers before resetting the `response_`.
  if (response_) {
    for (const auto& group : response_->groups()) {
      observers_.Notify(&Observer::OnCoralGroupRemoved, group->id);
    }
    response_.reset();
  }
  in_session_source_desk_ = nullptr;
  windows_observation_.RemoveAllObservations();
}

void BirchCoralProvider::OnGenAIAgeAvailabilityReceived(bool allow) {
  if (!allow) {
    VLOG(1) << "Coral: age is restricted by GenAI";
  }
  is_gen_ai_age_availability_checked_ = true;
  GetPrefService()->SetBoolean(prefs::kCoralGenAIAgeAllowed, allow);
}

}  // namespace ash
