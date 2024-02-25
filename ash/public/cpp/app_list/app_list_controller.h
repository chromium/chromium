// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_

#include <optional>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "ui/aura/window.h"

namespace ash {

class SearchModel;
class AppListModel;
class AppListClient;
class AppListControllerObserver;
class AppListModel;
class SearchModel;
class QuickAppAccessModel;

// An interface implemented in Ash to handle calls from Chrome.
// These include:
// - When app list data changes in Chrome, it should notifies the UI models and
//   views in Ash to get updated. This can happen while syncing, searching, etc.
// - When Chrome needs real-time UI information from Ash. This can happen while
//   calculating recommended search results based on the app list item order.
// - When app list states in Chrome change that require UI's response. This can
//   happen while installing/uninstalling apps and the app list gets toggled.
class ASH_PUBLIC_EXPORT AppListController {
 public:
  // Gets the instance.
  static AppListController* Get();

  // Sets a client to handle calls from Ash.
  virtual void SetClient(AppListClient* client) = 0;

  // Gets the client that handles calls from Ash.
  virtual AppListClient* GetClient() = 0;

  virtual void AddObserver(AppListControllerObserver* observer) = 0;
  virtual void RemoveObserver(AppListControllerObserver* obsever) = 0;

  // Updates the app list model and search model that should be used by the
  // controller.
  // This can be used to update the models represented in the app list UI when
  // the active user profile changes in Chrome. Additionally, it can be used in
  // tests to instantiate testing models.
  // `profile_id` Identifies the profile with which models are associated - used
  // as a model identifier passed to various `AppListClient` methods.
  virtual void SetActiveModel(int profile_id,
                              AppListModel* model,
                              SearchModel* search_model,
                              QuickAppAccessModel* quick_app_access_model) = 0;

  // Clears any previously set app list or search model.
  virtual void ClearActiveModel() = 0;

  // Dismisses the app list.
  virtual void DismissAppList() = 0;

  // Shows the app list.
  virtual void ShowAppList(AppListShowSource source) = 0;

  virtual AppListShowSource LastAppListShowSource() = 0;

  // Returns the app list window or nullptr if it is not visible.
  virtual aura::Window* GetWindow() = 0;

  // Returns whether the AppList is visible on the provided display.
  // If |display_id| is null, returns whether an app list is visible on any
  // display.
  virtual bool IsVisible(const std::optional<int64_t>& display_id) = 0;

  // Returns whether the AppList is visible on any display.
  virtual bool IsVisible() = 0;

  // Updates the app list with a new temporary sorting order. When exiting the
  // temporary sorting state, `new_order` is empty.
  // `animate`: if true, show a two-stage reorder animation that consists of a
  // fade out animation and a fade in animation.
  // `update_position_closure`: if set, the callback that should be called when
  // the animation to fade out the current grid completes. The closure is set
  // iff `animate` is true.
  virtual void UpdateAppListWithNewTemporarySortOrder(
      const std::optional<AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure) = 0;

 protected:
  AppListController();
  virtual ~AppListController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONTROLLER_H_
