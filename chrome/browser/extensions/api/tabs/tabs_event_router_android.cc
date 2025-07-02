// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router_android.h"

#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

namespace extensions {

TabsEventRouterAndroid::TabsEventRouterAndroid(Profile* profile)
    : profile_(profile) {
  CHECK(profile_);
  TabModelList::AddObserver(this);
  for (TabModel* model : TabModelList::models()) {
    OnTabModelAdded(model);
  }
}

TabsEventRouterAndroid::~TabsEventRouterAndroid() {
  TabModelList::RemoveObserver(this);
}

void TabsEventRouterAndroid::OnTabModelAdded(TabModel* tab_model) {
  if (profile_->IsSameOrParent(tab_model->GetProfile())) {
    tab_model_observations_.AddObservation(tab_model);
  }
}

void TabsEventRouterAndroid::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
}

void TabsEventRouterAndroid::WillAddTab(TabAndroid* tab,
                                        TabModel::TabLaunchType type) {
  NOTIMPLEMENTED();
}

void TabsEventRouterAndroid::TabRemoved(TabAndroid* tab) {
  NOTIMPLEMENTED();
}

}  // namespace extensions
