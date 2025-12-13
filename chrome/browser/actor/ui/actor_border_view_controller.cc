// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_border_view_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"

DEFINE_USER_DATA(ActorBorderViewController);

ActorBorderViewController::ActorBorderViewController(
    BrowserWindowInterface* browser)
    : scoped_data_holder_(browser->GetUnownedUserDataHost(), *this) {}
ActorBorderViewController::~ActorBorderViewController() = default;

// static
ActorBorderViewController* ActorBorderViewController::From(
    BrowserWindowInterface* browser_window_interface) {
  return ui::ScopedUnownedUserData<ActorBorderViewController>::Get(
      browser_window_interface->GetUnownedUserDataHost());
}

base::CallbackListSubscription
ActorBorderViewController::AddOnActorBorderGlowUpdatedCallback(
    ActorBorderGlowUpdatedCallback callback) {
  return on_actor_border_glow_updated_callback_list_.Add(std::move(callback));
}

void ActorBorderViewController::SetGlowEnabled(tabs::TabInterface* tab,
                                               bool enabled) {
  on_actor_border_glow_updated_callback_list_.Notify(tab, enabled);
}
