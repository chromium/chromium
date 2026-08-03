// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_
#define CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleScrollView);

class ActorTaskListBubbleController;
class Profile;

// Bubble that displays notifications about the user's ongoing tasks.
class ActorTaskListBubble : public views::WidgetObserver {
 public:
  using OnTaskClickedCallback = base::RepeatingCallback<void(actor::TaskId)>;
  explicit ActorTaskListBubble(
      Profile* profile,
      ActorTaskListBubbleController& controller,
      const absl::flat_hash_map<actor::TaskId, bool>& task_list,
      OnTaskClickedCallback on_row_clicked);
  ~ActorTaskListBubble() override;

  void Show(views::View* anchor_view);
  void Close();
  bool IsShowing() const;

  views::Widget* widget() { return widget_; }

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

 private:
  std::unique_ptr<views::View> CreateContentsView();

  raw_ptr<Profile> profile_;
  // Browser window scoped.
  raw_ref<ActorTaskListBubbleController> controller_;
  // From GlicActorTaskIconManager, profile scoped.
  raw_ref<const absl::flat_hash_map<actor::TaskId, bool>> task_list_;
  OnTaskClickedCallback on_row_clicked_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  raw_ptr<views::Widget> widget_ = nullptr;
  size_t num_rows_ = 0ul;
};

#endif  // CHROME_BROWSER_ACTOR_UI_TASK_LIST_BUBBLE_ACTOR_TASK_LIST_BUBBLE_H_
