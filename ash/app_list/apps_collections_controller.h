// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APPS_COLLECTIONS_CONTROLLER_H_
#define ASH_APP_LIST_APPS_COLLECTIONS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class AppListClient;

// Controller responsible for the Apps Collections feature tutorial view.
class ASH_EXPORT AppsCollectionsController {
 public:
  using ReorderCallback = base::RepeatingCallback<void(AppListSortOrder)>;
  // The different ways a user can dismiss the Apps Collections. Used for
  // logging, do not change the order of this enum.
  enum class DismissReason {
    // User sorted apps in the app list.
    kSorting = 0,
    // User clicked the exit button in the tutorial view nudge.
    kExitNudge = 1,
    kMaxValue = kExitNudge,
  };
  AppsCollectionsController();
  AppsCollectionsController(const AppsCollectionsController&) = delete;
  AppsCollectionsController& operator=(const AppsCollectionsController&) =
      delete;
  ~AppsCollectionsController();

  // Returns the singleton instance owned by AppListController.
  // NOTE: Exists if and only if the Apps Collection feature is enabled.
  static AppsCollectionsController* Get();

  // Whether the AppsCollection page should be presented by default when opening
  // the bubble, instead of the Apps page.
  bool ShouldShowAppsCollection();

  // Signal that the user has dismissed the AppsCollection page.
  void SetAppsCollectionDismissed(DismissReason reason);

  // Signal that the user has reorderes the Apps page.
  void SetAppsReordered();

  void SetClient(AppListClient* client);

  // Invoked when the user attempts to sort apps from the AppsCollection page.
  void RequestAppReorder(AppListSortOrder order);

  void SetReorderCallback(ReorderCallback callback);

 private:
  // The client which facilitates communication between Ash and the browser.
  raw_ptr<AppListClient> client_;

  // A local flag that stores whether the apps collections view was dismissed
  // during this session.
  bool apps_collections_was_dissmissed_ = false;

  // A local flag that stores whether the app list was reordered during this
  // session.
  bool app_list_was_reordered_ = false;

  // A callback invoked when the nudge on this page is removed/dismissed.
  ReorderCallback reorder_callback_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APPS_COLLECTIONS_CONTROLLER_H_
