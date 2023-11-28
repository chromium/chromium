// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_teacher_view.h"

#include <array>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/shell.h"
#include "ash/style/combobox.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace ash {
namespace {

enum class TeacherAssignmentsListType {
  kDueSoon,
  kRecentlyDue,
  kNoDueDate,
  kGraded,
};

// Helps to map `combo_box_view_` selected index to the corresponding
// `TeacherAssignmentsListType` value.
constexpr std::array<TeacherAssignmentsListType, 4>
    kTeacherAssignmentsListTypeOrdered = {
        TeacherAssignmentsListType::kDueSoon,
        TeacherAssignmentsListType::kRecentlyDue,
        TeacherAssignmentsListType::kNoDueDate,
        TeacherAssignmentsListType::kGraded};

// TODO(b/283371050): Localize these strings once finalized.
constexpr auto kTeacherAssignmentsListTypeToLabel =
    base::MakeFixedFlatMap<TeacherAssignmentsListType, base::StringPiece>(
        {{TeacherAssignmentsListType::kDueSoon, "Due Soon"},
         {TeacherAssignmentsListType::kRecentlyDue, "Recently Due"},
         {TeacherAssignmentsListType::kNoDueDate, "No Due Date"},
         {TeacherAssignmentsListType::kGraded, "Graded"}});

constexpr char kClassroomWebUIToReviewUrl[] =
    "https://classroom.google.com/u/0/ta/not-reviewed/all";
constexpr char kClassroomWebUIReviewedUrl[] =
    "https://classroom.google.com/u/0/ta/reviewed/all";

std::u16string GetAssignmentListName(size_t index) {
  CHECK(index >= 0 || index < kTeacherAssignmentsListTypeOrdered.size());

  const auto* const iter = kTeacherAssignmentsListTypeToLabel.find(
      kTeacherAssignmentsListTypeOrdered[index]);
  CHECK(iter != kTeacherAssignmentsListTypeToLabel.end());

  return base::UTF8ToUTF16(iter->second);
}

class ClassroomTeacherComboboxModel : public ui::ComboboxModel {
 public:
  ClassroomTeacherComboboxModel() = default;
  ClassroomTeacherComboboxModel(const ClassroomTeacherComboboxModel&) = delete;
  ClassroomTeacherComboboxModel& operator=(
      const ClassroomTeacherComboboxModel&) = delete;
  ~ClassroomTeacherComboboxModel() override = default;

  size_t GetItemCount() const override {
    return kTeacherAssignmentsListTypeOrdered.size();
  }

  std::u16string GetItemAt(size_t index) const override {
    return GetAssignmentListName(index);
  }

  std::optional<size_t> GetDefaultIndex() const override { return 0; }
};

}  // namespace

ClassroomBubbleTeacherView::ClassroomBubbleTeacherView()
    : ClassroomBubbleBaseView(
          std::make_unique<ClassroomTeacherComboboxModel>()) {
  CHECK(features::IsGlanceablesV2ClassroomTeacherViewEnabled());
  combo_box_view_->SetSelectionChangedCallback(base::BindRepeating(
      &ClassroomBubbleTeacherView::SelectedAssignmentListChanged,
      base::Unretained(this),
      /*initial_update=*/false));
  SelectedAssignmentListChanged(/*initial_update=*/true);
}

ClassroomBubbleTeacherView::~ClassroomBubbleTeacherView() = default;

void ClassroomBubbleTeacherView::OnSeeAllPressed() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_SeeAllPressed"));
  CHECK(combo_box_view_->GetSelectedIndex());
  const auto selected_index = combo_box_view_->GetSelectedIndex().value();
  CHECK(selected_index >= 0 ||
        selected_index < kTeacherAssignmentsListTypeOrdered.size());

  switch (kTeacherAssignmentsListTypeOrdered[selected_index]) {
    case TeacherAssignmentsListType::kDueSoon:
    case TeacherAssignmentsListType::kRecentlyDue:
    case TeacherAssignmentsListType::kNoDueDate:
      return OpenUrl(GURL(kClassroomWebUIToReviewUrl));
    case TeacherAssignmentsListType::kGraded:
      return OpenUrl(GURL(kClassroomWebUIReviewedUrl));
  }
}

void ClassroomBubbleTeacherView::SelectedAssignmentListChanged(
    bool initial_update) {
  if (!initial_update) {
    base::RecordAction(
        base::UserMetricsAction("Glanceables_Classroom_SelectedListChanged"));
  }
  auto* const client =
      Shell::Get()->glanceables_controller()->GetClassroomClient();
  if (!client) {
    // Hide this bubble when no classroom client exists.
    SetVisible(false);
    return;
  }

  CHECK(combo_box_view_->GetSelectedIndex());
  const auto selected_index = combo_box_view_->GetSelectedIndex().value();
  CHECK(selected_index >= 0 ||
        selected_index < kTeacherAssignmentsListTypeOrdered.size());

  // Cancel any old pending assignment callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  AboutToRequestAssignments();

  auto callback =
      base::BindOnce(&ClassroomBubbleTeacherView::OnGetAssignments,
                     weak_ptr_factory_.GetWeakPtr(),
                     GetAssignmentListName(selected_index), initial_update);
  switch (kTeacherAssignmentsListTypeOrdered[selected_index]) {
    case TeacherAssignmentsListType::kDueSoon:
      return client->GetTeacherAssignmentsWithApproachingDueDate(
          std::move(callback));
    case TeacherAssignmentsListType::kRecentlyDue:
      return client->GetTeacherAssignmentsRecentlyDue(std::move(callback));
    case TeacherAssignmentsListType::kNoDueDate:
      return client->GetTeacherAssignmentsWithoutDueDate(std::move(callback));
    case TeacherAssignmentsListType::kGraded:
      return client->GetGradedTeacherAssignments(std::move(callback));
  }
}

BEGIN_METADATA(ClassroomBubbleTeacherView, views::View)
END_METADATA

}  // namespace ash
