// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"

DEFINE_USER_DATA(ActorTaskListBubbleController);

ActorTaskListBubbleController::ActorTaskListBubbleController(
    BrowserWindowInterface* browser_window)
    : scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {}

ActorTaskListBubbleController::~ActorTaskListBubbleController() = default;

// static
ActorTaskListBubbleController* ActorTaskListBubbleController::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}
