// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/tasks/glanceables_task_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/date_helper.h"
#include "base/strings/string_util.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/font.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"

namespace ash {
namespace {

constexpr int kIconSize = 20;
constexpr char kFormatterPattern[] = "EEE, MMM d";  // "Wed, Feb 28"

constexpr int kBackgroundRadius = 4;
constexpr auto kSecondRowItemsMargin = gfx::Insets::TLBR(0, 0, 0, 4);

constexpr auto kSingleRowButtonMargin = gfx::Insets::VH(13, 18);
constexpr auto kDoubleRowButtonMargin = gfx::Insets::VH(16, 18);

constexpr auto kSingleRowTextMargins = gfx::Insets::TLBR(13, 0, 13, 16);
constexpr auto kDoubleRowTextMargins = gfx::Insets::TLBR(7, 0, 7, 16);

views::Label* SetupLabel(views::View* parent) {
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
  // Google Tasks API does not respect time portion of the date and always
  // returns "YYYY-MM-DDT00:00:00.000Z" format (see "due" field
  // https://developers.google.com/tasks/reference/rest/v1/tasks). Treating this
  // date in UTC format as is leads to showing one day less in timezones to the
  // west of UTC. The following line adjusts `due` so that it becomes a
  // **local** midnight instead.
  const auto adjusted_due = due - calendar_utils::GetTimeDifference(due);
  const auto midnight_today = base::Time::Now().LocalMidnight();
  const auto midnight_tomorrow = midnight_today + base::Days(1);

  if (midnight_today <= adjusted_due && adjusted_due < midnight_tomorrow) {
    return l10n_util::GetStringUTF16(IDS_GLANCEABLES_DUE_TODAY);
  }

  auto* const date_helper = DateHelper::GetInstance();
  CHECK(date_helper);
  const auto formatter =
      date_helper->CreateSimpleDateFormatter(kFormatterPattern);
  return date_helper->GetFormattedTime(&formatter, adjusted_due);
}

std::unique_ptr<views::ImageView> CreateSecondRowIcon(
    const gfx::VectorIcon& icon) {
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetProperty(views::kMarginsKey, kSecondRowItemsMargin);
  if (chromeos::features::IsJellyEnabled()) {
    icon_view->SetImage(ui::ImageModel::FromVectorIcon(
        icon, cros_tokens::kCrosSysOnSurfaceVariant));
  } else {
    icon_view->SetImage(
        ui::ImageModel::FromVectorIcon(icon, kColorAshTextColorSecondary));
  }
  return icon_view;
}

}  // namespace

class GlanceablesTaskView::CheckButton : public views::ImageButton {
 public:
  explicit CheckButton(PressedCallback pressed_callback)
      : views::ImageButton(std::move(pressed_callback)) {
    SetAccessibleRole(ax::mojom::Role::kCheckBox);
    UpdateImage();
    SetFlipCanvasOnPaintForRTLUI(/*enable=*/false);
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::ImageButton::GetAccessibleNodeData(node_data);

    node_data->SetName(l10n_util::GetStringUTF16(
        checked_
            ? IDS_GLANCEABLES_TASKS_TASK_ITEM_MARK_NOT_COMPLETED_ACCESSIBLE_NAME
            : IDS_GLANCEABLES_TASKS_TASK_ITEM_MARK_COMPLETED_ACCESSIBLE_NAME));

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
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  tasks_title_view_ =
      contents_view_->AddChildView(std::make_unique<views::BoxLayoutView>());
  tasks_details_view_ =
      contents_view_->AddChildView(std::make_unique<views::FlexLayoutView>());
  tasks_details_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  tasks_details_view_->SetOrientation(views::LayoutOrientation::kHorizontal);

  tasks_label_ = SetupLabel(tasks_title_view_);
  tasks_label_->SetText(base::UTF8ToUTF16(task->title));
  tasks_label_->SetLineHeight(TypographyProvider::Get()->ResolveLineHeight(
      TypographyToken::kCrosButton2));
  tasks_label_->SetID(
      base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel));
  SetupTasksLabel(/*completed=*/false);

  std::vector<std::u16string> details;
  if (task->due.has_value()) {
    tasks_details_view_->AddChildView(
        CreateSecondRowIcon(kGlanceablesTasksDueDateIcon));

    const auto formatted_due_date = GetFormattedDueDate(task->due.value());
    details.push_back(l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_HAS_DUE_DATE_ACCESSIBLE_DESCRIPTION,
        formatted_due_date));

    views::Label* due_date_label = SetupLabel(tasks_details_view_);
    due_date_label->SetText(formatted_due_date);
    due_date_label->SetID(
        base::to_underlying(GlanceablesViewId::kTaskItemDueLabel));
    due_date_label->SetProperty(views::kMarginsKey, kSecondRowItemsMargin);
    due_date_label->SetFontList(
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosAnnotation1));
    due_date_label->SetLineHeight(TypographyProvider::Get()->ResolveLineHeight(
        TypographyToken::kCrosAnnotation1));

    if (chromeos::features::IsJellyEnabled()) {
      due_date_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
    } else {
      due_date_label->SetEnabledColorId(kColorAshTextColorSecondary);
    }
  }

  if (task->has_subtasks) {
    details.push_back(l10n_util::GetStringUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_HAS_SUBTASK_ACCESSIBLE_DESCRIPTION));
    tasks_details_view_->AddChildView(
        CreateSecondRowIcon(kGlanceablesSubtaskIcon));
  }

  if (task->has_notes) {
    details.push_back(l10n_util::GetStringUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_HAS_DETAILS_ACCESSIBLE_DESCRIPTION));
    tasks_details_view_->AddChildView(
        CreateSecondRowIcon(kGlanceablesTasksNotesIcon));
  }

  // Use different margins depending on the number of
  // rows of text shown.
  const bool double_row = tasks_details_view_->children().size() > 0;
  contents_view_->SetProperty(views::kMarginsKey, double_row
                                                      ? kDoubleRowTextMargins
                                                      : kSingleRowTextMargins);
  button_->SetProperty(views::kMarginsKey, double_row ? kDoubleRowButtonMargin
                                                      : kSingleRowButtonMargin);

  auto a11y_description = base::UTF8ToUTF16(task->title);
  if (!details.empty()) {
    a11y_description += u". ";
    a11y_description += l10n_util::GetStringFUTF16(
        IDS_GLANCEABLES_TASKS_TASK_ITEM_METADATA_WRAPPER_ACCESSIBLE_DESCRIPTION,
        base::JoinString(details, u", "));
  }
  button_->SetAccessibleDescription(a11y_description);
  button_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

GlanceablesTaskView::~GlanceablesTaskView() = default;

void GlanceablesTaskView::ButtonPressed() {
  bool target_state = !button_->checked();
  // Visually mark the task as completed.
  button_->SetChecked(target_state);
  SetupTasksLabel(/*completed=*/target_state);
  RecordTaskMarkedAsCompleted(target_state);

  ash::Shell::Get()
      ->glanceables_v2_controller()
      ->GetTasksClient()
      ->MarkAsCompleted(task_list_id_, task_id_, /*completed=*/target_state);
}

const views::ImageButton* GlanceablesTaskView::GetButtonForTest() const {
  return button_;
}

bool GlanceablesTaskView::GetCompletedForTest() const {
  return button_->checked();
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
