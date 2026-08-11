// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_metrics_types.h"
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

base::ScopedClosureRunner
ActorUiTabControllerInterface::RegisterActorTabIndicatorStateChangedCallback(
    ActorTabIndicatorStateChangedCallback callback) {
  // Crash if attempting to register a null callback, or if a callback is
  // already registered.
  CHECK(!callback.is_null());
  CHECK(on_actor_tab_indicator_changed_callback_.is_null());
  on_actor_tab_indicator_changed_callback_ = std::move(callback);
  return base::ScopedClosureRunner(base::BindOnce(
      &ActorUiTabControllerInterface::UnregisterActorTabIndicatorStateChange,
      weak_factory_.GetWeakPtr()));
}

void ActorUiTabControllerInterface::UnregisterActorTabIndicatorStateChange() {
  on_actor_tab_indicator_changed_callback_.Reset();
}

bool ActorUiTabControllerInterface::NotifyActorTabIndicatorStateChanged(
    TabIndicatorStatus tab_indicator_status) {
  if (on_actor_tab_indicator_changed_callback_) {
    on_actor_tab_indicator_changed_callback_.Run(tab_indicator_status);
    return true;
  }
  return false;
}

}  // namespace actor::ui
