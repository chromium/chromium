// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_task_view.h"

#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/time/date_helper.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/image_view.h"

namespace {

constexpr int kIconSize = 20;
constexpr char kFormatterPattern[] = "EEE, MMM d";  // "Wed, Feb 28"

constexpr int kBackgroundRadius = 4;
constexpr auto kTimeIconMargin = gfx::Insets::TLBR(0, 0, 0, 4);
constexpr auto kSubtaskIconMargin = gfx::Insets::TLBR(0, 4, 0, 0);

constexpr auto kSingleRowButtonMargin = gfx::Insets::VH(13, 18);
constexpr auto kDoubleRowButtonMargin = gfx::Insets::VH(16, 18);

constexpr auto kSingleRowTextMargins = gfx::Insets::VH(13, 0);
constexpr auto kDoubleRowTextMargins = gfx::Insets::VH(7, 0);

views::Label* SetupLabel(views::FlexLayoutView* parent) {
  views::Label* label = parent->AddChildView(std::make_unique<views::Label>());
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Views should not be individually selected for accessibility. Accessible
  // name and behavior comes from the parent.
  label->GetViewAccessibility().OverrideIsIgnored(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetAutoColorReadabilityEnabled(false);
  return label;
}

std::u16string GetFormattedDueDate(const base::Time& due) {
  const auto midnight_today = base::Time::Now().LocalMidnight();
  const auto midnight_tomorrow = midnight_today + base::Days(1);

  if (midnight_today <= due && due < midnight_tomorrow) {
    return l10n_util::GetStringUTF16(IDS_GLANCEABLES_DUE_TODAY);
  }

  auto* const date_helper = ash::DateHelper::GetInstance();
  CHECK(date_helper);
  const auto formatter =
      date_helper->CreateSimpleDateFormatter(kFormatterPattern);
  return date_helper->GetFormattedTime(&formatter, due);
}

}  // namespace

namespace ash {

class GlanceablesTaskView::CheckButton : public views::ImageButton {
 public:
  CheckButton(PressedCallback pressed_callback)
      : views::ImageButton(pressed_callback) {
    SetAccessibleRole(ax::mojom::Role::kCheckBox);
    // TODO(b/294681832): Finalize, and then localize strings.
    SetAccessibleName(u"Mark completed");
    UpdateImage();
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::ImageButton::GetAccessibleNodeData(node_data);

    const ax::mojom::CheckedState checked_state =
        checked_ ? ax::mojom::CheckedState::kTrue
                 : ax::mojom::CheckedState::kFalse;
    node_data->SetCheckedState(checked_state);
    node_data->SetDefaultActionVerb(checked_
                                        ? ax::mojom::DefaultActionVerb::kUncheck
                                        : ax::mojom::DefaultActionVerb::kCheck);
  }

  void SetChecked(bool checked) {
    checked_ = checked;
    UpdateImage();
    NotifyAccessibilityEvent(ax::mojom::Event::kCheckedStateChanged, true);
  }

  bool checked() const { return checked_; }

 private:
  void UpdateImage() {
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(
            checked_ ? ash::kHollowCheckCircleIcon : ash::kHollowCircleIcon,
            cros_tokens::kFocusRingColor, kIconSize));
  }

  bool checked_ = false;
};

GlanceablesTaskView::GlanceablesTaskView(const std::string& task_list_id,
                                         const GlanceablesTask* task)
    : task_list_id_(task_list_id), task_id_(task->id) {
  SetAccessibleRole(ax::mojom::Role::kListItem);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kBackgroundRadius));

  button_ = AddChildView(std::make_unique<CheckButton>(base::BindRepeating(
      &GlanceablesTaskView::ButtonPressed, base::Unretained(this))));

  contents_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  contents_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  contents_view_->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  contents_view_->SetOrientation(views::LayoutOrientation::kVertical);
  contents_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded));

  tasks_title_view_ =
      contents_view_->AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_details_view_ =
      contents_view_->AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_details_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_details_view_->SetOrientation(views::LayoutOrientation::kHorizontal);

  tasks_label_ = SetupLabel(tasks_title_view_);
  tasks_label_->SetText(base::UTF8ToUTF16(task->title));
  tasks_label_->SetLineHeight(TypographyProvider::Get()->ResolveLineHeight(
      TypographyToken::kCrosButton2));
  SetupTasksLabel(/*completed=*/false);

  std::vector<std::u16string> details;
  if (task->due.has_value()) {
    views::ImageView* time_icon_view =
        tasks_details_view_->AddChildView(std::make_unique<views::ImageView>());
    time_icon_view->SetProperty(views::kMarginsKey, kTimeIconMargin);

    views::Label* due_date_label = SetupLabel(tasks_details_view_);
    due_date_label->SetText(GetFormattedDueDate(task->due.value()));
    // TODO(b/294681832): Finalize, and then localize strings.
    details.push_back(u"Due " + GetFormattedDueDate(task->due.value()));
    due_date_label->SetFontList(
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosAnnotation1));
    due_date_label->SetLineHeight(TypographyProvider::Get()->ResolveLineHeight(
        TypographyToken::kCrosAnnotation1));

    if (chromeos::features::IsJellyEnabled()) {
      time_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
          kGlanceablesTasksDueDateIcon, cros_tokens::kCrosSysOnSurfaceVariant));
      due_date_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
    } else {
      time_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
          kGlanceablesTasksDueDateIcon, kColorAshTextColorSecondary));
      due_date_label->SetEnabledColorId(kColorAshTextColorSecondary);
    }
  }

  if (task->has_subtasks) {
    // TODO(b/294681832): Finalize, and then localize strings.
    details.push_back(u"Has subtasks");
    views::ImageView* has_subtask_icon_view =
        tasks_details_view_->AddChildView(std::make_unique<views::ImageView>());
    has_subtask_icon_view->SetProperty(views::kMarginsKey, kSubtaskIconMargin);
    if (chromeos::features::IsJellyEnabled()) {
      has_subtask_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
          kGlanceablesSubtaskIcon, cros_tokens::kCrosSysOnSurfaceVariant));
    } else {
      has_subtask_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
          kGlanceablesSubtaskIcon, kColorAshTextColorSecondary));
    }
  }

  // Use different margins depending on the number of
  // rows of text shown.
  const bool double_row = tasks_details_view_->children().size() > 0;
  contents_view_->SetProperty(views::kMarginsKey, double_row
                                                      ? kDoubleRowTextMargins
                                                      : kSingleRowTextMargins);
  button_->SetProperty(views::kMarginsKey, double_row ? kDoubleRowButtonMargin
                                                      : kSingleRowButtonMargin);

  button_->SetAccessibleDescription(base::UTF8ToUTF16(task->title) + u", " +
                                    base::JoinString(details, u", "));
  button_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

GlanceablesTaskView::~GlanceablesTaskView() = default;

void GlanceablesTaskView::ButtonPressed() {
  if (button_->checked()) {
    return;
  }

  // Visually mark the task as completed.
  button_->SetChecked(true);

  ash::Shell::Get()
      ->glanceables_v2_controller()
      ->GetTasksClient()
      ->MarkAsCompleted(task_list_id_, task_id_,
                        base::BindOnce(&GlanceablesTaskView::MarkedAsCompleted,
                                       weak_ptr_factory_.GetWeakPtr()));
}

const views::ImageButton* GlanceablesTaskView::GetButtonForTest() const {
  return button_;
}

bool GlanceablesTaskView::GetCompletedForTest() const {
  return button_->checked();
}

void GlanceablesTaskView::MarkedAsCompleted(bool success) {
  if (!success) {
    SetupTasksLabel(/*completed=*/false);
  }

  // Uncheck button if the tasks is not successfully marked as completed.
  button_->SetChecked(success);
}

void GlanceablesTaskView::SetupTasksLabel(bool completed) {
  if (completed) {
    tasks_label_->SetFontList(
        TypographyProvider::Get()
            ->ResolveTypographyToken(TypographyToken::kCrosButton2)
            .DeriveWithStyle(gfx::Font::FontStyle::STRIKE_THROUGH));
    if (chromeos::features::IsJellyEnabled()) {
      tasks_label_->SetEnabledColorId(
          static_cast<ui::ColorId>(cros_tokens::kCrosSysSecondary));
    } else {
      tasks_label_->SetEnabledColorId(kColorAshTextColorSecondary);
    }
  } else {
    tasks_label_->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosButton2));
    if (chromeos::features::IsJellyEnabled()) {
      tasks_label_->SetEnabledColorId(
          static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface));
    } else {
      tasks_label_->SetEnabledColorId(kColorAshTextColorPrimary);
    }
  }
}

BEGIN_METADATA(GlanceablesTaskView, views::View)
END_METADATA

}  // namespace ash
