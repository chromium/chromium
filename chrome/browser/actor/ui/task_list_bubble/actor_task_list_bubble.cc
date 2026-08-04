// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"

#include <memory>
#include <vector>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleScrollView);

namespace {
const int kVerticalMargin = 8;
// Calculated as a max of 8 rows * 56 px per row. This is also inline with the
// extensions bubble max height (448) and the downloads bubble max height (450).
const int kMaxBubbleHeight = 448;

int GetPriorityForTaskState(actor::ActorTask::State task_state,
                            bool requires_processing,
                            glic::mojom::FeatureMode feature_mode) {
  // Tasks should be prioritized in the following order:
  // 1. Unprocessed tasks needing attention
  // 2. Processed tasks needing attention
  // 3. Remaining tasks that need processing
  // 4. All other tasks
  return glic::GlicActorTaskIconManager::RequiresAttention(task_state)
             ? (requires_processing ? 1 : 2)
         : glic::GlicActorTaskIconManager::RequiresTaskProcessing(task_state,
                                                                  feature_mode)
             ? 3
             : 4;
}

}  // namespace

ActorTaskListBubble::ActorTaskListBubble(
    Profile* profile,
    BrowserWindowInterface* browser,
    const absl::flat_hash_map<actor::TaskId, bool>& task_list,
    OnTaskClickedCallback on_row_clicked)
    : profile_(profile),
      browser_(browser),
      task_list_(task_list),
      on_row_clicked_(std::move(on_row_clicked)) {}

ActorTaskListBubble::~ActorTaskListBubble() {
  widget_observation_.Reset();
  Close();
}

void ActorTaskListBubble::Show(views::View* anchor_view) {
  auto contents_view = CreateContentsView();

  // If there are no rows, don't show the bubble.
  if (contents_view->children().empty()) {
    return;
  }

  std::unique_ptr<views::ScrollView> scroll_view =
      std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled);
  scroll_view->SetContents(std::move(contents_view));
  scroll_view->ClipHeightTo(0, kMaxBubbleHeight);
  scroll_view->SetDrawOverflowIndicator(false);

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetAccessibleTitle(
              l10n_util::GetStringUTF16(IDS_ACTOR_TASK_LIST_BUBBLE_A11Y_LABEL))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(scroll_view),
                  views::BubbleDialogModelHost::FieldType::kMenuItem),
              kActorTaskListBubbleScrollView)
          .OverrideShowCloseButton(false)
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  bubble->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble->set_margins(gfx::Insets::VH(kVerticalMargin, 0));

  widget_ = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  // Bubble can always show activated as it will only show in the active window.
  widget_->Show();
  widget_observation_.Reset();
  widget_observation_.Observe(widget_);
  actor::ui::RecordTaskListBubbleRows(num_rows_);
}

void ActorTaskListBubble::Close() {
  if (widget_) {
    widget_->Close();
    widget_ = nullptr;
  }
}

bool ActorTaskListBubble::IsShowing() const {
  return widget_ && widget_->IsVisible();
}

void ActorTaskListBubble::OnWidgetDestroyed(views::Widget* widget) {
  widget_observation_.Reset();
  widget_ = nullptr;
  if (auto* controller = ActorTaskListBubbleController::From(browser_)) {
    controller->OnBubbleDestroyed();
  }
}

// TODO(crbug.com/518584352): share the non-Views parts of this function with
// Android.
std::unique_ptr<views::View> ActorTaskListBubble::CreateContentsView() {
  std::unique_ptr<views::View> contents_view =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetProperty(views::kElementIdentifierKey, kActorTaskListBubbleView)
          .Build();

  auto* actor_service = actor::ActorKeyedService::Get(profile_);
  CHECK(actor_service);
  actor::ui::ActorUiStateManagerInterface* actor_ui_state_manager =
      actor_service->GetActorUiStateManager();

  // Keep track of tasks in each state for ordering tasks in the list bubble.
  std::vector<std::pair</*priority=*/int, actor::TaskId>> row_priority_list;

  // Loop through the list to assign priorities to each task.
  for (auto [task_id, requires_processing] : *task_list_) {
    auto task_state = actor_ui_state_manager->GetActorTaskState(task_id);
    if (!task_state) {
      actor::ui::RecordTaskIconError(
          actor::ui::ActorUiTaskIconError::kBubbleTaskDoesntExist);
      continue;
    }
    row_priority_list.emplace_back(
        GetPriorityForTaskState(
            task_state.value(), requires_processing,
            actor_ui_state_manager->GetFeatureMode(task_id)),
        task_id);
  }

  std::sort(row_priority_list.begin(), row_priority_list.end());

  // Can now create rows in order of priority.
  num_rows_ = 0ul;
  for (auto [priority, task_id] : row_priority_list) {
    auto task_state = actor_ui_state_manager->GetActorTaskState(task_id);
    auto task_title = actor_ui_state_manager->GetActorTaskTitle(task_id);
    auto task_tab = actor_ui_state_manager->GetLastActedOnTab(task_id);
    bool requires_processing = task_list_->at(task_id);
    CHECK(task_state.has_value() && task_title.has_value() &&
          task_tab.has_value());
    bool has_tab = task_tab.value() != nullptr;

    if (!has_tab && glic::GlicActorTaskIconManager::IsActiveExperimentalTask(
                        task_state.value(),
                        actor_ui_state_manager->GetFeatureMode(task_id))) {
      // Treat experimental triggering tasks as having a tab even if they don't
      // have one associated yet. This ensures they are clickable and can bring
      // the window/tab to the foreground.
      has_tab = true;
    }

    std::unique_ptr<ActorTaskListBubbleRowButton> row =
        std::make_unique<ActorTaskListBubbleRowButton>(
            base::BindRepeating(on_row_clicked_, task_id), task_state.value(),
            base::UTF8ToUTF16(task_title.value()), requires_processing, has_tab,
            actor_ui_state_manager->GetFeatureMode(task_id));

    contents_view->AddChildView(std::move(row));
    ++num_rows_;
  }
  return contents_view;
}
