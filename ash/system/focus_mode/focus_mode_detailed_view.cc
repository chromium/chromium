// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/focus_mode/focus_mode_detailed_view.h"

#include <memory>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "ash/style/typography.h"
#include "ash/system/focus_mode/focus_mode_animations.h"
#include "ash/system/focus_mode/focus_mode_controller.h"
#include "ash/system/focus_mode/focus_mode_countdown_view.h"
#include "ash/system/focus_mode/focus_mode_task_view.h"
#include "ash/system/focus_mode/focus_mode_util.h"
#include "ash/system/focus_mode/sounds/focus_mode_sounds_view.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/time/time_view_utils.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tri_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "base/check_op.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Margins between containers in the detailed view if the container is connected
// to the container above it.
constexpr auto kConnectedContainerMargins = gfx::Insets::TLBR(2, 0, 0, 0);

// Margins between containers in the detailed view if the container is not
// connected to the container above it.
constexpr auto kDisconnectedContainerMargins = gfx::Insets::TLBR(8, 0, 0, 0);

// Insets for items within the `toggle_view_`'s `TriView` container.
constexpr auto kToggleViewInsets = gfx::Insets::VH(5, 24);

constexpr auto kToggleViewHeight = 64;

// Timer view constants.
constexpr auto kTimerViewBorderInsets = gfx::Insets::TLBR(4, 0, 8, 8);
constexpr auto kTimerViewHeaderInsets = gfx::Insets::TLBR(14, 24, 10, 24);
constexpr auto kTimerSettingViewInsets = gfx::Insets::TLBR(0, 16, 12, 16);
constexpr int kTimerSettingViewMaxCharacters = 3;
constexpr int kTimerSettingViewTextHeight = 32;
constexpr int kTimerSettingViewBetweenChildSpacing = 8;
constexpr auto kTimerAdjustmentButtonSize = gfx::Size(63, 36);
constexpr auto kTimerCountdownViewInsets = gfx::Insets::TLBR(0, 24, 12, 16);
constexpr int kTimerTextfieldCornerRadius = 8;

// Task view constants.
constexpr auto kTaskViewContainerInsets = gfx::Insets::TLBR(4, 24, 22, 24);
constexpr auto kTaskViewHeaderInsets = gfx::Insets::VH(18, 0);

// Feedback button constants.
constexpr auto kFeedbackButtonInsets = gfx::Insets::TLBR(16, 0, 4, 0);
constexpr auto kFeedbackButtonPadding = gfx::Insets::VH(6, 12);
constexpr int kFeedbackButtonIconTextSpacing = 8;

constexpr int kToggleButtonLeftPadding = 8;

// Gives us the amount of time by which we should increment or decrement the
// current session duration. A negative result indicates a decrement, and
// positive an increment.
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

// Creates an `IconButton` with the formatting needed for the
// `timer_setting_view_`'s timer adjustment buttons.
std::unique_ptr<IconButton> CreateTimerAdjustmentButton(
    views::Button::PressedCallback callback,
    bool decrement) {
  const int accessible_name_id =
      decrement ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_DECREMENT_BUTTON
                : IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_INCREMENT_BUTTON;
  std::unique_ptr<IconButton> timer_adjustment_button =
      std::make_unique<IconButton>(
          std::move(callback), IconButton::Type::kLarge,
          decrement ? &kChevronDownIcon : &kChevronUpIcon, accessible_name_id);
  timer_adjustment_button->SetTooltipText(l10n_util::GetStringFUTF16(
      accessible_name_id,
      base::NumberToString16(std::abs(GetDurationDelta(
          FocusModeController::Get()->session_duration().InMinutes(),
          decrement)))));
  timer_adjustment_button->SetImageHorizontalAlignment(
      views::ImageButton::HorizontalAlignment::ALIGN_CENTER);
  timer_adjustment_button->SetImageVerticalAlignment(
      views::ImageButton::VerticalAlignment::ALIGN_MIDDLE);
  timer_adjustment_button->SetPreferredSize(kTimerAdjustmentButtonSize);
  timer_adjustment_button->SetIconColor(cros_tokens::kCrosSysOnSurface);
  timer_adjustment_button->SetBackgroundColor(
      cros_tokens::kCrosSysHighlightShape);
  return timer_adjustment_button;
}

// Tells us what the current session duration would be after an increment or
// decrement.
base::TimeDelta CalculateSessionDurationAfterAdjustment(int duration,
                                                        bool decrement) {
  duration += GetDurationDelta(duration, decrement);
  return std::clamp(base::Minutes(duration), focus_mode_util::kMinimumDuration,
                    focus_mode_util::kMaximumDuration);
}

// Returns true if `contents` is constructed by a number with no more than 3
// digits or by an empty string.
bool IsValidTimeNumber(const std::u16string& contents) {
  const int length = contents.length();
  if (length > kTimerSettingViewMaxCharacters) {
    return false;
  }

  // Check the character from the tail, because each time when the user inserts
  // a new char, `ContentsChanged` will be called.
  for (int i = contents.length() - 1; i >= 0; --i) {
    if (!std::isdigit(contents[i])) {
      return false;
    }
  }
  return true;
}

std::u16string GetAccessibleNameForDndButton(bool do_not_disturb_enabled) {
  return l10n_util::GetStringUTF16(
      do_not_disturb_enabled
          ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_DO_NOT_DISTURB_ACCESSIBLE_NAME_ENABLED
          : IDS_ASH_STATUS_TRAY_FOCUS_MODE_DO_NOT_DISTURB_ACCESSIBLE_NAME_DISABLED);
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
    // If there are more than 3 chars inserted from the virtual keyboard, or if
    // a non-numeric character is inserted from the virtual keyboard, set the
    // `textfield_` with the last known valid contents before `new_contents`
    // came in.
    if (!IsValidTimeNumber(new_contents)) {
      textfield_->SetText(valid_new_contents_);
      return;
    }

    valid_new_contents_ = new_contents;
    RefreshTextfieldSize(new_contents);
  }

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::ET_KEY_PRESSED) {
      return false;
    }

    if (key_event.key_code() == ui::VKEY_PROCESSKEY) {
      // Default handling for keyboard events that are not generated by physical
      // key press. This can happen, for example, when virtual keyboard button
      // is pressed.
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

      owner_->SetInactiveSessionDuration(base::Minutes(
          focus_mode_util::GetTimerTextfieldInputInMinutes(textfield_)));

      focus_manager->ClearFocus();

      // Avoid having the focus restored to the same view when the parent view
      // is refocused.
      focus_manager->SetStoredFocusView(nullptr);
      return true;
    }

    // Make sure that we set the timer adjustment buttons' enabled states before
    // moving focus to avoid recursively focusing.
    if (key_event.key_code() == ui::VKEY_TAB) {
      owner_->SetInactiveSessionDuration(base::Minutes(
          focus_mode_util::GetTimerTextfieldInputInMinutes(textfield_)));
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
  void OnViewFocused(views::View* view) override {
    valid_new_contents_ = textfield_->GetText();
  }

  void OnViewBlurred(views::View* view) override {
    valid_new_contents_.clear();
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

  // The valid contents (only digits and no more than 3 chars) showing in the
  // textfield while the `textfield_` in an editing state.
  std::u16string valid_new_contents_ = std::u16string();

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

  CreateTaskView();

  scroll_content()->AddChildView(std::make_unique<FocusModeSoundsView>());

  FocusModeController* focus_mode_controller = FocusModeController::Get();
  const bool in_focus_session = focus_mode_controller->in_focus_session();

  CreateDoNotDisturbContainer();
  do_not_disturb_view_->SetVisible(!in_focus_session);

  CreateFeedbackButton();

  scroll_content()->SizeToPreferredSize();
  if (!in_focus_session) {
    StartClockTimer();
  }

  focus_mode_controller->AddObserver(this);
  task_view_container_->AddObserver(this);
  Shell::Get()->system_tray_model()->clock()->AddObserver(this);
}

FocusModeDetailedView::~FocusModeDetailedView() {
  Shell::Get()->system_tray_model()->clock()->RemoveObserver(this);
  task_view_container_->RemoveObserver(this);
  FocusModeController::Get()->RemoveObserver(this);
}

void FocusModeDetailedView::OnViewBoundsChanged(views::View* observed_view) {
  DCHECK_EQ(task_view_container_, observed_view);

  const int old_height = task_view_container_height_;
  task_view_container_height_ = task_view_container_->bounds().height();
  // Skip the animations during the first time the user opens the
  // `FocusModeDetailedView`.
  const int shift_height = old_height - task_view_container_height_;
  if (old_height == 0) {
    return;
  }
  PerformTaskContainerViewResizeAnimation(task_view_container_->layer(),
                                          old_height);
  OnTaskViewAnimate(shift_height);
}

void FocusModeDetailedView::OnDateFormatChanged() {
  UpdateEndTimeLabel();
}

void FocusModeDetailedView::OnSystemClockTimeUpdated() {
  UpdateEndTimeLabel();
}

void FocusModeDetailedView::OnSystemClockCanSetTimeChanged(bool can_set_time) {
  UpdateEndTimeLabel();
}

void FocusModeDetailedView::Refresh() {
  UpdateEndTimeLabel();
}

void FocusModeDetailedView::AddedToWidget() {
  // The `TrayBubbleView` is not normally activatable. To make the textfield in
  // this view activatable, we need to tell the bubble that it can be activated.
  // The `TrayBubbleView` may not exist in unit tests.
  if (views::WidgetDelegate* bubble_view = GetWidget()->widget_delegate()) {
    bubble_view->SetCanActivate(true);
  }
}

void FocusModeDetailedView::OnFocusModeChanged(bool in_focus_session) {
  if (in_focus_session) {
    // The system tray bubble is closed by the `FocusModeController` whenever
    // we toggle focus mode on, so do nothing here.
    return;
  }

  toggle_view_->text_label()->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  if (toggle_view_->sub_text_label()) {
    toggle_view_->sub_text_label()->SetVisible(false);
  }
  views::AsViewClass<PillButton>(toggle_view_->right_view())
      ->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_START_BUTTON));
  UpdateToggleButtonAccessibility(in_focus_session);

  UpdateTimerView(false);

  StartClockTimer();
  do_not_disturb_view_->SetVisible(true);
}

void FocusModeDetailedView::OnTimerTick(
    const FocusModeSession::Snapshot& session_snapshot) {
  timer_countdown_view_->UpdateUI(session_snapshot);
}

void FocusModeDetailedView::OnActiveSessionDurationChanged(
    const FocusModeSession::Snapshot& session_snapshot) {
  if (session_snapshot.state != FocusModeSession::State::kOn) {
    return;
  }

  toggle_view_->SetSubText(focus_mode_util::GetFormattedEndTimeString(
      FocusModeController::Get()->GetActualEndTime()));
  timer_countdown_view_->UpdateUI(session_snapshot);
  UpdateToggleButtonAccessibility(/*in_focus_session=*/true);
}

void FocusModeDetailedView::CreateToggleView() {
  RoundedContainer* toggle_container =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kTopRounded));

  // `RoundedContainer` adds extra insets, so we need to remove those.
  toggle_container->SetBorderInsets(gfx::Insets());
  toggle_view_ = toggle_container->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  toggle_view_->SetPreferredSize(gfx::Size(0, kToggleViewHeight));
  views::InkDrop::Get(toggle_view_)
      ->SetMode(views::InkDropHost::InkDropMode::OFF);

  FocusModeController* focus_mode_controller = FocusModeController::Get();
  const bool in_focus_session = focus_mode_controller->in_focus_session();
  toggle_view_->AddIconAndLabel(
      ui::ImageModel::FromVectorIcon(kFocusModeLampIcon,
                                     cros_tokens::kCrosSysOnSurface),
      l10n_util::GetStringUTF16(
          in_focus_session ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_ACTIVE_LABEL
                           : IDS_ASH_STATUS_TRAY_FOCUS_MODE));
  toggle_view_->text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                        *toggle_view_->text_label());

  // As part of the first time user flow, if the user has never started a
  // session before, we want to provide description text.
  if (!focus_mode_controller->HasStartedSessionBefore()) {
    toggle_view_->SetSubText(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_FIRST_TIME_SUBLABEL));
  }

  if (in_focus_session) {
    toggle_view_->SetSubText(focus_mode_util::GetFormattedEndTimeString(
        focus_mode_controller->GetActualEndTime()));
    toggle_view_->sub_text_label()->SetEnabledColorId(
        cros_tokens::kCrosSysSecondary);
    TypographyProvider::Get()->StyleLabel(
        ash::TypographyToken::kCrosAnnotation1,
        *toggle_view_->sub_text_label());
  }

  toggle_view_->AddRightView(
      std::make_unique<PillButton>(
          base::BindRepeating(
              &FocusModeController::ToggleFocusMode,
              base::Unretained(focus_mode_controller),
              focus_mode_histogram_names::ToggleSource::kFocusPanel),
          l10n_util::GetStringUTF16(
              in_focus_session
                  ? IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON
                  : IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_START_BUTTON),
          PillButton::Type::kPrimaryLargeWithoutIcon, /*icon=*/nullptr)
          .release());
  UpdateToggleButtonAccessibility(in_focus_session);

  toggle_view_->SetFocusBehavior(View::FocusBehavior::NEVER);
  toggle_view_->tri_view()->SetInsets(kToggleViewInsets);
  views::BoxLayout* toggle_view_tri_view_layout =
      toggle_view_->tri_view()->box_layout();
  toggle_view_tri_view_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

void FocusModeDetailedView::UpdateToggleButtonAccessibility(
    bool in_focus_session) {
  auto* toggle_button =
      views::AsViewClass<PillButton>(toggle_view_->right_view());

  if (!in_focus_session) {
    toggle_button->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_START_BUTTON_ACCESSIBLE_NAME));
    toggle_button->SetTooltipText(toggle_button->GetAccessibleName());
    return;
  }

  const std::u16string duration_string = focus_mode_util::GetDurationString(
      FocusModeController::Get()->GetActualEndTime() - base::Time::Now(),
      /*digital_format=*/false);
  toggle_button->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TOGGLE_END_BUTTON_ACCESSIBLE_NAME,
      duration_string));
  toggle_button->SetTooltipText(toggle_button->GetAccessibleName());
}

void FocusModeDetailedView::CreateTimerView() {
  // Create the timer view container.
  timer_view_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kBottomRounded));
  timer_view_container_->SetProperty(views::kMarginsKey,
                                     kConnectedContainerMargins);
  timer_view_container_->SetBorderInsets(kTimerViewBorderInsets);

  // Create the timer header.
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

  // Create the countdown view.
  timer_countdown_view_ = timer_view_container_->AddChildView(
      std::make_unique<FocusModeCountdownView>(/*include_end_button=*/false));
  timer_countdown_view_->SetBorder(
      views::CreateEmptyBorder(kTimerCountdownViewInsets));

  // Create the timer setting view.
  timer_setting_view_ = timer_view_container_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  timer_setting_view_->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  timer_setting_view_->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  timer_setting_view_->SetInsideBorderInsets(kTimerSettingViewInsets);
  timer_setting_view_->SetBetweenChildSpacing(
      kTimerSettingViewBetweenChildSpacing);

  // Add a container for the textfield, the minutes label, and the "Until"
  // label.
  auto* end_time_container = timer_setting_view_->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  end_time_container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  end_time_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Add a container for the timer textfield and the minutes label.
  auto* textfield_container = end_time_container->AddChildView(
      std::make_unique<views::BoxLayoutView>());
  textfield_container->SetOrientation(
      views::BoxLayout::Orientation::kHorizontal);
  textfield_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  textfield_container->SetBetweenChildSpacing(
      kTimerSettingViewBetweenChildSpacing);

  // `SystemTextfield` does not currently confirm text when the user clicks
  // outside of the textfield but within the textfield's parent. See
  // b/302038651.
  timer_textfield_ = textfield_container->AddChildView(
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kLarge));
  timer_textfield_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  timer_textfield_->SetFontList(
      TypographyProvider::Get()->ResolveTypographyToken(
          TypographyToken::kCrosDisplay6Regular));
  timer_textfield_controller_ =
      std::make_unique<TimerTextfieldController>(timer_textfield_, this);
  timer_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_TEXTFIELD));
  timer_textfield_->SetActiveStateChangedCallback(base::BindRepeating(
      &FocusModeDetailedView::HandleTextfieldActivationChange,
      weak_factory_.GetWeakPtr()));
  auto* focus_ring = views::FocusRing::Get(timer_textfield_);
  DCHECK(focus_ring);
  // Override the default focus ring gap of `SystemTextfield` to let it not
  // intersect with `end_time_label_`.
  focus_ring->SetHaloInset(0);
  // Override the rounded highlight path set in `SystemTextfield` to keep it the
  // same as the corner radius for the task textfield.
  views::InstallRoundRectHighlightPathGenerator(timer_textfield_, gfx::Insets(),
                                                kTimerTextfieldCornerRadius);

  views::Label* minutes_label = textfield_container->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_FOCUS_MODE_MINUTES_LABEL)));
  minutes_label->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosDisplay6Regular,
                                        *minutes_label);
  timer_setting_view_->SetFlexForView(end_time_container, 1);

  // The minutes label ignores the between child spacing on its left side so
  // that it can be directly next to the textfield.
  minutes_label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(0, -1 * kTimerSettingViewBetweenChildSpacing, 0, 0));

  end_time_label_ =
      end_time_container->AddChildView(std::make_unique<views::Label>());
  end_time_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                        *end_time_label_);
  end_time_label_->SetEnabledColorId(cros_tokens::kCrosSysSecondary);
  end_time_label_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(0, kTimerSettingViewBetweenChildSpacing)));

  timer_decrement_button_ =
      timer_setting_view_->AddChildView(CreateTimerAdjustmentButton(
          base::BindRepeating(
              &FocusModeDetailedView::AdjustInactiveSessionDuration,
              base::Unretained(this),
              /*decrement=*/true),
          /*decrement=*/true));
  views::InstallRoundRectHighlightPathGenerator(
      timer_decrement_button_, gfx::Insets(),
      kTimerAdjustmentButtonSize.height() / 2);
  views::InkDrop::Get(timer_decrement_button_)
      ->SetMode(views::InkDropHost::InkDropMode::OFF);

  timer_increment_button_ =
      timer_setting_view_->AddChildView(CreateTimerAdjustmentButton(
          base::BindRepeating(
              &FocusModeDetailedView::AdjustInactiveSessionDuration,
              base::Unretained(this),
              /*decrement=*/false),
          /*decrement=*/false));
  views::InstallRoundRectHighlightPathGenerator(
      timer_increment_button_, gfx::Insets(),
      kTimerAdjustmentButtonSize.height() / 2);
  views::InkDrop::Get(timer_increment_button_)
      ->SetMode(views::InkDropHost::InkDropMode::OFF);

  UpdateTimerView(FocusModeController::Get()->in_focus_session());
}

void FocusModeDetailedView::UpdateTimerView(bool in_focus_session) {
  CHECK(timer_setting_view_ && timer_countdown_view_);
  timer_setting_view_->SetVisible(!in_focus_session);
  timer_countdown_view_->SetVisible(in_focus_session);

  if (in_focus_session) {
    timer_countdown_view_->UpdateUI(
        FocusModeController::Get()->current_session()->GetSnapshot(
            base::Time::Now()));
  } else {
    UpdateTimerSettingViewUI();
  }
}

void FocusModeDetailedView::HandleTextfieldActivationChange() {
  if (!timer_textfield_->IsActive() && timer_textfield_->HasFocus()) {
    auto* focus_manager = timer_textfield_->GetWidget()->GetFocusManager();
    focus_manager->ClearFocus();
    focus_manager->SetStoredFocusView(nullptr);

    // TODO(b/322863087): Remove the call of `UpdateBackground` for
    // timer_textfield_ after the bug resolved. The reason for calling it can be
    // found from the description of the bug.
    timer_textfield_->UpdateBackground();

    // Once we clear the focus for the `timer_textfield_`, we need to call the
    // function below manually to update the UI according to the latest session
    // duration, since the `OnViewBlurred` for the textfield controller doesn't
    // automatically call it to avoid the bug b/315358227.
    SetInactiveSessionDuration(base::Minutes(
        focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield_)));
  }
}

void FocusModeDetailedView::CreateTaskView() {
  task_view_container_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kAllRounded));
  task_view_container_->SetProperty(views::kMarginsKey,
                                    kDisconnectedContainerMargins);
  task_view_container_->SetBorderInsets(kTaskViewContainerInsets);
  task_view_container_->SetPaintToLayer();
  task_view_container_->layer()->SetFillsBoundsOpaquely(false);

  // Create the task header.
  auto* task_view_header =
      task_view_container_->AddChildView(std::make_unique<views::Label>());
  task_view_header->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_FOCUS_MODE_TASK_SUBHEADER));
  task_view_header->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  task_view_header->SetBorder(views::CreateEmptyBorder(kTaskViewHeaderInsets));
  task_view_header->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                        *task_view_header);

  // Create the focus mode task view.
  focus_mode_task_view_ =
      task_view_container_->AddChildView(std::make_unique<FocusModeTaskView>());
}

void FocusModeDetailedView::OnTaskViewAnimate(const int shift_height) {
  std::vector<views::View*> animatable_views;

  // Currently, we only have the `do_not_disturb_view_` below the task view
  // container. We only need to insert a new added view into this map in future.
  if (do_not_disturb_view_->GetVisible()) {
    animatable_views.push_back(do_not_disturb_view_);
  }

  if (animatable_views.empty()) {
    return;
  }
  PerformViewsVerticalShitfAnimation(animatable_views, shift_height);
}

void FocusModeDetailedView::CreateDoNotDisturbContainer() {
  do_not_disturb_view_ =
      scroll_content()->AddChildView(std::make_unique<RoundedContainer>(
          RoundedContainer::Behavior::kAllRounded));
  do_not_disturb_view_->SetProperty(views::kMarginsKey,
                                    kDisconnectedContainerMargins);
  // `RoundedContainer` adds extra insets, so we need to remove those.
  do_not_disturb_view_->SetBorderInsets(gfx::Insets());

  HoverHighlightView* toggle_row = do_not_disturb_view_->AddChildView(
      std::make_unique<HoverHighlightView>(/*listener=*/this));
  toggle_row->SetFocusBehavior(View::FocusBehavior::NEVER);
  toggle_row->SetPreferredSize(gfx::Size(0, kToggleViewHeight));
  views::InkDrop::Get(toggle_row)
      ->SetMode(views::InkDropHost::InkDropMode::OFF);

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
  auto* controller = FocusModeController::Get();
  const bool do_not_disturb_enabled = controller->turn_on_do_not_disturb();
  toggle->SetAccessibleName(
      GetAccessibleNameForDndButton(do_not_disturb_enabled));
  toggle->SetIsOn(do_not_disturb_enabled);
  do_not_disturb_toggle_button_ = toggle.get();
  toggle_row->AddRightView(toggle.release(),
                           views::CreateEmptyBorder(gfx::Insets::TLBR(
                               0, kToggleButtonLeftPadding, 0, 0)));

  toggle_row->SetExpandable(true);
  toggle_row->tri_view()->SetInsets(kToggleViewInsets);
  views::BoxLayout* toggle_view_tri_view_layout =
      toggle_row->tri_view()->box_layout();
  toggle_view_tri_view_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

void FocusModeDetailedView::OnDoNotDisturbToggleClicked() {
  auto* controller = FocusModeController::Get();
  CHECK(!controller->in_focus_session());

  controller->set_turn_on_do_not_disturb(
      do_not_disturb_toggle_button_->GetIsOn());

  do_not_disturb_toggle_button_->SetAccessibleName(
      GetAccessibleNameForDndButton(controller->turn_on_do_not_disturb()));
}

void FocusModeDetailedView::CreateFeedbackButton() {
  auto* button_container =
      scroll_content()->AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  button_container->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  button_container->SetInsideBorderInsets(kFeedbackButtonInsets);

  auto* feedback_button =
      button_container->AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&FocusModeDetailedView::OnFeedbackButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_FOCUS_MODE_FEEDBACK_BUTTON),
          PillButton::Type::kFloatingWithIconLeading));
  feedback_button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kFeedbackIcon,
                                     cros_tokens::kCrosSysSecondary));
  feedback_button->SetEnabledTextColorIds(kColorAshTextColorSecondary);
  feedback_button->SetImageLabelSpacing(kFeedbackButtonIconTextSpacing);
  feedback_button->SetBorder(views::CreateEmptyBorder(kFeedbackButtonPadding));

  views::InkDropHost* const ink_drop = views::InkDrop::Get(feedback_button);
  ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop->GetInkDrop()->SetShowHighlightOnHover(true);
  ink_drop->SetVisibleOpacity(1.0f);
  ink_drop->SetBaseColorId(cros_tokens::kButtonBackgroundColorSecondaryHover);
}

void FocusModeDetailedView::OnFeedbackButtonPressed() {
  Shell::Get()->shell_delegate()->OpenFeedbackDialog(
      ShellDelegate::FeedbackSource::kFocusMode,
      /*description_template=*/"#FocusMode",
      /*category_tag=*/std::string());
}

void FocusModeDetailedView::OnClockMinutePassed() {
  if (FocusModeController::Get()->in_focus_session()) {
    UpdateToggleButtonAccessibility(/*in_focus_session=*/true);
    return;
  }

  StartClockTimer();

  // If the user is still setting the timer, we should not always update the UI
  // (for example, if a clock minute passes while the textfield is focused).
  if (timer_textfield_->HasFocus()) {
    return;
  }

  // When a clock minute passes outside of a focus session, we want to update
  // `end_time_label_` to display the correct session end time and restart the
  // clock timer. If we are in a focus session, then
  // `FocusModeController::GetEndTime()` will tell us the time at which the
  // session will end.
  UpdateEndTimeLabel();
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
  const base::TimeDelta adjusted_duration =
      CalculateSessionDurationAfterAdjustment(
          focus_mode_util::GetTimerTextfieldInputInMinutes(timer_textfield_),
          decrement);

  SetInactiveSessionDuration(adjusted_duration);

  // `GetDurationDelta` will return a negative number for a decrement, so we
  // take the absolute value to indicate a positive number of minutes to
  // decrement by.
  timer_decrement_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_DECREMENT_BUTTON,
      base::NumberToString16(std::abs(GetDurationDelta(
          adjusted_duration.InMinutes(), /*decrement=*/true)))));
  timer_increment_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_FOCUS_MODE_TIMER_INCREMENT_BUTTON,
      base::NumberToString16(GetDurationDelta(adjusted_duration.InMinutes(),
                                              /*decrement=*/false))));
}

void FocusModeDetailedView::UpdateTimerSettingViewUI() {
  // We always directly fetch `session_duration` here since the timer setting
  // view doesn't care about the durations that are adjusted during a focus
  // session.
  const base::TimeDelta session_duration =
      FocusModeController::Get()->session_duration();
  end_time_label_->SetText(focus_mode_util::GetFormattedEndTimeString(
      session_duration + base::Time::Now()));
  std::u16string new_session_duration_string =
      base::NumberToString16(session_duration.InMinutes());
  timer_textfield_->SetText(new_session_duration_string);
  timer_textfield_controller_->RefreshTextfieldSize(
      new_session_duration_string);

  timer_decrement_button_->SetEnabled(session_duration >
                                      focus_mode_util::kMinimumDuration);
  timer_increment_button_->SetEnabled(session_duration <
                                      focus_mode_util::kMaximumDuration);
}

void FocusModeDetailedView::SetInactiveSessionDuration(
    base::TimeDelta duration) {
  FocusModeController::Get()->SetInactiveSessionDuration(duration);
  UpdateTimerSettingViewUI();
}

void FocusModeDetailedView::UpdateEndTimeLabel() {
  FocusModeController* focus_mode_controller = FocusModeController::Get();
  if (focus_mode_controller->in_focus_session()) {
    toggle_view_->SetSubText(focus_mode_util::GetFormattedEndTimeString(
        focus_mode_controller->GetActualEndTime()));
  } else {
    end_time_label_->SetText(focus_mode_util::GetFormattedEndTimeString(
        focus_mode_controller->session_duration() + base::Time::Now()));
  }
}

BEGIN_METADATA(FocusModeDetailedView)
END_METADATA

}  // namespace ash
