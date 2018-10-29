// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
#define ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/interfaces/menu.mojom.h"
#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
class GestureEvent;
}  // namespace ui

namespace ws {
class WindowService;
}  // namespace ws

namespace app_list {

class AppListModel;
class SearchModel;

class ASH_PUBLIC_EXPORT AppListViewDelegate {
 public:
  virtual ~AppListViewDelegate() {}
  // Gets the model associated with the view delegate. The model may be owned
  // by the delegate, or owned elsewhere (e.g. a profile keyed service).
  virtual AppListModel* GetModel() = 0;

  // Gets the search model associated with the view delegate. The model may be
  // owned by the delegate, or owned elsewhere (e.g. a profile keyed service).
  virtual SearchModel* GetSearchModel() = 0;

  // Invoked to start a new Google Assistant session.
  virtual void StartAssistant() = 0;

  // Invoked to start a new search. This collects a list of search results
  // matching the raw query, which is an unhandled string typed into the search
  // box by the user.
  virtual void StartSearch(const base::string16& raw_query) = 0;

  // Invoked to open the search result.
  virtual void OpenSearchResult(const std::string& result_id,
                                int event_flags) = 0;

  // Called to invoke a custom action on a result with |result_id|.
  // |action_index| corresponds to the index of an icon in
  // |result.action_icons()|.
  virtual void InvokeSearchResultAction(const std::string& result_id,
                                        int action_index,
                                        int event_flags) = 0;

  // Returns the context menu model for a ChromeSearchResult with |result_id|,
  // or NULL if there is currently no menu for the result.
  // Note the returned menu model is owned by that result.
  using GetContextMenuModelCallback =
      base::OnceCallback<void(std::vector<ash::mojom::MenuItemPtr>)>;
  virtual void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) = 0;

  // Invoked when a context menu item of a search result is clicked.
  // |result_id| is the clicked SearchResult's id
  // |command_id| is the clicked menu item's command id
  // |event_flags| is flags from the event which triggered this command
  virtual void SearchResultContextMenuItemSelected(const std::string& result_id,
                                                   int command_id,
                                                   int event_flags) = 0;

  // Invoked when the app list is shown.
  virtual void ViewShown(int64_t display_id) = 0;

  // Invoked to dismiss app list. This may leave the view open but hidden from
  // the user.
  virtual void DismissAppList() = 0;

  // Invoked when the app list is closing.
  virtual void ViewClosing() = 0;

  // Invoked when the app list is closed.
  virtual void ViewClosed() = 0;

  // Gets the wallpaper prominent colors.
  using GetWallpaperProminentColorsCallback =
      base::OnceCallback<void(const std::vector<SkColor>&)>;
  virtual void GetWallpaperProminentColors(
      GetWallpaperProminentColorsCallback callback) = 0;

  // Activates (opens) the item.
  virtual void ActivateItem(const std::string& id, int event_flags) = 0;

  // Returns the context menu model for a ChromeAppListItem with |id|, or NULL
  // if there is currently no menu for the item (e.g. during install).
  // Note the returned menu model is owned by that item.
  virtual void GetContextMenuModel(const std::string& id,
                                   GetContextMenuModelCallback callback) = 0;

  // Invoked when a context menu item of an app list item is clicked.
  // |id| is the clicked AppListItem's id
  // |command_id| is the clicked menu item's command id
  // |event_flags| is flags from the event which triggered this command
  virtual void ContextMenuItemSelected(const std::string& id,
                                       int command_id,
                                       int event_flags) = 0;

  // Show wallpaper context menu from the specified onscreen location.
  virtual void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                        ui::MenuSourceType source_type) = 0;

  // Forwards events to the home launcher gesture handler and returns true if
  // they have been processed.
  virtual bool ProcessHomeLauncherGesture(
      ui::GestureEvent* event,
      const gfx::Point& screen_location) = 0;

  // Checks if we are allowed to process events on the app list main view and
  // its descendants.
  virtual bool CanProcessEventsOnApplistViews() = 0;

  virtual ws::WindowService* GetWindowService() = 0;
};

}  // namespace app_list

#endif  // ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_H_
