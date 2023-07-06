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
#include "ash/style/typography.h"
#include "ash/system/time/date_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace {

constexpr int kIconSize = 20;
constexpr int kTaskHeight = 48;
constexpr int kTaskWidth = 332;
constexpr char kFormatterPattern[] = "EEE, MMM d";  // "Wed, Feb 28"

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

GlanceablesTaskView::GlanceablesTaskView(const std::string& task_list_id,
                                         const GlanceablesTask* task)
    : task_list_id_(task_list_id), task_id_(task->id) {
  button_ =
      AddChildView(std::make_unique<views::ImageButton>(base::BindRepeating(
          &GlanceablesTaskView::ButtonPressed, base::Unretained(this))));
  button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kHollowCircleIcon,
                                     cros_tokens::kFocusRingColor, kIconSize));
  // TODO(b:277268122): set accessible name once spec is available.
  button_->SetAccessibleName(u"Glanceables Task View Button");

  contents_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  contents_view_->SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  contents_view_->SetOrientation(views::LayoutOrientation::kVertical);

  tasks_title_view_ =
      contents_view_->AddChildView(std::make_unique<views::FlexLayoutView>());

  views::Label* tasks_label = SetupLabel(tasks_title_view_);
  tasks_label->SetText(base::UTF8ToUTF16(task->title));
  tasks_label->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosButton2));
  if (chromeos::features::IsJellyEnabled()) {
    tasks_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  } else {
    tasks_label->SetEnabledColorId(kColorAshTextColorPrimary);
  }

  if (task->due.has_value()) {
    tasks_due_date_view_ =
        contents_view_->AddChildView(std::make_unique<views::FlexLayoutView>());
    views::ImageView* time_icon_view = tasks_due_date_view_->AddChildView(
        std::make_unique<views::ImageView>());

    views::Label* due_date_label = SetupLabel(tasks_due_date_view_);
    due_date_label->SetText(GetFormattedDueDate(task->due.value()));
    due_date_label->SetFontList(
        TypographyProvider::Get()->ResolveTypographyToken(
            TypographyToken::kCrosAnnotation1));

    if (chromeos::features::IsJellyEnabled()) {
      time_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
          kGlanceablesTasksDueDateIcon, cros_tokens::kCrosSysOnSurfaceVariant,
          kIconSize));
      due_date_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
    } else {
      time_icon_view->SetImage(ui::ImageModel::FromVectorIcon(
          kGlanceablesTasksDueDateIcon, kColorAshTextColorSecondary,
          kIconSize));
      due_date_label->SetEnabledColorId(kColorAshTextColorSecondary);
    }
  }

  // TODO(b:277268122): Implement accessibility behavior.
  SetAccessibleRole(ax::mojom::Role::kListBox);
  SetAccessibleName(u"Glanceables Task View Accessible Name");
}

GlanceablesTaskView::~GlanceablesTaskView() = default;

void GlanceablesTaskView::ButtonPressed() {
  ash::Shell::Get()
      ->glanceables_v2_controller()
      ->GetTasksClient()
      ->MarkAsCompleted(task_list_id_, task_id_,
                        base::BindOnce(&GlanceablesTaskView::MarkedAsCompleted,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void GlanceablesTaskView::MarkedAsCompleted(bool success) {
  if (!success) {
    return;
  }
  completed_ = true;
  // TODO(b:277268122): Update icons and styling.
  button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kHollowCheckCircleIcon,
                                     cros_tokens::kFocusRingColor, kIconSize));
}

gfx::Size GlanceablesTaskView::CalculatePreferredSize() const {
  return gfx::Size(kTaskWidth, kTaskHeight);
}

BEGIN_METADATA(GlanceablesTaskView, views::View)
END_METADATA

}  // namespace ash
