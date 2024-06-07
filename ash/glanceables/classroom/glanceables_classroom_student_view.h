// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_STUDENT_VIEW_H_
#define ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_STUDENT_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view_observer.h"

class GURL;
class PrefRegistrySimple;
class PrefService;

namespace ui {
class ComboboxModel;
}

namespace views {
class BoxLayoutView;
class FlexLayoutView;
class Label;
class View;
class ViewObserver;
}  // namespace views

namespace ash {

class Combobox;
class CounterExpandButton;
class GlanceablesContentsScrollView;
class GlanceablesListFooterView;
class GlanceablesProgressBarView;
struct GlanceablesClassroomAssignment;

// This enum is used for metrics, so enum values should not be changed. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum class StudentAssignmentsListType {
  kAssigned = 0,
  kNoDueDate = 1,
  kMissing = 2,
  kDone = 3,
  kMaxValue = kDone
};

class ASH_EXPORT GlanceablesClassroomStudentView
    : public GlanceablesTimeManagementBubbleView,
      public views::ViewObserver {
  METADATA_HEADER(GlanceablesClassroomStudentView,
                  GlanceablesTimeManagementBubbleView)

 public:
  GlanceablesClassroomStudentView();
  GlanceablesClassroomStudentView(const GlanceablesClassroomStudentView&) =
      delete;
  GlanceablesClassroomStudentView& operator=(
      const GlanceablesClassroomStudentView&) = delete;
  ~GlanceablesClassroomStudentView() override;

  // Registers syncable user profile prefs with the specified `registry`.
  static void RegisterUserProfilePrefs(PrefRegistrySimple* registry);

  // Clears any student glanceables state from user `pref_services`.
  static void ClearUserStatePrefs(PrefService* pref_service);

  // views::ViewObserver:
  void OnViewFocused(views::View* view) override;

  // GlanceablesTimeManagementBubbleView:
  bool IsExpanded() const override;
  int GetCollapsedStatePreferredHeight() const override;

  // Invalidates any pending assignments requests. Called when the
  // glanceables bubble widget starts closing to avoid unnecessary UI updates.
  void CancelUpdates();

  // Creates `this` view's own background and updates layout accordingly.
  void CreateElevatedBackground();

  void SetExpandState(bool is_expanded);

 private:
  // Triggers classroom bubble resize animation to new preferred size, if an
  // animation is required.
  void AnimateResize();

  // Toggles `is_expanded_` and updates the layout.
  void ToggleExpandState();

  // Handles press on the "See all" button in `GlanceablesListFooterView`. Opens
  // classroom web UI based on the selected menu option.
  void OnSeeAllPressed();

  // Opens classroom url.
  void OpenUrl(const GURL& url) const;

  // Called when an item view is pressed/clicked on.
  void OnItemViewPressed(bool initial_list_selected, const GURL& url);

  // Called when the header icon is pressed/clicked on.
  void OnHeaderIconPressed();

  // Handle switching between assignment lists.
  void SelectedAssignmentListChanged(bool initial_update);

  // Handles received assignments by rendering them in `list_container_view_`.
  void OnGetAssignments(
      const std::u16string& list_name,
      bool initial_update,
      bool success,
      std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments);

  // Announces text describing the assignment list state through a screen
  // reader, using `combo_box_view_` view accessibility helper.
  void AnnounceListStateOnComboBoxAccessibility();

  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> header_view_ = nullptr;
  raw_ptr<Combobox> combo_box_view_ = nullptr;
  // This is a simple label that copies the label style on `combo_box_view_` so
  // that it can visually replace it when `combo_box_view_` is hidden.
  raw_ptr<views::Label> combobox_replacement_label_ = nullptr;
  raw_ptr<GlanceablesContentsScrollView> content_scroll_view_ = nullptr;
  raw_ptr<views::FlexLayoutView> body_container_ = nullptr;
  raw_ptr<views::BoxLayoutView> list_container_view_ = nullptr;
  raw_ptr<GlanceablesListFooterView> list_footer_view_ = nullptr;
  raw_ptr<GlanceablesProgressBarView> progress_bar_ = nullptr;
  raw_ptr<views::Label> empty_list_label_ = nullptr;
  raw_ptr<CounterExpandButton> expand_button_ = nullptr;

  // Whether the view is expanded and showing the contents in `body_container_`.
  bool is_expanded_ = true;

  // Total number of assignments in the selected assignment list.
  size_t total_assignments_ = 0u;

  // Time stamp of when the view was created.
  const base::Time shown_time_;

  // Records the time when the bubble was about to request an assignment list.
  // Used for metrics.
  base::TimeTicks assignments_requested_time_;

  // The start time that a selected assignment list is shown.
  std::optional<base::TimeTicks> list_shown_start_time_;

  // Whether the first assignment list has been shown in this view's lifetime.
  bool first_assignment_list_shown_ = false;

  // The number of times that the selected list has changed during the lifetime
  // of this view.
  int selected_list_change_count_ = 0;

  // The currently selected assignment list.
  StudentAssignmentsListType selected_list_type_ =
      StudentAssignmentsListType::kAssigned;

  base::ScopedObservation<views::View, views::ViewObserver>
      combobox_view_observation_{this};

  base::WeakPtrFactory<GlanceablesClassroomStudentView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_CLASSROOM_GLANCEABLES_CLASSROOM_STUDENT_VIEW_H_
