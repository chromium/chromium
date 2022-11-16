// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_up_next_view.h"

#include <memory>

#include "ash/bubble/bubble_utils.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/time/calendar_event_list_item_view_jelly.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/metadata/view_factory_internal.h"

namespace ash {
namespace {

constexpr int kContainerInsets = 12;
constexpr int kBackgroundRadius = 12;
constexpr int kBetweenChildSpacing = 8;
constexpr int kMaxEventListItemWidth = 160;

views::Builder<views::Label> CreateHeaderLabel() {
  return views::Builder<views::Label>(bubble_utils::CreateLabel(
      bubble_utils::TypographyStyle::kButton2,
      l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_UP_NEXT)));
}

}  // namespace

CalendarUpNextView::CalendarUpNextView(
    CalendarViewController* calendar_view_controller)
    : calendar_view_controller_(calendar_view_controller),
      header_view_(AddChildView(std::make_unique<views::View>())),
      scroll_view_(AddChildView(std::make_unique<views::ScrollView>(
          views::ScrollView::ScrollWithLayers::kEnabled))),
      content_view_(
          scroll_view_->SetContents(std::make_unique<views::View>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(kContainerInsets),
      kBetweenChildSpacing));

  header_view_->SetLayoutManager(std::make_unique<views::FlexLayout>());
  header_view_->AddChildView(CreateHeaderLabel().Build());

  scroll_view_->SetAllowKeyboardScrolling(false);
  scroll_view_->SetBackgroundColor(absl::nullopt);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);
  scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);

  content_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kBetweenChildSpacing));

  UpdateEvents();
}

CalendarUpNextView::~CalendarUpNextView() = default;

void CalendarUpNextView::Layout() {
  // For some reason the `content_view_` is constrained to the `scroll_view_`
  // width and so it isn't scrollable. This seems to be a problem with
  // horizontal `ScrollView`s as this doesn't happen if you make this view
  // vertically scrollable. To make the content scrollable, we need to set it's
  // preferred size here so it's bigger than the `scroll_view_` and
  // therefore scrolls.
  if (content_view_)
    content_view_->SizeToPreferredSize();

  // `content_view_` is a child of this class so we need to Layout after
  // changing its width.
  views::View::Layout();
}

void CalendarUpNextView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(views::CreateRoundedRectBackground(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBase),
      kBackgroundRadius));
}

void CalendarUpNextView::UpdateEvents() {
  content_view_->RemoveAllChildViews();

  std::list<google_apis::calendar::CalendarEvent> events =
      calendar_view_controller_->UpcomingEvents();

  auto now = base::Time::NowFromSystemTime();
  for (auto& event : events) {
    content_view_->AddChildView(
        std::make_unique<CalendarEventListItemViewJelly>(
            calendar_view_controller_,
            SelectedDateParams{now, now.UTCMidnight(), now.LocalMidnight()},
            /*event=*/event, /*round_top_corners=*/true,
            /*round_bottom_corners=*/true,
            /*max_width=*/kMaxEventListItemWidth));
  }
}

BEGIN_METADATA(CalendarUpNextView, views::View);
END_METADATA

}  // namespace ash
