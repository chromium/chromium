// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_CONTROLLER_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_CONTROLLER_H_

#include "base/functional/callback_forward.h"

namespace ash {

class AppListFolderItem;
class AppListItemView;

// An interface used to abstract app list folder UI activation from
// AppsGridView. Tapping a folder item in the apps grid is expected to show
// folder UI for that item. Apps grid view itself does not know how to show a
// folder view - folder UI is managed by apps grid view's embedders. This
// interface lets AppsGridView request folder UI state changes without making
// assumptions about the context in which it's shown.
class AppListFolderController {
 public:
  virtual ~AppListFolderController() = default;

  // Shows a folder view for the provided app list folder item view. The folder
  // will be anchored at `folder_item_view`, and it will show the contents of
  // the associated folder item (`folder_item_view->item()`).
  // `focus_name_input` indicates whether the folder name textfield should
  // receive focus by default.
  // `hide_callback` is a callback run when the folder view gets hidden.
  virtual void ShowFolderForItemView(AppListItemView* folder_item_view,
                                     bool focus_name_input,
                                     base::OnceClosure hide_callback) = 0;

  // Shows the root level apps list. Called when the UI navigates back from the
  // folder for `folder_item_view`. If `folder_item_view` is nullptr skips
  // animation. If `folder_item_view` is non-null and `select_folder` is true,
  // the folder item is selected (e.g. for keyboard navigation).
  virtual void ShowApps(AppListItemView* folder_item_view,
                        bool select_folder) = 0;

  // Transits the UI from folder view to root level apps grid view when
  // re-parenting a child item of |folder_item|.
  virtual void ReparentFolderItemTransit(AppListFolderItem* folder_item) = 0;

  // Notifies the container that a reparent drag has completed.
  virtual void ReparentDragEnded() = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_FOLDER_CONTROLLER_H_
