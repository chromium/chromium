// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_teacher_view.h"

#include <array>
#include <memory>
#include <utility>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/combobox/combobox.h"

namespace ash {
namespace {

constexpr auto kIndividualItemViewMargin = gfx::Insets::TLBR(0, 0, 2, 0);

constexpr int kMaxAssignments = 3;

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
    CHECK(index >= 0 || index < kTeacherAssignmentsListTypeOrdered.size());

    const auto* const iter = kTeacherAssignmentsListTypeToLabel.find(
        kTeacherAssignmentsListTypeOrdered[index]);
    CHECK(iter != kTeacherAssignmentsListTypeToLabel.end());

    return base::UTF8ToUTF16(iter->second);
  }

  absl::optional<size_t> GetDefaultIndex() const override { return 0; }
};

}  // namespace

ClassroomBubbleTeacherView::ClassroomBubbleTeacherView(
    DetailedViewDelegate* delegate)
    : ClassroomBubbleBaseView(
          delegate,
          std::make_unique<ClassroomTeacherComboboxModel>()) {
  combo_box_view_->SetCallback(base::BindRepeating(
      &ClassroomBubbleTeacherView::SelectedAssignmentListChanged,
      base::Unretained(this)));
  SelectedAssignmentListChanged();
}

ClassroomBubbleTeacherView::~ClassroomBubbleTeacherView() = default;

void ClassroomBubbleTeacherView::OnGetTeacherAssignments(
    std::vector<std::unique_ptr<GlanceablesClassroomTeacherAssignment>>
        assignments) {
  list_container_view_->RemoveAllChildViews();

  for (const auto& assignment : assignments) {
    list_container_view_
        ->AddChildView(std::make_unique<GlanceablesClassroomTeacherItemView>(
            assignment.get()))
        ->SetProperty(views::kMarginsKey, kIndividualItemViewMargin);

    if (list_container_view_->children().size() >= kMaxAssignments) {
      break;
    }
  }

  if (!list_container_view_->children().empty()) {
    // Reset bottom margin of the last element in the list.
    list_container_view_->children().back()->SetProperty(views::kMarginsKey,
                                                         gfx::Insets());
  }
}

void ClassroomBubbleTeacherView::SelectedAssignmentListChanged() {
  auto* const client =
      Shell::Get()->glanceables_v2_controller()->GetClassroomClient();
  if (!client) {
    // Hide this bubble when no classroom client exists.
    SetVisible(false);
    return;
  }

  CHECK(combo_box_view_->GetSelectedIndex());
  const auto selected_index = combo_box_view_->GetSelectedIndex().value();
  CHECK(selected_index >= 0 ||
        selected_index < kTeacherAssignmentsListTypeOrdered.size());

  auto callback =
      base::BindOnce(&ClassroomBubbleTeacherView::OnGetTeacherAssignments,
                     weak_ptr_factory_.GetWeakPtr());
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

}  // namespace ash
