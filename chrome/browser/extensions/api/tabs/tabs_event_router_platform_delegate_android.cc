// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_android.h"

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

// NOTE: This entire file is throwaway code. It is being used to bootstrap tests
// on the desktop Android platform. It will eventually be replaced by a cross
// platform implementation in tabs_event_router.h/cc.

namespace extensions {

TabsEventRouterPlatformDelegate::TabsEventRouterPlatformDelegate(
    TabsEventRouter& router,
    Profile& profile)
    : router_(router), profile_(profile) {
  TabModelList::AddObserver(this);
  for (TabModel* const model : TabModelList::models()) {
    OnTabModelAdded(model);
  }
}

TabsEventRouterPlatformDelegate::~TabsEventRouterPlatformDelegate() {
  TabModelList::RemoveObserver(this);
}

void TabsEventRouterPlatformDelegate::OnTabModelAdded(TabModel* tab_model) {
  // Ignore non-standard tab models which have tabs that cannot load and don't
  // have WebContents. Also ignore empty regular tab models for ephemeral or
  // incognito CCTs.
  if (tab_model->GetTabModelType() != TabModel::TabModelType::kStandard ||
      tab_model->IsEmptyRegularModelForEphemeralOrIncognitoCct()) {
    return;
  }
  if (profile_->IsSameOrParent(tab_model->GetProfile())) {
    router_->TrackTabList(*tab_model);
  }
}

void TabsEventRouterPlatformDelegate::OnTabModelRemoved(TabModel* tab_model) {
  // No need to do anything here. The TabsEventRouter handles untracking the
  // tab models when they go away.
}

}  // namespace extensions
