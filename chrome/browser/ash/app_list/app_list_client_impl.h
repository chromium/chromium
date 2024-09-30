// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_LIST_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_LIST_CLIENT_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/apps_collections_controller.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/types/display_constants.h"

class ProfileManager;

namespace app_list {
class AppListSurveyHandler;
class SearchController;
}  // namespace app_list

class AppListClientWithProfileTest;
class AppListNotifier;
class AppListModelUpdater;
class AppSyncUIStateWatcher;
class Profile;

class AppListClientImpl
    : public ash::AppListClient,
      public AppListControllerDelegate,
      public user_manager::UserManager::UserSessionStateObserver,
      public session_manager::SessionManagerObserver,
      public TemplateURLServiceObserver,
      public ProfileManagerObserver {
 public:
  // Indicates the launcher usage state during the session started by a new user
  // (i.e. the session completing the OOBE flow) but before any account
  // switching. These are used in histograms, do not remove/renumber entries. If
  // you're adding to this enum with the intention that it will be logged,
  // update the AppListUsageStateByNewUsers enum listing in
  // tools/metrics/histograms/enums.xml.
  enum class AppListUsageStateByNewUsers {
    // Launcher is used during the session started by a new user.
    kUsed = 0,

    // Launcher is not used before destruction. The destruction can be triggered
    // in the following scenarios: logging out all account, shutting down the
    // device and system crashes.
    kNotUsedBeforeDestruction = 1,

    // Launcher is not used before switching accounts.
    kNotUsedBeforeSwitchingAccounts = 2,

    kMaxValue = kNotUsedBeforeSwitchingAccounts,
  };

  AppListClientImpl();
  AppListClientImpl(const AppListClientImpl&) = delete;
  AppListClientImpl& operator=(const AppListClientImpl&) = delete;
  ~AppListClientImpl() override;

  static AppListClientImpl* GetInstance();

  // ash::AppListClient:
  void OnAppListControllerDestroyed() override;
  std::vector<ash::AppListSearchControlCategory> GetToggleableCategories()
      const override;
  void StartZeroStateSearch(base::OnceClosure on_done,
                            base::TimeDelta timeout) override;
  void StartSearch(const std::u16string& trimmed_query) override;
  void OpenSearchResult(int profile_id,
                        const std::string& result_id,
                        int event_flags,
                        ash::AppListLaunchedFrom launched_from,
                        ash::AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                ash::SearchResultActionType action) override;
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags,
                    ash::AppListLaunchedFrom launched_from,
                    bool is_above_the_fold) override;
  void GetContextMenuModel(int profile_id,
                           const std::string& id,
                           ash::AppListItemContext item_context,
                           GetContextMenuModelCallback callback) override;
  void OnAppListVisibilityWillChange(bool visible) override;
  void OnAppListVisibilityChanged(bool visible) override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visible) override;
  void OnQuickSettingsChanged(
      const std::string& setting_name,
      const std::map<std::string, int>& values) override;
  ash::AppListNotifier* GetNotifier() override;
  void RecalculateWouldTriggerLauncherSearchIph() override;
  std::unique_ptr<ash::ScopedIphSession> CreateLauncherSearchIphSession()
      override;
  void LoadIcon(int profile_id, const std::string& app_id) override;
  ash::AppListSortOrder GetPermanentSortingOrder() const override;
  std::optional<bool> IsNewUser(const AccountId& account_id) const override;
  void RecordAppsDefaultVisibility(
      const std::vector<std::string>& apps_above_the_fold,
      const std::vector<std::string>& apps_below_the_fold,
      bool is_apps_collections_page) override;
  bool HasReordered() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  // AppListControllerDelegate overrides:
  void DismissView() override;
  aura::Window* GetAppListWindow() override;
  int64_t GetAppListDisplayId() override;
  bool IsAppPinned(const std::string& app_id) override;
  bool IsAppOpen(const std::string& app_id) const override;
  void PinApp(const std::string& app_id) override;
  void UnpinApp(const std::string& app_id) override;
  Pinnable GetPinnable(const std::string& app_id) override;
  void CreateNewWindow(bool incognito,
                       bool should_trigger_session_restore) override;
  void OpenURL(Profile* profile,
               const GURL& url,
               ui::PageTransition transition,
               WindowOpenDisposition disposition) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // Associates this client with the current active user, called when this
  // client is accessed or active user is changed.
  void UpdateProfile();

  void ShowAppList(ash::AppListShowSource source);

  bool app_list_target_visibility() const {
    return app_list_target_visibility_;
  }
  bool app_list_visible() const { return app_list_visible_; }

  // Returns a pointer to control the app list views in ash.
  ash::AppListController* GetAppListController() const;

  AppListControllerDelegate* GetControllerDelegate();
  Profile* GetCurrentAppListProfile() const;

  app_list::SearchController* search_controller();

  void SetSearchControllerForTest(
      std::unique_ptr<app_list::SearchController> test_controller);

  AppListModelUpdater* GetModelUpdaterForTest();

  // Initializes as if a new user logged in for testing.
  void InitializeAsIfNewUserLoginForTest();

  // Recalculate the default position of apps with a modified order.
  void MaybeRecalculateAppsGridDefaultOrder();

 private:
  friend class AppListSurveyTriggerTest;
  FRIEND_TEST_ALL_PREFIXES(AppListClientWithProfileTest, CheckDataRace);

  struct StateForNewUser {
    // Indicates whether showing the app list has been recorded.
    bool showing_recorded = false;
    bool shown_in_tablet_mode = false;

    // Indicates whether any launcher action has been recorded.
    bool action_recorded = false;

    // Whether the user entered a query into the search box.
    bool started_search = false;
    // Whether the result that the user launched during their first search
    // attempt got recorder.
    bool first_search_result_recorded = false;
  };

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Overridden from TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Configures the AppList for the given |profile|.
  void SetProfile(Profile* profile);

  // Updates the speech webview and start page for the current |profile_|.
  void SetUpSearchUI();

  // Records the metrics related to showing the app list.
  void RecordViewShown(bool is_apps_collection_shown);

  // Records the browser window status + the opened search result type when
  // the result is opened from the search box.
  void RecordOpenedResultFromSearchBox(ash::SearchResultType result_type);

  // Maybe records the launcher action. Launcher actions include activating an
  // app and opening a search result from either a suggestion chip or the search
  // box. `launched_from` indicates where the launcher action comes from.
  void MaybeRecordLauncherAction(ash::AppListLaunchedFrom launched_from);

  // Maybe record an activated item's visibility. An app item visibility refers
  // to above or below the fold of the launcher (i.e. is it is visible without
  // scrolling or switching the page).
  void MaybeRecordActivatedItemVisibility(
      const std::string& id,
      ash::AppListLaunchedFrom launched_from,
      bool is_app_above_the_fold);

  // Unowned pointer to the associated profile. May change if SetProfile is
  // called.
  raw_ptr<Profile> profile_ = nullptr;

  // Unowned pointer to the model updater owned by AppListSyncableService. Will
  // change if |profile_| changes.
  raw_ptr<AppListModelUpdater> current_model_updater_ = nullptr;

  // Store the mappings between profiles and AppListModelUpdater instances.
  // In multi-profile mode, mojo callings from the Ash process to access the app
  // list items should be dealt with the correct AppListModelUpdater instance.
  // Otherwise data race may happen, like the issue 939755
  // (https://crbug.com/939755).
  // TODO: Replace the mojo interface functions provided by AppListClient with
  // callbacks.
  std::map<int, raw_ptr<AppListModelUpdater, CtnExperimental>>
      profile_model_mappings_;

  std::unique_ptr<app_list::SearchController> search_controller_;
  std::unique_ptr<AppSyncUIStateWatcher> app_sync_ui_state_watcher_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  raw_ptr<ash::AppListController> app_list_controller_ = nullptr;

  std::unique_ptr<ash::AppListNotifier> app_list_notifier_;

  // Records the app list state for the session started by a new user. It
  // gets reset when:
  // (1) the active user changes, or
  // (2) the user signs out all accounts. `AppListClientImpl` is destructed in
  // this scenario.
  std::optional<StateForNewUser> state_for_new_user_;

  // Indicates when the session of a new user becomes active. If there is no new
  // users logged in, `new_user_session_activation_time_` is null.
  std::optional<base::Time> new_user_session_activation_time_;

  bool app_list_target_visibility_ = false;
  bool app_list_visible_ = false;

  // If present, indicates whether the user associated with the primary profile
  // is considered new. A user is considered new if the first app list sync in
  // the session was the first sync ever across all ChromeOS devices and
  // sessions for the given user. As such, this value is absent until the first
  // app list sync of the session is completed.
  std::optional<bool> is_primary_profile_new_user_;

  std::unique_ptr<app_list::AppListSurveyHandler> survey_handler_;

  // The profile manager is observed in order to ensure that the AppList has the
  // necessary dependencies to identify new users.
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::WeakPtrFactory<AppListClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_LIST_CLIENT_IMPL_H_
