// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_student_view.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/combobox.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Helps to map `combo_box_view_` selected index to the corresponding
// `StudentAssignmentsListType` value.
constexpr std::array<StudentAssignmentsListType, 4>
    kStudentAssignmentsListTypeOrdered = {
        StudentAssignmentsListType::kAssigned,
        StudentAssignmentsListType::kNoDueDate,
        StudentAssignmentsListType::kMissing,
        StudentAssignmentsListType::kDone};

constexpr auto kStudentAssignmentsListTypeToLabel =
    base::MakeFixedFlatMap<StudentAssignmentsListType, int>(
        {{StudentAssignmentsListType::kAssigned,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_DUE_SOON_LIST_NAME},
         {StudentAssignmentsListType::kNoDueDate,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_NO_DUE_DATE_LIST_NAME},
         {StudentAssignmentsListType::kMissing,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_MISSING_LIST_NAME},
         {StudentAssignmentsListType::kDone,
          IDS_GLANCEABLES_CLASSROOM_STUDENT_DONE_LIST_NAME}});

constexpr char kClassroomWebUIAssignedUrl[] =
    "https://classroom.google.com/u/0/a/not-turned-in/all";
constexpr char kClassroomWebUIMissingUrl[] =
    "https://classroom.google.com/u/0/a/missing/all";
constexpr char kClassroomWebUIDoneUrl[] =
    "https://classroom.google.com/u/0/a/turned-in/all";

const char kLastSelectedAssignmentsListPref[] =
    "ash.glanceables.classroom.student.last_selected_assignments_list";

std::u16string GetAssignmentListName(size_t index) {
  CHECK(index >= 0 || index < kStudentAssignmentsListTypeOrdered.size());

  const auto* const iter = kStudentAssignmentsListTypeToLabel.find(
      kStudentAssignmentsListTypeOrdered[index]);
  CHECK(iter != kStudentAssignmentsListTypeToLabel.end());

  return l10n_util::GetStringUTF16(iter->second);
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

  absl::optional<size_t> GetDefaultIndex() const override {
    const auto selected_list_type = static_cast<StudentAssignmentsListType>(
        Shell::Get()->session_controller()->GetActivePrefService()->GetInteger(
            kLastSelectedAssignmentsListPref));
    const auto* const iter =
        std::find(kStudentAssignmentsListTypeOrdered.begin(),
                  kStudentAssignmentsListTypeOrdered.end(), selected_list_type);
    return iter != kStudentAssignmentsListTypeOrdered.end()
               ? iter - kStudentAssignmentsListTypeOrdered.begin()
               : 0;
  }
};

}  // namespace

ClassroomBubbleStudentView::ClassroomBubbleStudentView(
    DetailedViewDelegate* delegate)
    : ClassroomBubbleBaseView(
          delegate,
          std::make_unique<ClassroomStudentComboboxModel>()) {
  combo_box_view_->SetSelectionChangedCallback(base::BindRepeating(
      &ClassroomBubbleStudentView::SelectedAssignmentListChanged,
      base::Unretained(this),
      /*initial_update=*/false));
  SelectedAssignmentListChanged(/*initial_update=*/true);
}

ClassroomBubbleStudentView::~ClassroomBubbleStudentView() {
  if (list_shown_start_time_.has_value()) {
    RecordStudentAssignmentListShowTime(
        selected_list_type_,
        base::TimeTicks::Now() - list_shown_start_time_.value(),
        /*default_list=*/selected_list_change_count_ == 0);
  }
  if (first_assignment_list_shown_) {
    RecordStudentSelectedListChangeCount(selected_list_change_count_);
  }
}

// static
void ClassroomBubbleStudentView::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      kLastSelectedAssignmentsListPref,
      base::to_underlying(StudentAssignmentsListType::kAssigned));
}

// static
void ClassroomBubbleStudentView::ClearUserStatePrefs(
    PrefService* pref_service) {
  pref_service->ClearPref(kLastSelectedAssignmentsListPref);
}

void ClassroomBubbleStudentView::OnSeeAllPressed() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_SeeAllPressed"));
  CHECK(combo_box_view_->GetSelectedIndex());

  switch (selected_list_type_) {
    case StudentAssignmentsListType::kAssigned:
    case StudentAssignmentsListType::kNoDueDate:
      return OpenUrl(GURL(kClassroomWebUIAssignedUrl));
    case StudentAssignmentsListType::kMissing:
      return OpenUrl(GURL(kClassroomWebUIMissingUrl));
    case StudentAssignmentsListType::kDone:
      return OpenUrl(GURL(kClassroomWebUIDoneUrl));
  }
}

void ClassroomBubbleStudentView::SelectedAssignmentListChanged(
    bool initial_update) {
  auto* const client =
      Shell::Get()->glanceables_controller()->GetClassroomClient();
  if (!client) {
    // Hide this bubble when no classroom client exists.
    SetVisible(false);
    return;
  }

  const auto prev_selected_list_type = selected_list_type_;
  CHECK(combo_box_view_->GetSelectedIndex());
  const auto selected_index = combo_box_view_->GetSelectedIndex().value();
  CHECK(selected_index >= 0 ||
        selected_index < kStudentAssignmentsListTypeOrdered.size());
  selected_list_type_ = kStudentAssignmentsListTypeOrdered[selected_index];

  if (!initial_update) {
    base::RecordAction(
        base::UserMetricsAction("Glanceables_Classroom_SelectedListChanged"));
    if (list_shown_start_time_.has_value()) {
      RecordStudentAssignmentListShowTime(
          prev_selected_list_type,
          base::TimeTicks::Now() - list_shown_start_time_.value(),
          /*default_list=*/selected_list_change_count_ == 0);
    }
    RecordStudentAssignmentListSelected(selected_list_type_);
    selected_list_change_count_++;
  }
  list_shown_start_time_.reset();

  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      kLastSelectedAssignmentsListPref,
      base::to_underlying(selected_list_type_));

  // Cancel any old pending assignment requests.
  weak_ptr_factory_.InvalidateWeakPtrs();

  AboutToRequestAssignments();

  auto callback =
      base::BindOnce(&ClassroomBubbleStudentView::OnGetAssignments,
                     weak_ptr_factory_.GetWeakPtr(),
                     GetAssignmentListName(selected_index), initial_update);
  switch (selected_list_type_) {
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
