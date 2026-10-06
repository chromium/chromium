// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_surface_tab_helper.h"

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/profiles/profile.h"

namespace actor {

DEFINE_USER_DATA(ActorSurfaceTabHelper);

ActorSurfaceTabHelper::ActorSurfaceTabHelper(tabs::TabInterface& tab)
    : tab_(tab.GetHandle()),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  if (auto* service = ActorKeyedService::Get(tab.GetProfile())) {
    registry_ = service->GetSurfaceRegistry().GetWeakPtr();
    registry_->OnTabCreated(tab);
  }
}

ActorSurfaceTabHelper::~ActorSurfaceTabHelper() {
  if (registry_) {
    registry_->OnTabWillBeDestroyed(tab_);
  }
}

// static
ActorSurfaceTabHelper* ActorSurfaceTabHelper::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

}  // namespace actor
