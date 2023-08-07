// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_student_view.h"

#include <array>
#include <memory>
#include <utility>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_piece_forward.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace ash {
namespace {

enum class StudentAssignmentsListType {
  kAssigned,
  kNoDueDate,
  kMissing,
  kDone,
};

// Helps to map `combo_box_view_` selected index to the corresponding
// `StudentAssignmentsListType` value.
constexpr std::array<StudentAssignmentsListType, 4>
    kStudentAssignmentsListTypeOrdered = {
        StudentAssignmentsListType::kAssigned,
        StudentAssignmentsListType::kNoDueDate,
        StudentAssignmentsListType::kMissing,
        StudentAssignmentsListType::kDone};

// TODO(b/283371050): Localize these strings once finalized.
constexpr auto kStudentAssignmentsListTypeToLabel =
    base::MakeFixedFlatMap<StudentAssignmentsListType, base::StringPiece>(
        {{StudentAssignmentsListType::kAssigned, "Assigned"},
         {StudentAssignmentsListType::kNoDueDate, "No due date"},
         {StudentAssignmentsListType::kMissing, "Missing"},
         {StudentAssignmentsListType::kDone, "Done"}});

constexpr char kClassroomWebUIAssignedUrl[] =
    "https://classroom.google.com/u/0/a/not-turned-in/all";
constexpr char kClassroomWebUIMissingUrl[] =
    "https://classroom.google.com/u/0/a/missing/all";
constexpr char kClassroomWebUIDoneUrl[] =
    "https://classroom.google.com/u/0/a/turned-in/all";

std::u16string GetAssignmentListName(size_t index) {
  CHECK(index >= 0 || index < kStudentAssignmentsListTypeOrdered.size());

  const auto* const iter = kStudentAssignmentsListTypeToLabel.find(
      kStudentAssignmentsListTypeOrdered[index]);
  CHECK(iter != kStudentAssignmentsListTypeToLabel.end());

  return base::UTF8ToUTF16(iter->second);
}

class ClassroomStudentComboboxModel : public ui::ComboboxModel {
 public:
  ClassroomStudentComboboxModel() = default;
  ClassroomStudentComboboxModel(const ClassroomStudentComboboxModel&) = delete;
  ClassroomStudentComboboxModel& operator=(
      const ClassroomStudentComboboxModel&) = delete;
  ~ClassroomStudentComboboxModel() override = default;

  size_t GetItemCount() const override {
    return kStudentAssignmentsListTypeOrdered.size();
  }

  std::u16string GetItemAt(size_t index) const override {
    return GetAssignmentListName(index);
  }

  absl::optional<size_t> GetDefaultIndex() const override { return 0; }
};

}  // namespace

ClassroomBubbleStudentView::ClassroomBubbleStudentView(
    DetailedViewDelegate* delegate)
    : ClassroomBubbleBaseView(
          delegate,
          std::make_unique<ClassroomStudentComboboxModel>()) {
  combo_box_view_->SetCallback(base::BindRepeating(
      &ClassroomBubbleStudentView::SelectedAssignmentListChanged,
      base::Unretained(this)));
  SelectedAssignmentListChanged();
}

ClassroomBubbleStudentView::~ClassroomBubbleStudentView() = default;

void ClassroomBubbleStudentView::OnSeeAllPressed() {
  CHECK(combo_box_view_->GetSelectedIndex());
  const auto selected_index = combo_box_view_->GetSelectedIndex().value();
  CHECK(selected_index >= 0 ||
        selected_index < kStudentAssignmentsListTypeOrdered.size());

  switch (kStudentAssignmentsListTypeOrdered[selected_index]) {
    case StudentAssignmentsListType::kAssigned:
    case StudentAssignmentsListType::kNoDueDate:
      return OpenUrl(GURL(kClassroomWebUIAssignedUrl));
    case StudentAssignmentsListType::kMissing:
      return OpenUrl(GURL(kClassroomWebUIMissingUrl));
    case StudentAssignmentsListType::kDone:
      return OpenUrl(GURL(kClassroomWebUIDoneUrl));
  }
}

void ClassroomBubbleStudentView::SelectedAssignmentListChanged() {
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
        selected_index < kStudentAssignmentsListTypeOrdered.size());

  // Cancel any old pending assignment requests.
  weak_ptr_factory_.InvalidateWeakPtrs();

  AboutToRequestAssignments();

  auto callback = base::BindOnce(&ClassroomBubbleStudentView::OnGetAssignments,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 GetAssignmentListName(selected_index));
  switch (kStudentAssignmentsListTypeOrdered[selected_index]) {
    case StudentAssignmentsListType::kAssigned:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_DUE_LIST));
      return client->GetStudentAssignmentsWithApproachingDueDate(
          std::move(callback));
    case StudentAssignmentsListType::kNoDueDate:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_DUE_LIST));
      return client->GetStudentAssignmentsWithoutDueDate(std::move(callback));
    case StudentAssignmentsListType::kMissing:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_MISSING_LIST));
      return client->GetStudentAssignmentsWithMissedDueDate(
          std::move(callback));
    case StudentAssignmentsListType::kDone:
      empty_list_label_->SetText(l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_STUDENT_EMPTY_ITEM_DONE_LIST));
      return client->GetCompletedStudentAssignments(std::move(callback));
  }
}

BEGIN_METADATA(ClassroomBubbleStudentView, views::View)
END_METADATA

}  // namespace ash
