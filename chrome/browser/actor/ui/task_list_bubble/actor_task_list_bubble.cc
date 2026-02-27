// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"

#include <memory>
#include <vector>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);

namespace {
const int kVerticalMargin = 8;

int GetPriorityForTaskState(actor::ActorTask::State task_state,
                            bool requires_processing) {
  // Tasks should be prioritized in the following order:
  // 1. Unprocessed tasks needing attention
  // 2. Processed tasks needing attention
  // 3. Remaining tasks that need processing
  // 4. All other tasks
  return tabs::GlicActorTaskIconManager::RequiresAttention(task_state)
             ? (requires_processing ? 1 : 2)
         : tabs::GlicActorTaskIconManager::RequiresTaskProcessing(task_state)
             ? 3
             : 4;
}

}  // namespace

// static
views::Widget* ActorTaskListBubble::ShowBubble(
    Profile* profile,
    views::View* anchor_view,
    const absl::flat_hash_map<actor::TaskId, bool>& task_list,
    base::RepeatingCallback<void(actor::TaskId)> on_row_clicked) {
  auto contents_view =
      CreateContentsView(profile, task_list, std::move(on_row_clicked));

  // If there are no rows, don't show the bubble.
  if (contents_view->children().empty()) {
    return nullptr;
  }

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetAccessibleTitle(
              l10n_util::GetStringUTF16(IDS_ACTOR_TASK_LIST_BUBBLE_A11Y_LABEL))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(contents_view),
                  views::BubbleDialogModelHost::FieldType::kMenuItem),
              kActorTaskListBubbleView)
          .OverrideShowCloseButton(false)
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  bubble->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble->set_margins(gfx::Insets::VH(kVerticalMargin, 0));

  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  // Bubble can always show activated as it will only show in the active window.
  widget->Show();

  return widget;
}

std::unique_ptr<views::View> ActorTaskListBubble::CreateContentsView(
    Profile* profile,
    const absl::flat_hash_map<actor::TaskId, bool>& task_list,
    base::RepeatingCallback<void(actor::TaskId)> on_row_clicked) {
  std::unique_ptr<views::View> contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();

  actor::ui::ActorUiStateManagerInterface* actor_ui_state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();

  // Keep track of tasks in each state for ordering tasks in the list bubble.
  std::vector<std::pair</*priority=*/int, actor::TaskId>> row_priority_list;

  // Loop through the list to assign priorities to each task.
  for (auto [task_id, requires_processing] : task_list) {
    auto task_state = actor_ui_state_manager->GetActorTaskState(task_id);
    if (!task_state) {
      actor::ui::RecordTaskIconError(
          actor::ui::ActorUiTaskIconError::kBubbleTaskDoesntExist);
      continue;
    }

    row_priority_list.emplace_back(
        GetPriorityForTaskState(task_state.value(), requires_processing),
        task_id);
  }

  std::sort(row_priority_list.begin(), row_priority_list.end());

  // Can now create rows in order of priority.
  for (auto [priority, task_id] : row_priority_list) {
    auto task_state = actor_ui_state_manager->GetActorTaskState(task_id);
    auto task_title = actor_ui_state_manager->GetActorTaskTitle(task_id);
    auto task_tab = actor_ui_state_manager->GetLastActedOnTab(task_id);
    bool requires_processing = task_list.at(task_id);
    CHECK(task_state.has_value() && task_title.has_value() &&
          task_tab.has_value());

    std::unique_ptr<ActorTaskListBubbleRowButton> row =
        std::make_unique<ActorTaskListBubbleRowButton>(
            base::BindRepeating(on_row_clicked, task_id), task_state.value(),
            base::UTF8ToUTF16(task_title.value()), requires_processing,
            task_tab.value() != nullptr);

    contents_view->AddChildView(std::move(row));
  }
  return contents_view;
}
