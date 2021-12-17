// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "calendar_event_list_item_view.h"
#include "google_apis/calendar/calendar_api_response_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The paddings in `CalendarEventListView`.
constexpr gfx::Insets kContentInsets{0, 20};

}  // namespace

CalendarEventListView::CalendarEventListView(
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller),
      close_button_(AddChildView(std::make_unique<views::ImageButton>(
          views::Button::PressedCallback(base::BindRepeating(
              &CalendarViewController::CloseEventListView,
              base::Unretained(calendar_view_controller)))))),
      scroll_view_(AddChildView(std::make_unique<views::ScrollView>())),
      content_view_(
          scroll_view_->SetContents(std::make_unique<views::View>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  close_button_->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kIcCloseIcon,
                            calendar_utils::GetPrimaryTextColor()));
  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_RIGHT);
  close_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  close_button_->SetBorder(views::CreateEmptyBorder(kContentInsets));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_CLOSE_BUTTON_ACCESSIBLE_DESCRIPTION));
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_CLOSE_BUTTON_TOOLTIP));
  close_button_->SetFocusBehavior(FocusBehavior::ALWAYS);

  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->ClipHeightTo(0, INT_MAX);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  content_view_->SetBorder(views::CreateEmptyBorder(kContentInsets));

  UpdateListItems();

  scoped_calendar_view_controller_observer_.Observe(calendar_view_controller_);
}

CalendarEventListView::~CalendarEventListView() = default;

void CalendarEventListView::OnSelectedDateUpdated() {
  UpdateListItems();
}

void CalendarEventListView::UpdateListItems() {
  content_view_->RemoveAllChildViews();

  for (google_apis::calendar::CalendarEvent event :
       calendar_view_controller_->SelectedDateEvents()) {
    auto* event_entry = content_view_->AddChildView(
        std::make_unique<CalendarEventListItemView>(event));

    // Needs to repaint the `content_view_`'s children.
    event_entry->InvalidateLayout();
  }
}

BEGIN_METADATA(CalendarEventListView, views::View);
END_METADATA

}  // namespace ash
