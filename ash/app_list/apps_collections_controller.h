// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APPS_COLLECTIONS_CONTROLLER_H_
#define ASH_APP_LIST_APPS_COLLECTIONS_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash {

// Controller responsible for the Apps Collections feature tutorial view.
class ASH_EXPORT AppsCollectionsController {
 public:
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
};

}  // namespace ash

#endif  // ASH_APP_LIST_APPS_COLLECTIONS_CONTROLLER_H_
