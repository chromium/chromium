// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

// Controller that handles the visibility and display of the
// ActorTaskListBubble.
class ActorTaskListBubbleController {
 public:
  explicit ActorTaskListBubbleController(
      BrowserWindowInterface* browser_window);
  virtual ~ActorTaskListBubbleController();

  DECLARE_USER_DATA(ActorTaskListBubbleController);
  static ActorTaskListBubbleController* From(BrowserWindowInterface* window);

 private:
  ui::ScopedUnownedUserData<ActorTaskListBubbleController>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_H_
