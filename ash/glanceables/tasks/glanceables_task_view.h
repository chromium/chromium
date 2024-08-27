// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
#define ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_

#include <string>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/ash_export.h"
#include "ash/glanceables/tasks/glanceables_tasks_error_type.h"
#include "ash/style/error_message_toast.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "google_apis/common/api_error_codes.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_observer.h"

namespace views {
class ImageButton;
class ImageView;
class LabelButton;
}  // namespace views

namespace ash {

class SystemTextfield;

// `GlanceablesTaskView` uses `views::FlexLayout` to show tasks metadata within
// the `GlanceablesTasksView`.
// +---------------------------------------------------------------+
// |`GlanceablesTaskView`                                        |
// |                                                               |
// | +-----------------+ +---------------------------------------+ |
// | |'check_button_'  | |'contents_view_'                       | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | |'tasks_title_view_'                | | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | +-----------------------------------+ | |
// | |                 | | |'tasks_details_view_'              | | |
// | |                 | | +-----------------------------------+ | |
// | +-----------------+ +---------------------------------------+ |
// +---------------------------------------------------------------+
class ASH_EXPORT GlanceablesTaskView : public views::FlexLayoutView,
                                       public views::ViewObserver {
  METADATA_HEADER(GlanceablesTaskView, views::FlexLayoutView)

 public:
  using MarkAsCompletedCallback =
      base::RepeatingCallback<void(const std::string& task_id, bool completed)>;
  using SaveCallback = base::RepeatingCallback<void(
      base::WeakPtr<GlanceablesTaskView> view,
      const std::string& task_id,
      const std::string& title,
      api::TasksClient::OnTaskSavedCallback callback)>;
  using ShowErrorMessageCallback =
      base::RepeatingCallback<void(GlanceablesTasksErrorType,
                                   ErrorMessageToast::ButtonActionType)>;
  using StateChangeObserverCallback =
      base::RepeatingCallback<void(bool view_expanding)>;
  // Modes of `tasks_title_view_` (simple label or text field).
  enum class TaskTitleViewState { kNotInitialized, kView, kEdit };

  GlanceablesTaskView(const api::Task* task,
                      MarkAsCompletedCallback mark_as_completed_callback,
                      SaveCallback save_callback,
                      base::RepeatingClosure edit_in_browser_callback,
                      ShowErrorMessageCallback show_error_message_callback);
  GlanceablesTaskView(const GlanceablesTaskView&) = delete;
  GlanceablesTaskView& operator=(const GlanceablesTaskView&) = delete;
  ~GlanceablesTaskView() override;

  void set_state_change_observer(const StateChangeObserverCallback& observer) {
    state_change_observer_ = observer;
  }

  // views::ViewObserver:
  void OnViewBlurred(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  const views::ImageButton* GetCheckButtonForTest() const;
  bool GetCompletedForTest() const;
  void SetCheckedForTest(bool checked);

  // Updates `tasks_title_view_` according to `state`.
  void UpdateTaskTitleViewForState(TaskTitleViewState state);

  // Sets the network to be connected. This should only be used in tests.
  static void SetIsNetworkConnectedForTest(bool connected);

 private:
  class CheckButton;
  class TaskTitleButton;

  // Adds additional content (assigned task privacy notice if available + edit
  // in browser button) when the view is in edit mode.
  void AddExtraContentForEditState();

  // Updates the margins of views in `contents_view_`.
  void UpdateContentsMargins(TaskTitleViewState state);

  // Updates the `extra_content_for_edit_state_` visibility animating its
  // opacity. When hiding the container, the container is hidden immediately,
  // but copy of its layer is created, and animated in its place.
  void AnimateExtraContentForEditStateVisibility(bool visible);
  void OnExtraContentForEditStateVisibilityAnimationCompleted();

  // Handles press events on `check_button_`.
  void CheckButtonPressed();

  // Handles press events on `task_title_button_`.
  void TaskTitleButtonPressed();

  // Handles finished editing event from the text field, updates `task_title_`
  // and propagates new `title` to the server.
  void OnFinishedEditing(const std::u16string& title);

  // Handles completion of running `save_callback_` callback.
  // `task` - newly created or updated task.
  void OnSaved(google_apis::ApiErrorCode http_error, const api::Task* task);

  // Owned by views hierarchy.
  raw_ptr<CheckButton> check_button_ = nullptr;
  raw_ptr<views::FlexLayoutView> contents_view_ = nullptr;
  raw_ptr<views::FlexLayoutView> tasks_title_view_ = nullptr;
  raw_ptr<TaskTitleButton> task_title_button_ = nullptr;
  raw_ptr<SystemTextfield> task_title_textfield_ = nullptr;
  raw_ptr<views::FlexLayoutView> tasks_details_view_ = nullptr;
  raw_ptr<views::ImageView> origin_surface_type_icon_ = nullptr;
  raw_ptr<views::View> extra_content_for_edit_state_ = nullptr;
  raw_ptr<views::LabelButton> edit_in_browser_button_ = nullptr;

  // ID for the task represented by this view.
  std::string task_id_;

  // Title of the task.
  std::u16string task_title_;

  // The type of surface a task originates from.
  api::Task::OriginSurfaceType origin_surface_type_;

  // Cached to reset the value of `task_title_` when the new title failed to
  // commit after editing.
  std::u16string task_title_before_edit_ = u"";

  bool saving_task_changes_ = false;

  // Marks the task as completed.
  const MarkAsCompletedCallback mark_as_completed_callback_;

  // Saves the task (either creates or updates the existing one).
  const SaveCallback save_callback_;

  // `edit_in_browser_button_` callback that opens the Tasks in browser.
  const base::RepeatingClosure edit_in_browser_callback_;

  // Shows an error message in the parent `GlanceablesTasksView`.
  const ShowErrorMessageCallback show_error_message_callback_;

  // Callback which, if set, gets called when the task view state changes
  // between editing and viewing state.
  StateChangeObserverCallback state_change_observer_;

  // Copy of `extra_content_for_edit_state_` (assigned task privacy notice if
  // available + edit in browser button) layer used for the visibility animation
  // when hiding the container. The container gets hidden immediately, and the
  // layer is faded out in its place. Deleted when the fade out animation
  // completes.
  std::unique_ptr<ui::Layer> animating_extra_content_for_edit_state_layer_;

  base::ScopedMultiSourceObservation<views::View, GlanceablesTaskView>
      edit_exit_observer_{this};

  base::WeakPtrFactory<GlanceablesTaskView> state_change_weak_ptr_factory_{
      this};

  base::WeakPtrFactory<GlanceablesTaskView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_TASKS_GLANCEABLES_TASK_VIEW_H_
