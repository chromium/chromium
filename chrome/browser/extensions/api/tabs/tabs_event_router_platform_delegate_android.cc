// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_android.h"

#include "base/debug/dump_without_crashing.h"
#include "base/notimplemented.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"

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
  // have WebContents.
  if (tab_model->GetTabModelType() != TabModel::TabModelType::kStandard) {
    return;
  }
  if (profile_->IsSameOrParent(tab_model->GetProfile())) {
    tab_model_observations_.AddObservation(tab_model);
    router_->TrackTabList(*tab_model);
  }
}

void TabsEventRouterPlatformDelegate::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
}

void TabsEventRouterPlatformDelegate::TabRemoved(TabAndroid* tab) {
  if (!tab || !tab->web_contents()) {
    return;
  }
  int tab_id = ExtensionTabUtil::GetTabId(tab->web_contents());
  if (!SessionID::IsValidValue(tab_id)) {
    return;
  }
  // NOTE: Some tests call `TabRemoved()` without calling `DidAddTab()`, so
  // there may not be anything to erase.
  bool expect_registered = false;
  router_->UnregisterForTabNotifications(*tab->web_contents(),
                                         expect_registered);
}

}  // namespace extensions
