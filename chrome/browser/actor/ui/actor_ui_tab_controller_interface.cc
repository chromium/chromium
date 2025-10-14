// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"

#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor::ui {

DEFINE_USER_DATA(ActorUiTabControllerInterface);

ActorUiTabControllerInterface::ActorUiTabControllerInterface(
    tabs::TabInterface& tab)
    : scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}
ActorUiTabControllerInterface::~ActorUiTabControllerInterface() = default;

// static
ActorUiTabControllerInterface* ActorUiTabControllerInterface::From(
    tabs::TabInterface* tab) {
  if (!tab) {
    LOG(ERROR) << "Tab does not exist.";
    RecordTabControllerError(
        ActorUiTabControllerError::kRequestedForNonExistentTab);
    return nullptr;
  }

  return Get(tab->GetUnownedUserDataHost());
}

}  // namespace actor::ui
