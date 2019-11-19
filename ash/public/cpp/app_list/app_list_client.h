// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CLIENT_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "components/sync/model/string_ordinal.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

class AppListController;

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
  // Triggers a search query.
  // |trimmed_query|: the trimmed input texts from the search text field.
  virtual void StartSearch(const base::string16& trimmed_query) = 0;
  // Opens a search result and logs to metrics when its view is clicked or
  // pressed.
  // |result_id|: the id of the search result the user wants to open.
  // |launched_from|: where the result was launched.
  // |launch_type|: how the result is represented in the UI.
  // |suggestion_index|: the position of the result as a suggestion chip in
  // the AppsGridView or the position of the result in the zero state search
  // page.
  // |launch_as_default|: True if the result is launched as the default result
  // by user pressing ENTER key.
  virtual void OpenSearchResult(const std::string& result_id,
                                int event_flags,
                                ash::AppListLaunchedFrom launched_from,
                                ash::AppListLaunchType launch_type,
                                int suggestion_index,
                                bool launch_as_default) = 0;
  // Invokes a custom action on a result with |result_id|.
  // |action_index| corresponds to the index of an action on the search result,
  // for example, installing. They are stored in SearchResult::actions_.
  virtual void InvokeSearchResultAction(const std::string& result_id,
                                        int action_index,
                                        int event_flags) = 0;
  // Returns the context menu model for the search result with |result_id|, or
  // an empty array if there is currently no menu for the result.
  using GetSearchResultContextMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetSearchResultContextMenuModelCallback callback) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Interfaces on the app list UI:
  // Invoked when the app list is shown in the display with |display_id|.
  virtual void ViewShown(int64_t display_id) = 0;
  // Invoked when the app list is closed.
  virtual void ViewClosing() = 0;
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
                            int event_flags) = 0;
  // Returns the context menu model for the item with |id|, or an empty array if
  // there is currently no menu for the item (e.g. during install).
  using GetContextMenuModelCallback =
      base::OnceCallback<void(std::unique_ptr<ui::SimpleMenuModel>)>;
  virtual void GetContextMenuModel(int profile_id,
                                   const std::string& id,
                                   GetContextMenuModelCallback callback) = 0;
  // Invoked when a folder is created in Ash (e.g. merge items into a folder).
  virtual void OnFolderCreated(
      int profile_id,
      std::unique_ptr<ash::AppListItemMetadata> folder) = 0;
  // Invoked when a folder has only one item left and so gets removed.
  virtual void OnFolderDeleted(
      int profile_id,
      std::unique_ptr<ash::AppListItemMetadata> folder) = 0;
  // Invoked when user changes a folder's name or an item's position.
  virtual void OnItemUpdated(
      int profile_id,
      std::unique_ptr<ash::AppListItemMetadata> folder) = 0;
  // Invoked when a "page break" item is added with |id| and |position|.
  virtual void OnPageBreakItemAdded(int profile_id,
                                    const std::string& id,
                                    const syncer::StringOrdinal& position) = 0;
  // Invoked when a "page break" item with |id| is deleted.
  virtual void OnPageBreakItemDeleted(int profile_id,
                                      const std::string& id) = 0;

  // Updated when item with |id| is set to |visible|. Only sent if
  // |notify_visibility_change| was set on the SearchResultMetadata.
  virtual void OnSearchResultVisibilityChanged(const std::string& id,
                                               bool visibility) = 0;

  // Acquires a NavigableContentsFactory (indirectly) from the Content Service
  // to allow the app list to display embedded web contents. Currently used only
  // for answer card search results.
  virtual void GetNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory>
          receiver) = 0;

  virtual void NotifySearchResultsForLogging(
      const base::string16& trimmed_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int position_index) = 0;

 protected:
  virtual ~AppListClient() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CLIENT_H_
