// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CLIENT_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

namespace ash {

class AppListController;
class AppListNotifier;

// A client interface implemented in Chrome to handle calls from Ash.
// These include:
// - When Chrome components are needed to get involved in the user's actions on
//   app list views. This can happen while the user is searching, clicking on
//   any app list item, etc.
// - When view changes in Ash and we want to notify Chrome. This can happen
//   while app list is performing animations.
// - When a user action on views need information from Chrome to complete. This
//   can happen while populating context menu models, which depends on item data
//   in Chrome.
class ASH_PUBLIC_EXPORT AppListClient {
 public:
  // Invoked when AppListController is destroyed.
  virtual void OnAppListControllerDestroyed() = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Interfaces on searching:

  // Returns the search categories that are available for users to choose if
  // they want to have the results in the categories displayed in launcher
  // search.
  virtual std::vector<AppListSearchControlCategory> GetToggleableCategories()
      const = 0;

  // Refreshes the search zero-state suggestions and invokes `on_done` when
  // complete. The client must run `on_done` before `timeout` because this
  // method is called when the user tries to open the launcher and the UI waits
  // until `on_done` before opening it.
  virtual void StartZeroStateSearch(base::OnceClosure on_done,
                                    base::TimeDelta timeout) = 0;

  // Triggers a search query.
  // |trimmed_query|: the trimmed input texts from the search text field.
  virtual void StartSearch(const std::u16string& trimmed_query) = 0;
  // Opens a search result and logs to metrics when its view is clicked or
  // pressed.
  // `profile_id`: indicates the active profile (i.e. the profile whose app list
  // data is used by Ash side).
  // `result_id`: the id of the search result the user wants to open.
  // `launched_from`: where the result was launched.
  // `launch_type`: how the result is represented in the UI.
  // `suggestion_index`: the position of the result as a suggestion chip in
  // the AppsGridView or the position of the result in the zero state search
  // page.
  // `launch_as_default`: True if the result is launched as the default result
  // by user pressing ENTER key.
  virtual void OpenSearchResult(int profile_id,
                                const std::string& result_id,
                                int event_flags,
                                AppListLaunchedFrom launched_from,
                                AppListLaunchType launch_type,
                                int suggestion_index,
                                bool launch_as_default) = 0;
  // Invokes a custom action |action| on a result with |result_id|.
  virtual void InvokeSearchResultAction(const std::string& result_id,
                                        SearchResultActionType action) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Interfaces on the app list UI:
  // Notifies target visibility changes of the app list.
  virtual void OnAppListVisibilityWillChange(bool visible) = 0;
  // Notifies visibility changes of the app list.
  virtual void OnAppListVisibilityChanged(bool visible) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Interfaces on app list items. |profile_id| indicates the profile to which
  // app list items belong. In multi-profile mode, each profile has its own
  // app list model updater:
  // Activates (opens) the item with |id|.
  virtual void ActivateItem(int profile_id,
                            const std::string& id,
                            int event_flags,
                            ash::AppListLaunchedFrom launched_from,
                            bool is_above_the_fold) = 0;
  // Returns the context menu model for the item with |id|, or an empty array if
  // there is currently no menu for the item (e.g. during install).
  // `item_context` is where the item is being shown (e.g. apps grid or recent
  // apps).
  using GetContextMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenuModel(int profile_id,
                                   const std::string& id,
                                   AppListItemContext item_context,
                                   GetContextMenuModelCallback callback) = 0;
  // Invoked when a "quick setting" is changed.
  virtual void OnQuickSettingsChanged(
      const std::string& setting_name,
      const std::map<std::string, int>& values) = 0;
  // Updated when item with |id| is set to |visible|. Only sent if
  // |notify_visibility_change| was set on the SearchResultMetadata.
  virtual void OnSearchResultVisibilityChanged(const std::string& id,
                                               bool visibility) = 0;

  // Returns the AppListNotifier instance owned by this client. Depending on the
  // implementation, this can return nullptr.
  virtual AppListNotifier* GetNotifier() = 0;

  // Recalculate whether launcher search IPH should be shown and update
  // SearchBoxModel.
  virtual void RecalculateWouldTriggerLauncherSearchIph() = 0;

  // `feature_engagement::Tracker` needs to be initialized before this method
  // gets called. Call `WouldTriggerLauncherSearchIph` to initialize it. This
  // returns false if the tracker is not initialized yet.
  virtual std::unique_ptr<ScopedIphSession>
  CreateLauncherSearchIphSession() = 0;

  // Invoked to load an icon of the app identified by `app_id`.
  virtual void LoadIcon(int profile_id, const std::string& app_id) = 0;

  // Returns the sorting order that is saved in perf service and gets shared
  // among synced devices.
  virtual ash::AppListSortOrder GetPermanentSortingOrder() const = 0;

  // If present, indicates whether the user associated with the given
  // `account_id` is considered new across all ChromeOS devices (i,e, it is the
  // first device the user has ever logged into). A user is considered new if
  // the first app list sync in the session was the first sync ever across all
  // ChromeOS devices and sessions for the given user. As such, this value is
  // absent until the first app list sync of the session is completed. NOTE:
  // Currently only the primary user profile is supported.
  virtual std::optional<bool> IsNewUser(const AccountId& account_id) const = 0;

  // Record metrics regarding the current visibility of apps in the launcher.
  virtual void RecordAppsDefaultVisibility(
      const std::vector<std::string>& apps_above_the_fold,
      const std::vector<std::string>& apps_below_the_fold,
      bool is_apps_collections_page) = 0;

  // Whether the app list was reordered locally.
  virtual bool HasReordered() = 0;

 protected:
  virtual ~AppListClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CLIENT_H_
