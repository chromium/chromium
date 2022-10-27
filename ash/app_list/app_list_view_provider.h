// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_PROVIDER_H_
#define ASH_APP_LIST_APP_LIST_VIEW_PROVIDER_H_

#include "ash/ash_export.h"

namespace ash {

class AppListToastContainerView;
class AppsGridView;
class ContinueSectionView;
class RecentAppsView;

// Provides access to various views. Provides an abstraction around the
// clamshell bubble launcher vs. the fullscreen tablet launcher.
class ASH_EXPORT AppListViewProvider {
 public:
  // Returns the continue section view or null.
  virtual ContinueSectionView* GetContinueSectionView() = 0;

  // Returns the recent apps view or null.
  virtual RecentAppsView* GetRecentAppsView() = 0;

  // Returns the toast container view or null.
  virtual AppListToastContainerView* GetToastContainerView() = 0;

  // Returns the apps grid view, which may be either scrollable or paged.
  virtual AppsGridView* GetAppsGridView() = 0;

 protected:
  virtual ~AppListViewProvider() = default;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_VIEW_PROVIDER_H_
