// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_DELEGATE_H_

// Interface through which ActorTaskListBubbleController communicates with its
// UI.
class ActorTaskListBubbleControllerDelegate {
 public:
  virtual ~ActorTaskListBubbleControllerDelegate() = default;

  // Show the task list bubble anchored to the button.
  virtual void ShowActorTaskListBubble() = 0;

  // Hide the task list bubble.
  virtual void CloseActorTaskListBubble() = 0;

  // Returns true if the task list bubble is showing.
  virtual bool IsActorTaskListBubbleShowing() = 0;
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_CONTROLLER_DELEGATE_H_
