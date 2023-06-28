// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_view.h"

#include <array>
#include <memory>
#include <utility>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kSpacingAboveListContainerView = 16;
constexpr auto kIndividualItemViewMargin = gfx::Insets::TLBR(0, 0, 2, 0);

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

// The maximum number of assignments shown at once.
constexpr int kMaxAssignments = 3;

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
    CHECK(index >= 0 || index < kStudentAssignmentsListTypeOrdered.size());

    const auto* const iter = kStudentAssignmentsListTypeToLabel.find(
        kStudentAssignmentsListTypeOrdered[index]);
    CHECK(iter != kStudentAssignmentsListTypeToLabel.end());

    return base::UTF8ToUTF16(iter->second);
  }

  absl::optional<size_t> GetDefaultIndex() const override { return 0; }
};

}  // namespace

ClassroomBubbleView::ClassroomBubbleView(DetailedViewDelegate* delegate)
    : GlanceableTrayChildBubble(delegate) {
  header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));
  header_view_->SetProperty(views::kMarginsKey, gfx::Insets(16));

  auto* const header_icon =
      header_view_->AddChildView(std::make_unique<views::ImageView>());
  header_icon->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysBaseElevated, 16));
  header_icon->SetImage(ui::ImageModel::FromVectorIcon(
      kGlanceablesClassroomIcon, cros_tokens::kCrosSysOnSurface, 20));
  header_icon->SetPreferredSize(gfx::Size(32, 32));
  header_icon->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 4));

  combo_box_view_ =
      header_view_->AddChildView(std::make_unique<views::Combobox>(
          std::make_unique<ClassroomStudentComboboxModel>()));
  combo_box_view_->SetID(kComboBoxViewId);
  combo_box_view_->SetSelectedIndex(0);
  combo_box_view_->SetCallback(
      base::BindRepeating(&ClassroomBubbleView::SelectedAssignmentListChanged,
                          base::Unretained(this)));
  // TODO(b:283370907): Implement accessibility behavior.
  combo_box_view_->SetTooltipTextAndAccessibleName(u"Assignment list selector");

  list_container_view_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  list_container_view_->SetID(kListContainerViewId);
  list_container_view_->SetOrientation(views::LayoutOrientation::kVertical);
  list_container_view_->SetPaintToLayer();
  list_container_view_->layer()->SetFillsBoundsOpaquely(false);
  list_container_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(16));
  list_container_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kSpacingAboveListContainerView, 0, 0, 0));

  // TODO(b/283370328): Implement fetching assignments for teachers.
  SelectedAssignmentListChanged();
}

ClassroomBubbleView::~ClassroomBubbleView() = default;

// views::View:
void ClassroomBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // TODO(b:283370907): Implement accessibility behavior.
  if (!GetVisible()) {
    return;
  }
  node_data->role = ax::mojom::Role::kListBox;
  node_data->SetName(u"Glanceables Bubble Classroom View Accessible Name");
}

void ClassroomBubbleView::OnGetStudentAssignments(
    std::vector<std::unique_ptr<GlanceablesClassroomStudentAssignment>>
        assignments) {
  list_container_view_->RemoveAllChildViews();

  for (const auto& assignment : assignments) {
    list_container_view_
        ->AddChildView(
            std::make_unique<GlanceablesClassroomItemView>(assignment.get()))
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

void ClassroomBubbleView::SelectedAssignmentListChanged() {
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

  auto callback = base::BindOnce(&ClassroomBubbleView::OnGetStudentAssignments,
                                 weak_ptr_factory_.GetWeakPtr());
  switch (kStudentAssignmentsListTypeOrdered[selected_index]) {
    case StudentAssignmentsListType::kAssigned:
      return client->GetStudentAssignmentsWithApproachingDueDate(
          std::move(callback));
    case StudentAssignmentsListType::kNoDueDate:
      return client->GetStudentAssignmentsWithoutDueDate(std::move(callback));
    case StudentAssignmentsListType::kMissing:
      return client->GetStudentAssignmentsWithMissedDueDate(
          std::move(callback));
    case StudentAssignmentsListType::kDone:
      return client->GetCompletedStudentAssignments(std::move(callback));
  }
}

BEGIN_METADATA(ClassroomBubbleView, views::View)
END_METADATA

}  // namespace ash
