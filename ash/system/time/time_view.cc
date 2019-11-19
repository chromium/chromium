// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_view.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace tray {
namespace {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

// Padding between the left edge of the shelf and the left edge of the vertical
// clock.
const int kVerticalClockLeftPadding = 9;

// Offset used to bring the minutes line closer to the hours line in the
// vertical clock.
const int kVerticalClockMinutesTopOffset = -2;

// Leading padding used to draw the tray background to the left of the clock
// when the shelf is vertically aligned.
const int kClockLeadingPadding = 8;

}  // namespace

TimeView::TimeView(ClockLayout clock_layout, ClockModel* model)
    : ActionableView(TrayPopupInkDropStyle::INSET_BOUNDS), model_(model) {
  SetTimer(base::Time::Now());
  SetFocusBehavior(FocusBehavior::NEVER);
  model_->AddObserver(this);
  SetupLabels();
  UpdateTextInternal(base::Time::Now());
  UpdateClockLayout(clock_layout);
}

TimeView::~TimeView() {
  model_->RemoveObserver(this);
  timer_.Stop();
}

void TimeView::UpdateClockLayout(ClockLayout clock_layout) {
  // Do nothing if the layout hasn't changed.
  if (((clock_layout == ClockLayout::HORIZONTAL_CLOCK) ? horizontal_label_
                                                       : vertical_label_hours_)
          ->parent() == this)
    return;

  SetBorder(views::NullBorder());
  if (clock_layout == ClockLayout::HORIZONTAL_CLOCK) {
    RemoveChildView(vertical_label_hours_.get());
    RemoveChildView(vertical_label_minutes_.get());
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(horizontal_label_.get());
  } else {
    RemoveChildView(horizontal_label_.get());
    // Remove the current layout manager since it could be the FillLayout which
    // only allows one child.
    SetLayoutManager(nullptr);
    // Pre-add the children since ownership is being retained by this.
    AddChildView(vertical_label_hours_.get());
    AddChildView(vertical_label_minutes_.get());
    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());
    const int kColumnId = 0;
    views::ColumnSet* columns = layout->AddColumnSet(kColumnId);
    columns->AddPaddingColumn(0, kVerticalClockLeftPadding);
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);
    layout->AddPaddingRow(0, kClockLeadingPadding);
    layout->StartRow(0, kColumnId);
    // Add the views as existing since ownership isn't being transferred.
    layout->AddExistingView(vertical_label_hours_.get());
    layout->StartRow(0, kColumnId);
    layout->AddExistingView(vertical_label_minutes_.get());
    layout->AddPaddingRow(0, kVerticalClockMinutesTopOffset);
  }
  Layout();
}

void TimeView::SetTextColorBasedOnSession(
    session_manager::SessionState session_state) {
  auto set_color = [&](std::unique_ptr<views::Label>& label) {
    label->SetEnabledColor(TrayIconColor(session_state));
  };

  set_color(horizontal_label_);
  set_color(vertical_label_hours_);
  set_color(vertical_label_minutes_);
}

void TimeView::OnDateFormatChanged() {
  UpdateTimeFormat();
}

void TimeView::OnSystemClockTimeUpdated() {
  UpdateTimeFormat();
}

void TimeView::OnSystemClockCanSetTimeChanged(bool can_set_time) {}

void TimeView::Refresh() {
  UpdateText();
}

base::HourClockType TimeView::GetHourTypeForTesting() const {
  return model_->hour_clock_type();
}

const char* TimeView::GetClassName() const {
  return "TimeView";
}

bool TimeView::PerformAction(const ui::Event& event) {
  return false;
}

void TimeView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kTime;
}

void TimeView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

bool TimeView::OnMousePressed(const ui::MouseEvent& event) {
  // Let the event fall through.
  return false;
}

void TimeView::OnGestureEvent(ui::GestureEvent* event) {
  // Skip gesture handling happening in Button so that the container views
  // receive and handle them properly.
  // TODO(mohsen): Refactor TimeView/DateView classes so that they are not
  // ActionableView anymore. Create an ActionableView as a container for when
  // needed.
}

void TimeView::UpdateText() {
  base::Time now = base::Time::Now();
  UpdateTextInternal(now);
  SchedulePaint();
  SetTimer(now);
}

void TimeView::UpdateTimeFormat() {
  UpdateText();
}

void TimeView::UpdateTextInternal(const base::Time& now) {
  // Just in case |now| is null, do NOT update time; otherwise, it will
  // crash icu code by calling into base::TimeFormatTimeOfDayWithHourClockType,
  // see details in crbug.com/147570.
  if (now.is_null()) {
    LOG(ERROR) << "Received null value from base::Time |now| in argument";
    return;
  }

  SetAccessibleName(base::TimeFormatTimeOfDayWithHourClockType(
                        now, model_->hour_clock_type(), base::kKeepAmPm) +
                    base::ASCIIToUTF16(", ") +
                    base::TimeFormatFriendlyDate(now));

  NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);

  base::string16 current_time = base::TimeFormatTimeOfDayWithHourClockType(
      now, model_->hour_clock_type(), base::kDropAmPm);
  horizontal_label_->SetText(current_time);
  horizontal_label_->SetTooltipText(base::TimeFormatFriendlyDate(now));
  horizontal_label_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                                              true);

  // Calculate vertical clock layout labels.
  size_t colon_pos = current_time.find(base::ASCIIToUTF16(":"));
  base::string16 hour = current_time.substr(0, colon_pos);
  base::string16 minute = current_time.substr(colon_pos + 1);

  // Sometimes pad single-digit hours with a zero for aesthetic reasons.
  if (hour.length() == 1 && model_->hour_clock_type() == base::k24HourClock &&
      !base::i18n::IsRTL())
    hour = base::ASCIIToUTF16("0") + hour;

  vertical_label_hours_->SetText(hour);
  vertical_label_minutes_->SetText(minute);
  vertical_label_hours_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
  vertical_label_minutes_->NotifyAccessibilityEvent(
      ax::mojom::Event::kTextChanged, true);
  Layout();
}

void TimeView::SetupLabels() {
  horizontal_label_.reset(new views::Label());
  SetupLabel(horizontal_label_.get());
  vertical_label_hours_.reset(new views::Label());
  SetupLabel(vertical_label_hours_.get());
  vertical_label_minutes_.reset(new views::Label());
  SetupLabel(vertical_label_minutes_.get());
  // Pull the minutes up closer to the hours by using a negative top border.
  vertical_label_minutes_->SetBorder(
      views::CreateEmptyBorder(kVerticalClockMinutesTopOffset, 0, 0, 0));
}

void TimeView::SetupLabel(views::Label* label) {
  label->set_owned_by_client();
  SetupLabelForTray(label);
  label->SetElideBehavior(gfx::NO_ELIDE);
}

void TimeView::SetTimer(const base::Time& now) {
  // Try to set the timer to go off at the next change of the minute. We don't
  // want to have the timer go off more than necessary since that will cause
  // the CPU to wake up and consume power.
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  // Often this will be called at minute boundaries, and we'll actually want
  // 60 seconds from now.
  int seconds_left = 60 - exploded.second;
  if (seconds_left == 0)
    seconds_left = 60;

  // Make sure that the timer fires on the next minute. Without this, if it is
  // called just a teeny bit early, then it will skip the next minute.
  seconds_left += kTimerSlopSeconds;

  timer_.Stop();
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(seconds_left), this,
               &TimeView::UpdateText);
}

}  // namespace tray
}  // namespace ash
