// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/time/time_view_utils.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Margins between containers in the detailed view.
constexpr auto kContainerMargins = gfx::Insets::TLBR(2, 0, 0, 0);

// Insets for items within the `toggle_view_`'s `TriView` container.
constexpr auto kToggleViewInsets = gfx::Insets::VH(13, 24);

// Margins between children in the `toggle_view_`.
constexpr int kToggleViewBetweenChildSpacing = 16;

// Creates the appropriately formatted string to display for the time remaining
// display in the detailed view. When focus mode is active, this function
// returns a string reading the hours and minutes remaining in the session, with
// hours removed if their value is equal to 0. For example, if there are 10
// minutes remaining in an active focus session, the string returned will be "10
// min" as opposed to "0 hr, 10 min". On the other hand, if focus mode is
// inactive, only the minutes of the currently set session duration will be
// returned.
std::u16string CreateTimeRemainingString() {
  auto* controller = FocusModeController::Get();
  CHECK(controller);

  const base::Time now = base::Time::Now();
  const base::TimeDelta session_duration_remaining =
      controller->in_focus_session() ? controller->end_time() - now
                                     : controller->session_duration();
  // `FocusModeController::end_time_` is only calculated when the focus
  // session is started. Thus, if focus mode is not active, we can find this
  // end time by adding the focus mode controller's session duration to the
  // current time.
  const base::Time end_time = now + session_duration_remaining;
  const std::u16string time_string = focus_mode_util::GetDurationString(
      session_duration_remaining,
      focus_mode_util::TimeFormatType::kMinutesOnly);
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_TIME_SUBLABEL, time_string,
      focus_mode_util::GetFormattedClockString(end_time));
}

}  // namespace

FocusModeDetailedView::FocusModeDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  // TODO(b/288975135): update with official string.
  CreateTitleRow(IDS_ASH_STATUS_TRAY_FOCUS_MODE);
  CreateScrollableList();

  CreateToggleView();

  // TODO(b/286931575): remove border inset and add Timer UI.
  timer_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kNotRounded));
  timer_view_->SetBorderInsets(gfx::Insets::VH(56, 0));
  timer_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  // TODO(b/286931806): remove border inset and add Focus Scene UI.
  scene_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kNotRounded));
  scene_view_->SetBorderInsets(gfx::Insets::VH(100, 0));
  scene_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  CreateDoNotDisturbContainer();

  scroll_content()->SizeToPreferredSize();

  FocusModeController* focus_mode_controller = FocusModeController::Get();
  if (!focus_mode_controller->in_focus_session()) {
    StartClockTimer();
  }

  focus_mode_controller->AddObserver(this);
  message_center::MessageCenter::Get()->AddObserver(this);
}

FocusModeDetailedView::~FocusModeDetailedView() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
  FocusModeController::Get()->RemoveObserver(this);
}

void FocusModeDetailedView::OnQuietModeChanged(bool in_quiet_mode) {
  // When focus mode is not in a session, the state of the
  // `do_not_disturb_toggle_button_` will represent the initial state for the
  // next focus session. Once the focus mode session begins, this button should
  // be reflective of the actual system do not disturb state.
  if (FocusModeController::Get()->in_focus_session()) {
    do_not_disturb_toggle_button_->SetIsOn(in_quiet_mode);
  }
}

void FocusModeDetailedView::OnFocusModeChanged(bool in_focus_session) {
  // TODO(b/302194469): centralize bubble-closing logic.
  if (in_focus_session) {
    // Close the system tray bubble. Deletes `this`.
    CloseBubble();
    return;
  }

  toggle_view_->text_label()->SetText(l10n_util::GetStringUTF16(
      in_focus_session ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_ACTIVE_LABEL
                       : IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  toggle_view_->SetSubText(CreateTimeRemainingString());
  views::AsViewClass<PillButton>(toggle_view_->right_view())
      ->SetText(l10n_util::GetStringUTF16(
          in_focus_session
              ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON
              : IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_START_BUTTON));

  if (in_focus_session) {
    clock_timer_.Stop();
  } else {
    StartClockTimer();
  }

  do_not_disturb_toggle_button_->SetIsOn(
      FocusModeController::Get()->turn_on_do_not_disturb());
}

void FocusModeDetailedView::OnTimerTick() {
  toggle_view_->SetSubText(CreateTimeRemainingString());
}

void FocusModeDetailedView::CreateToggleView() {
  RoundedContainer* toggle_container =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kTopRounded));

  // `RoundedContainer` adds extra insets, so we need to remove those.
  toggle_container->SetBorderInsets(gfx::Insets());
  toggle_view_ = toggle_container->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));

  FocusModeController* focus_mode_controller = FocusModeController::Get();
  const bool in_focus_session = focus_mode_controller->in_focus_session();
  toggle_view_->AddIconAndLabel(
      ui::ImageModel::FromVectorIcon(kFocusModeLampIcon),
      l10n_util::GetStringUTF16(
          in_focus_session ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_ACTIVE_LABEL
                           : IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  toggle_view_->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                        *toggle_view_->text_label());

  toggle_view_->SetSubText(CreateTimeRemainingString());
  toggle_view_->sub_text_label()->SetEnabledColorId(
      cros_tokens::kCrosSysSecondary);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosAnnotation1,
                                        *toggle_view_->sub_text_label());

  toggle_view_->AddRightView(
      std::make_unique<PillButton>(
          base::BindRepeating(&FocusModeController::ToggleFocusMode,
                              base::Unretained(focus_mode_controller)),
          l10n_util::GetStringUTF16(
              in_focus_session
                  ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON
                  : IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_START_BUTTON),
          PillButton::Type::kPrimaryWithoutIcon, /*icon=*/nullptr)
          .release());

  toggle_view_->SetExpandable(true);
  toggle_view_->tri_view()->SetInsets(kToggleViewInsets);
  views::BoxLayout* toggle_view_tri_view_layout =
      toggle_view_->tri_view()->box_layout();
  toggle_view_tri_view_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  toggle_view_tri_view_layout->set_between_child_spacing(
      kToggleViewBetweenChildSpacing);
  toggle_view_tri_view_layout->InvalidateLayout();
}

void FocusModeDetailedView::CreateDoNotDisturbContainer() {
  do_not_disturb_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));
  do_not_disturb_view_->SetProperty(views::kMarginsKey, kContainerMargins);

  HoverHighlightView* toggle_row = do_not_disturb_view_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  toggle_row->SetFocusBehavior(View::FocusBehavior::NEVER);

  // Create the do not disturb icon and its label.
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemTrayDoNotDisturbIcon, cros_tokens::kCrosSysOnSurface));
  toggle_row->AddViewAndLabel(
      std::move(icon),
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE_DO_NOT_DISTURB));
  toggle_row->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton1,
                                        *toggle_row->text_label());

  // Create the toggle button for do not disturb.
  auto toggle = std::make_unique<Switch>(
      base::BindRepeating(&FocusModeDetailedView::OnDoNotDisturbToggleClicked,
                          weak_factory_.GetWeakPtr()));
  toggle->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DO_NOT_DISTURB));
  auto* controller = FocusModeController::Get();

  // The state of the toggle button is used for showing whether the
  // do-not-disturb mode is on/off on the device while in a focus session.
  // However, if there is no focus session running, it's used for representing
  // if the user wants to turn on/off the do not disturb when the next focus
  // session is started.
  toggle->SetIsOn(controller->in_focus_session()
                      ? message_center::MessageCenter::Get()->IsQuietMode()
                      : controller->turn_on_do_not_disturb());
  do_not_disturb_toggle_button_ = toggle.get();
  toggle_row->AddRightView(toggle.release());

  // TODO(hongyulong): Add insets for the tri_view of the toggle row.
  toggle_row->SetExpandable(true);
}

void FocusModeDetailedView::OnDoNotDisturbToggleClicked() {
  auto* controller = FocusModeController::Get();
  const bool is_on = do_not_disturb_toggle_button_->GetIsOn();
  if (controller->in_focus_session()) {
    message_center::MessageCenter::Get()->SetQuietMode(is_on);
  } else {
    controller->set_turn_on_do_not_disturb(is_on);
  }
}

void FocusModeDetailedView::OnClockMinutePassed() {
  if (FocusModeController::Get()->in_focus_session()) {
    return;
  }

  // When a clock minute passes outside of focus mode, we want to update the
  // subheading to display the correct session end time and restart the clock
  // timer. If we are in focus mode, then `FocusModeController::end_time()` will
  // tell us the time at which the session will end.
  toggle_view_->SetSubText(CreateTimeRemainingString());
  StartClockTimer();
}

void FocusModeDetailedView::StartClockTimer() {
  clock_timer_.Start(
      FROM_HERE,
      time_view_utils::GetTimeRemainingToNextMinute(base::Time::Now()), this,
      &FocusModeDetailedView::OnClockMinutePassed);
}

BEGIN_METADATA(FocusModeDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash
