// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_view.h"

#include <memory>

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {
namespace {

constexpr int kSpacingAboveListContainerView = 16;

// TODO(b/283371050): Localize these strings once finalized.
const char* const kStudentLists[] = {
    "Assigned",
    "No due date",
    "Missing",
    "Done",
};

// The maximum number of assignments shown at once.
constexpr int kMaxAssignments = 3;

class ClassroomStudentComboboxModel : public ui::ComboboxModel {
 public:
  ClassroomStudentComboboxModel() = default;
  ClassroomStudentComboboxModel(const ClassroomStudentComboboxModel&) = delete;
  ClassroomStudentComboboxModel& operator=(
      const ClassroomStudentComboboxModel&) = delete;
  ~ClassroomStudentComboboxModel() override = default;

  size_t GetItemCount() const override { return 4; }

  std::u16string GetItemAt(size_t index) const override {
    CHECK(index >= 0 || index < 4);
    return base::UTF8ToUTF16(kStudentLists[index]);
  }

  absl::optional<size_t> GetDefaultIndex() const override { return 0; }
};

}  // namespace

ClassroomBubbleView::ClassroomBubbleView() {
  header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

  combo_box_view_ =
      header_view_->AddChildView(std::make_unique<views::Combobox>(
          std::make_unique<ClassroomStudentComboboxModel>()));
  combo_box_view_->SetSelectedIndex(0);
  combo_box_view_->SetCallback(
      base::BindRepeating(&ClassroomBubbleView::SelectedAssignmentListChanged,
                          base::Unretained(this)));
  // TODO(b:283370907): Implement accessibility behavior.
  combo_box_view_->SetTooltipTextAndAccessibleName(u"Assignment list selector");

  list_container_view_ =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  list_container_view_->SetOrientation(views::LayoutOrientation::kVertical);
  list_container_view_->SetPaintToLayer();
  list_container_view_->layer()->SetFillsBoundsOpaquely(false);
  list_container_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(16));
  list_container_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(kSpacingAboveListContainerView, 0, 0, 0));

  // TODO(b/283370328): Implement fetching assignments for teachers.
  // TODO(b/283370862): Implement fetching assignments for students.
  if (Shell::Get()->glanceables_v2_controller()->GetClassroomClient()) {
    Shell::Get()
        ->glanceables_v2_controller()
        ->GetClassroomClient()
        ->GetStudentAssignmentsWithApproachingDueDate(
            base::BindOnce(&ClassroomBubbleView::OnGetStudentAssignmentsDueSoon,
                           weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Hide this bubble when no classroom client exists.
    SetVisible(false);
  }
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

void ClassroomBubbleView::OnGetStudentAssignmentsDueSoon(
    std::vector<std::unique_ptr<GlanceablesClassroomStudentAssignment>>
        assignments) {
  for (const auto& assignment : assignments) {
    list_container_view_->AddChildView(
        std::make_unique<GlanceablesClassroomItemView>(assignment.get()));
    if (list_container_view_->children().size() >= kMaxAssignments) {
      break;
    }
  }
}

void ClassroomBubbleView::SelectedAssignmentListChanged() {
  // TODO(b:277268122): Update list_container_view_.
}

BEGIN_METADATA(ClassroomBubbleView, views::View)
END_METADATA

}  // namespace ash
