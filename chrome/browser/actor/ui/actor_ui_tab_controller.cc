// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "components/tabs/public/tab_interface.h"

namespace actor {
using ::tabs::TabInterface;

ActorUiTabController::ActorUiTabController(TabInterface& tab) : tab_(tab) {}
ActorUiTabController::~ActorUiTabController() = default;

}  // namespace actor
