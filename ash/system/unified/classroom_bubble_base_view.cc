// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/classroom_bubble_base_view.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/common/glanceables_error_message_view.h"
#include "ash/glanceables/common/glanceables_list_footer_view.h"
#include "ash/glanceables/common/glanceables_progress_bar_view.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/combobox.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/combobox_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr int kInteriorGlanceableBubbleMargin = 16;
constexpr auto kHeaderIconButtonMargins = gfx::Insets::TLBR(0, 0, 0, 4);
constexpr size_t kMaxAssignments = 3;

constexpr char kClassroomHomePage[] = "https://classroom.google.com/u/0/h";

}  // namespace

ClassroomBubbleBaseView::ClassroomBubbleBaseView(
    std::unique_ptr<ui::ComboboxModel> combobox_model)
    : GlanceableTrayChildBubble(/*for_glanceables_container=*/true) {
  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_
      ->SetInteriorMargin(gfx::Insets::TLBR(kInteriorGlanceableBubbleMargin,
                                            kInteriorGlanceableBubbleMargin, 0,
                                            kInteriorGlanceableBubbleMargin))
      .SetOrientation(views::LayoutOrientation::kVertical);

  header_view_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  header_view_->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_view_->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred));

  auto* const header_icon =
      header_view_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&ClassroomBubbleBaseView::OnHeaderIconPressed,
                              base::Unretained(this)),
          IconButton::Type::kMedium, &kGlanceablesClassroomIcon,
          IDS_GLANCEABLES_CLASSROOM_HEADER_ICON_ACCESSIBLE_NAME));
  header_icon->SetBackgroundColor(cros_tokens::kCrosSysBaseElevated);
  header_icon->SetProperty(views::kMarginsKey, kHeaderIconButtonMargins);
  header_icon->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleHeaderIcon));

  combo_box_view_ = header_view_->AddChildView(
      std::make_unique<Combobox>(std::move(combobox_model)));
  combo_box_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleComboBox));
  combo_box_view_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_GLANCEABLES_CLASSROOM_DROPDOWN_ACCESSIBLE_NAME));
  combo_box_view_->SetAccessibleDescription(u"");
  combobox_view_observation_.Observe(combo_box_view_);

  progress_bar_ = AddChildView(std::make_unique<GlanceablesProgressBarView>());
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  list_container_view_ = AddChildView(std::make_unique<views::View>());
  list_container_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListContainer));
  auto* layout =
      list_container_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_between_child_spacing(2);
  list_container_view_->SetAccessibleRole(ax::mojom::Role::kList);

  const auto* const typography_provider = TypographyProvider::Get();
  empty_list_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetProperty(views::kMarginsKey, gfx::Insets::TLBR(24, 0, 32, 0))
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetFontList(typography_provider->ResolveTypographyToken(
              TypographyToken::kCrosButton2))
          .SetLineHeight(typography_provider->ResolveLineHeight(
              TypographyToken::kCrosButton2))
          .SetID(base::to_underlying(
              GlanceablesViewId::kClassroomBubbleEmptyListLabel))
          .Build());

  list_footer_view_ = AddChildView(std::make_unique<GlanceablesListFooterView>(
      l10n_util::GetStringUTF16(
          IDS_GLANCEABLES_CLASSROOM_SEE_ALL_BUTTON_ACCESSIBLE_NAME),
      base::BindRepeating(&ClassroomBubbleBaseView::OnSeeAllPressed,
                          base::Unretained(this))));
  list_footer_view_->SetID(
      base::to_underlying(GlanceablesViewId::kClassroomBubbleListFooter));
}

ClassroomBubbleBaseView::~ClassroomBubbleBaseView() = default;

void ClassroomBubbleBaseView::OnViewFocused(views::View* view) {
  CHECK_EQ(view, combo_box_view_);

  AnnounceListStateOnComboBoxAccessibility();
}

void ClassroomBubbleBaseView::CancelUpdates() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void ClassroomBubbleBaseView::AboutToRequestAssignments() {
  assignments_requested_time_ = base::TimeTicks::Now();
  progress_bar_->UpdateProgressBarVisibility(/*visible=*/true);
  combo_box_view_->SetAccessibleDescription(u"");
}

void ClassroomBubbleBaseView::OnGetAssignments(
    const std::u16string& list_name,
    bool initial_update,
    bool success,
    std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments) {
  const gfx::Size old_preferred_size = GetPreferredSize();

  progress_bar_->UpdateProgressBarVisibility(/*visible=*/false);

  list_container_view_->RemoveAllChildViews();
  total_assignments_ = assignments.size();

  const size_t num_assignments = std::min(kMaxAssignments, assignments.size());
  for (size_t i = 0; i < num_assignments; ++i) {
    list_container_view_->AddChildView(
        std::make_unique<GlanceablesClassroomItemView>(
            assignments[i].get(),
            base::BindRepeating(&ClassroomBubbleBaseView::OnItemViewPressed,
                                base::Unretained(this), initial_update,
                                assignments[i]->link),
            /*item_index=*/i, /*last_item_index=*/num_assignments - 1));
  }
  const size_t shown_assignments = list_container_view_->children().size();
  list_footer_view_->UpdateItemsCount(shown_assignments, total_assignments_);

  const bool is_list_empty = shown_assignments == 0;
  empty_list_label_->SetVisible(is_list_empty);
  list_footer_view_->SetVisible(!is_list_empty);

  list_container_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_GLANCEABLES_CLASSROOM_SELECTED_LIST_ACCESSIBLE_NAME, list_name));
  list_container_view_->SetAccessibleDescription(
      list_footer_view_->items_count_label());
  list_container_view_->NotifyAccessibilityEvent(
      ax::mojom::Event::kChildrenChanged,
      /*send_native_event=*/true);

  // The list is shown in response to the action on the assignment selector
  // combobox, notify the user of the list state id the combox is still focused.
  AnnounceListStateOnComboBoxAccessibility();

  if (old_preferred_size != GetPreferredSize()) {
    PreferredSizeChanged();

    if (!initial_update) {
      GetWidget()->LayoutRootViewIfNecessary();
      ScrollViewToVisible();
    }
  }

  auto* controller = Shell::Get()->glanceables_controller();

  if (initial_update) {
    RecordClassromInitialLoadTime(
        /* first_occurrence=*/controller->bubble_shown_count() == 1,
        base::TimeTicks::Now() - controller->last_bubble_show_time());
  } else {
    RecordClassroomChangeLoadTime(
        success, base::TimeTicks::Now() - assignments_requested_time_);
  }

  list_shown_start_time_ = base::TimeTicks::Now();
  first_assignment_list_shown_ = true;

  if (features::IsGlanceablesV2ErrorMessageEnabled()) {
    if (success) {
      MaybeDismissErrorMessage();
    } else {
      ShowErrorMessage(
          l10n_util::GetStringUTF16(IDS_GLANCEABLES_CLASSROOM_FETCH_ERROR));

      // Explicitly signal to the layout manager to ignore the view.
      layout_manager_->SetChildViewIgnoredByLayout(error_message(),
                                                   /*ignored=*/true);
    }
  }
}

void ClassroomBubbleBaseView::OpenUrl(const GURL& url) const {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      url, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void ClassroomBubbleBaseView::AnnounceListStateOnComboBoxAccessibility() {
  if (empty_list_label_->GetVisible()) {
    combo_box_view_->GetViewAccessibility().AnnounceText(
        empty_list_label_->GetText());
  } else if (list_footer_view_->items_count_label()->GetVisible()) {
    combo_box_view_->GetViewAccessibility().AnnounceText(
        list_footer_view_->items_count_label()->GetText());
  }
}

void ClassroomBubbleBaseView::OnItemViewPressed(bool initial_list_selected,
                                                const GURL& url) {
  if (initial_list_selected) {
    base::RecordAction(base::UserMetricsAction(
        "Glanceables_Classroom_AssignmentPressed_DefaultList"));
  }
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_AssignmentPressed"));
  OpenUrl(url);
}

void ClassroomBubbleBaseView::OnHeaderIconPressed() {
  base::RecordAction(
      base::UserMetricsAction("Glanceables_Classroom_HeaderIconPressed"));
  OpenUrl(GURL(kClassroomHomePage));
}

BEGIN_METADATA(ClassroomBubbleBaseView, views::View)
END_METADATA

}  // namespace ash
