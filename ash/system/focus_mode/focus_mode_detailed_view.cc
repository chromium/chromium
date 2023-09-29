// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/time/time_view_utils.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/time_formatting.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

namespace ash {

namespace {

// Margins between containers in the detailed view.
constexpr auto kContainerMargins = gfx::Insets::TLBR(2, 0, 0, 0);

// Insets for items within the `toggle_view_`'s `TriView` container.
constexpr auto kToggleViewInsets = gfx::Insets::VH(13, 24);

// Margins between children in the `toggle_view_`.
constexpr int kToggleViewBetweenChildSpacing = 16;

// Constants for the `timer_setting_view_`.
constexpr auto kTimerViewBorderInsets = gfx::Insets::VH(8, 0);
constexpr auto kTimerViewHeaderInsets = gfx::Insets::VH(10, 24);
constexpr auto kTimerSettingViewInsets = gfx::Insets::TLBR(8, 16, 12, 16);
constexpr int kTimerSettingViewMaxCharacters = 3;
constexpr int kTimerSettingViewTextHeight = 32;
constexpr int kTimerSettingViewBetweenChildSpacing = 8;
constexpr auto kTimerAdjustmentButtonSize = gfx::Size(63, 36);

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

// Creates an `IconButton` with the formatting needed for the
// `timer_setting_view_`'s timer adjustment buttons.
std::unique_ptr<IconButton> CreateTimerAdjustmentButton(
    views::Button::PressedCallback callback,
    const gfx::VectorIcon& icon,
    ui::ColorId background_color,
    int accessible_name_id) {
  std::unique_ptr<IconButton> timer_adjustment_button =
      std::make_unique<IconButton>(callback, IconButton::Type::kLarge, &icon,
                                   accessible_name_id);
  timer_adjustment_button->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  timer_adjustment_button->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  timer_adjustment_button->SetPreferredSize(kTimerAdjustmentButtonSize);
  timer_adjustment_button->SetIconColorId(cros_tokens::kCrosSysOnSurface);
  timer_adjustment_button->SetBackground(
      views::CreateThemedRoundedRectBackground(
          background_color, kTimerAdjustmentButtonSize.height() / 2, 0));
  return timer_adjustment_button;
}

// Gives us the amount of time by which we should increment or decrement the
// current session duration.
int GetDurationDelta(int duration, bool decrement) {
  const int direction = decrement ? -1 : 1;

  // If the duration is at 5 or below, we can decrement by 1. But we can only
  // increment by 1 if the duration is below 5.
  if ((!decrement && duration < 5) || (decrement && duration <= 5)) {
    return direction;
  }

  // Likewise, if the duration is at 60 or below, we decrement to the nearest
  // multiple of 5. But we can only increment to the nearest multiple of 5 if
  // the duration is under 60.
  if ((!decrement && duration < 60) || (decrement && duration <= 60)) {
    const int duration_remainder = duration % 5;

    if (duration_remainder == 0) {
      return direction * 5;
    }

    return direction *
           (decrement ? duration_remainder : 5 - duration_remainder);
  }

  // Everything else increments to the nearest multiple of 15.
  const int duration_remainder = duration % 15;

  if (duration_remainder == 0) {
    return direction * 15;
  }

  return direction * (decrement ? duration_remainder : 15 - duration_remainder);
}

// Tells us what the current session duration would be after an increment or
// decrement.
base::TimeDelta CalculateSessionDurationAfterAdjustment(int duration,
                                                        bool decrement) {
  duration += GetDurationDelta(duration, decrement);
  return std::clamp(base::Minutes(duration), focus_mode_util::kMinimumDuration,
                    focus_mode_util::kMaximumDuration);
}

}  // namespace

// Handles input validation and events for the textfield in
// `timer_setting_view_`.
class FocusModeDetailedView::TimerTextfieldController
    : public SystemTextfieldController,
      public views::ViewObserver {
 public:
  TimerTextfieldController(SystemTextfield* textfield,
                           FocusModeDetailedView* owner)
      : SystemTextfieldController(textfield),
        textfield_(textfield),
        owner_(owner) {
    textfield_->AddObserver(this);
  }
  TimerTextfieldController(const TimerTextfieldController&) = delete;
  TimerTextfieldController& operator=(const TimerTextfieldController&) = delete;
  ~TimerTextfieldController() override { textfield_->RemoveObserver(this); }

  // SystemTextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    RefreshTextfieldSize(new_contents);
  }

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::ET_KEY_PRESSED) {
      return false;
    }

    views::FocusManager* focus_manager = sender->GetWidget()->GetFocusManager();

    if (key_event.key_code() == ui::VKEY_RETURN) {
      if (sender->GetText().length() == 0) {
        textfield_->RestoreText();
        // `RestoreText()`, uses `SetText()`, which does not invoke
        // `ContentsChanged()`. Call `ContentsChanged()` directly, so the text
        // change gets handled by controller overrides.
        ContentsChanged(sender, sender->GetText());
        textfield_->SetActive(false);
      }

      focus_manager->ClearFocus();

      // Avoid having the focus restored to the same view when the parent view
      // is refocused.
      focus_manager->SetStoredFocusView(nullptr);
      return true;
    }

    if (SystemTextfieldController::HandleKeyEvent(sender, key_event)) {
      if (key_event.key_code() == ui::VKEY_ESCAPE) {
        focus_manager->ClearFocus();
      }

      return true;
    }

    // Skip non-numeric characters.
    const char16_t character = key_event.GetCharacter();
    if (std::isprint(character) && !std::isdigit(character)) {
      return true;
    }

    // We check selected range because if it is not empty then the user is
    // highlighting text that will be replaced with the input character.
    if (std::isdigit(character) &&
        sender->GetText().length() == kTimerSettingViewMaxCharacters &&
        sender->GetSelectedRange().is_empty()) {
      return true;
    }

    return false;
  }

  // views::ViewObserver:
  void OnViewBlurred(views::View* view) override {
    owner_->SetInactiveSessionDuration(base::Minutes(
        focus_mode_util::GetTimerTextfieldInputInMinutes(textfield_)));
  }

  // Recalculates and sets the size of the textfield to fit the input contents.
  void RefreshTextfieldSize(const std::u16string& contents) {
    int width = 0;
    int height = 0;
    gfx::Canvas::SizeStringInt(contents, textfield_->GetFontList(), &width,
                               &height, 0, gfx::Canvas::NO_ELLIPSIS);
    textfield_->SetPreferredSize(
        gfx::Size(width + textfield_->GetCaretBounds().width() +
                      textfield_->GetInsets().width(),
                  kTimerSettingViewTextHeight));
  }

 private:
  const raw_ptr<SystemTextfield> textfield_ = nullptr;

  // The owning `FocusModeDetailedView`.
  const raw_ptr<FocusModeDetailedView> owner_ = nullptr;
};

FocusModeDetailedView::FocusModeDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  // TODO(b/288975135): update with official string.
  CreateTitleRow(IDS_ASH_STATUS_TRAY_FOCUS_MODE);
  CreateScrollableList();

  CreateToggleView();

  CreateTimerView();

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

void FocusModeDetailedView::AddedToWidget() {
  // The `TrayBubbleView` is not normally activatable. To make the textfield in
  // this view activatable, we need to tell the bubble that it can be activated.
  // The `TrayBubbleView` may not exist in unit tests.
  if (views::WidgetDelegate* bubble_view = GetWidget()->widget_delegate()) {
    bubble_view->SetCanActivate(true);
  }
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

  if (!in_focus_session && !timer_setting_view_) {
    CreateTimerSettingView();
  } else {
    timer_setting_view_->SetVisible(!in_focus_session);
  }

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

void FocusModeDetailedView::CreateTimerView() {
  timer_view_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kNotRounded));
  timer_view_container_->SetProperty(views::kMarginsKey, kContainerMargins);
  timer_view_container_->SetBorderInsets(kTimerViewBorderInsets);

  auto timer_view_header = std::make_unique<views::Label>();
  timer_view_header->SetText(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_SUBHEADER));
  timer_view_header->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  timer_view_header->SetBorder(
      views::CreateEmptyBorder(kTimerViewHeaderInsets));
  timer_view_header->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                        *timer_view_header);
  timer_view_container_->AddChildView(std::move(timer_view_header));

  if (!FocusModeController::Get()->in_focus_session()) {
    CreateTimerSettingView();
  }
}

void FocusModeDetailedView::CreateTimerSettingView() {
  timer_setting_view_ = timer_view_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  timer_setting_view_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  timer_setting_view_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  timer_setting_view_->SetInsideBorderInsets(kTimerSettingViewInsets);
  timer_setting_view_->SetBetweenChildSpacing(
      kTimerSettingViewBetweenChildSpacing);

  // `SystemTextfield` does not currently confirm text when the user clicks
  // outside of the textfield but within the textfield's parent. See
  // b/302038651.
  timer_textfield_ = timer_setting_view_->AddChildView(
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kLarge));
  timer_textfield_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(
          TypographyToken::kCrosDisplay6Regular));
  std::u16string default_time_text = base::NumberToString16(
      FocusModeController::Get()->session_duration().InMinutes());
  timer_textfield_->SetText(default_time_text);
  timer_textfield_controller_ =
      std::make_unique<TimerTextfieldController>(timer_textfield_, this);
  timer_textfield_controller_->RefreshTextfieldSize(default_time_text);
  timer_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_TEXTFIELD));

  views::Label* minutes_label = timer_setting_view_->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_MINUTES_LABEL)));
  minutes_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosDisplay6Regular,
                                        *minutes_label);
  timer_setting_view_->SetFlexForView(minutes_label, 1);

  // The minutes label ignores the between child spacing on its left side so
  // that it can be directly next to the textfield.
  minutes_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, -1 * kTimerSettingViewBetweenChildSpacing, 0, 0));

  // TODO(b/302196478): Make increment and decrement buttons disabled
  // when increment or decrement limit is reached.
  timer_setting_view_->AddChildView(CreateTimerAdjustmentButton(
      base::BindRepeating(&FocusModeDetailedView::AdjustInactiveSessionDuration,
                          base::Unretained(this),
                          /*decrement=*/true),
      kChevronDownIcon, cros_tokens::kCrosSysBaseElevated,
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_DECREMENT_BUTTON));

  timer_setting_view_->AddChildView(CreateTimerAdjustmentButton(
      base::BindRepeating(&FocusModeDetailedView::AdjustInactiveSessionDuration,
                          base::Unretained(this),
                          /*decrement=*/false),
      kChevronUpIcon, cros_tokens::kCrosSysHighlightShape,
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_INCREMENT_BUTTON));
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

void FocusModeDetailedView::AdjustInactiveSessionDuration(bool decrement) {
  FocusModeController* focus_mode_controller = FocusModeController::Get();
  CHECK(!focus_mode_controller->in_focus_session());

  SetInactiveSessionDuration(CalculateSessionDurationAfterAdjustment(
      focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield_),
      decrement));
}

void FocusModeDetailedView::OnInactiveSessionDurationChanged() {
  FocusModeController* focus_mode_controller = FocusModeController::Get();
  CHECK(!focus_mode_controller->in_focus_session());
  toggle_view_->SetSubText(CreateTimeRemainingString());
  std::u16string new_session_duration_string = base::NumberToString16(
      focus_mode_controller->session_duration().InMinutes());
  timer_textfield_->SetText(new_session_duration_string);
  timer_textfield_controller_->RefreshTextfieldSize(
      new_session_duration_string);
}

void FocusModeDetailedView::SetInactiveSessionDuration(
    base::TimeDelta duration) {
  FocusModeController::Get()->SetSessionDuration(duration);
  OnInactiveSessionDurationChanged();
}

BEGIN_METADATA(FocusModeDetailedView, TrayDetailedView)
END_METADATA

}  // namespace ash
