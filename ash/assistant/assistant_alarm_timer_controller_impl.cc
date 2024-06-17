// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_alarm_timer_controller_impl.h"

#include <cmath>
#include <utility>

#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/assistant_notification_controller_impl.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/measfmt.h"
#include "third_party/icu/source/i18n/unicode/measunit.h"
#include "third_party/icu/source/i18n/unicode/measure.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using assistant::AssistantNotification;
using assistant::AssistantNotificationButton;
using assistant::AssistantNotificationPriority;
using assistant::AssistantTimer;
using assistant::AssistantTimerState;
using assistant::util::AlarmTimerAction;

// Grouping key and ID prefix for timer notifications.
constexpr char kTimerNotificationGroupingKey[] = "assistant/timer";
constexpr char kTimerNotificationIdPrefix[] = "assistant/timer";

// Helpers ---------------------------------------------------------------------

std::string ToFormattedTimeString(base::TimeDelta time,
                                  UMeasureFormatWidth width) {
  DCHECK(width == UMEASFMT_WIDTH_NARROW || width == UMEASFMT_WIDTH_NUMERIC);

  // Method aliases to prevent line-wrapping below.
  const auto createHour = icu::MeasureUnit::createHour;
  const auto createMinute = icu::MeasureUnit::createMinute;
  const auto createSecond = icu::MeasureUnit::createSecond;

  // We round |total_seconds| to the nearest full second since we don't display
  // our time string w/ millisecond granularity and because this method is
  // called very near to full second boundaries. Otherwise, values like 4.99 sec
  // would be displayed to the user as "0:04" instead of the expected "0:05".
  const int64_t total_seconds = std::abs(std::round(time.InSecondsF()));

  // Calculate time in hours/minutes/seconds.
  const int32_t hours = total_seconds / 3600;
  const int32_t minutes = (total_seconds - hours * 3600) / 60;
  const int32_t seconds = total_seconds % 60;

  // Success of the ICU APIs is tracked by |status|.
  UErrorCode status = U_ZERO_ERROR;

  // Create our distinct |measures| to be formatted.
  std::vector<icu::Measure> measures;

  // We only show |hours| if necessary.
  if (hours)
    measures.emplace_back(hours, createHour(status), status);

  // We only show |minutes| if necessary or if using numeric format |width|.
  if (minutes || width == UMEASFMT_WIDTH_NUMERIC)
    measures.emplace_back(minutes, createMinute(status), status);

  // We only show |seconds| if necessary or if using numeric format |width|.
  if (seconds || width == UMEASFMT_WIDTH_NUMERIC)
    measures.emplace_back(seconds, createSecond(status), status);

  // Format our |measures| into a |unicode_message|.
  icu::UnicodeString unicode_message;
  icu::FieldPosition field_position = icu::FieldPosition::DONT_CARE;
  icu::MeasureFormat measure_format(icu::Locale::getDefault(), width, status);
  measure_format.formatMeasures(measures.data(), measures.size(),
                                unicode_message, field_position, status);

  std::string formatted_time;
  if (U_SUCCESS(status)) {
    // If formatting was successful, convert our |unicode_message| into UTF-8.
    unicode_message.toUTF8String(formatted_time);
  } else {
    // If something went wrong formatting w/ ICU, fall back to I18N messages.
    LOG(ERROR) << "Error formatting time string: " << status;
    formatted_time =
        base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                width == UMEASFMT_WIDTH_NARROW
                    ? IDS_ASSISTANT_TIMER_NOTIFICATION_FORMATTED_TIME_NARROW_FALLBACK
                    : IDS_ASSISTANT_TIMER_NOTIFICATION_FORMATTED_TIME_NUMERIC_FALLBACK),
            hours, minutes, seconds));
  }

  // If necessary, negate the amount of time remaining.
  if (time.InSeconds() < 0) {
    formatted_time =
        base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                IDS_ASSISTANT_TIMER_NOTIFICATION_FORMATTED_TIME_NEGATE),
            formatted_time));
  }

  return formatted_time;
}

// Returns a string representation of the original duration for a given |timer|.
std::string ToOriginalDurationString(const AssistantTimer& timer) {
  return ToFormattedTimeString(timer.original_duration, UMEASFMT_WIDTH_NARROW);
}

// Returns a string representation of the remaining time for the given |timer|.
std::string ToRemainingTimeString(const AssistantTimer& timer) {
  return ToFormattedTimeString(timer.remaining_time, UMEASFMT_WIDTH_NUMERIC);
}

// Creates a notification ID for the given |timer|. It is guaranteed that this
// method will always return the same notification ID given the same timer.
std::string CreateTimerNotificationId(const AssistantTimer& timer) {
  return std::string(kTimerNotificationIdPrefix) + timer.id;
}

// Creates a notification title for the given |timer|.
std::string CreateTimerNotificationTitle(const AssistantTimer& timer) {
  return ToRemainingTimeString(timer);
}

// Creates a notification message for the given |timer|.
std::string CreateTimerNotificationMessage(const AssistantTimer& timer) {
  if (timer.label.empty()) {
    return base::UTF16ToUTF8(
        base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                timer.state == AssistantTimerState::kFired
                    ? IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE_WHEN_FIRED
                    : IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE),
            ToOriginalDurationString(timer)));
  }
  return base::UTF16ToUTF8(base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(
          timer.state == AssistantTimerState::kFired
              ? IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE_WHEN_FIRED_WITH_LABEL
              : IDS_ASSISTANT_TIMER_NOTIFICATION_MESSAGE_WITH_LABEL),
      ToOriginalDurationString(timer), timer.label));
}

// Creates notification buttons for the given |timer|.
std::vector<AssistantNotificationButton> CreateTimerNotificationButtons(
    const AssistantTimer& timer) {
  std::vector<AssistantNotificationButton> buttons;

  if (timer.state != AssistantTimerState::kFired) {
    if (timer.state == AssistantTimerState::kPaused) {
      // "RESUME" button.
      buttons.push_back({l10n_util::GetStringUTF8(
                             IDS_ASSISTANT_TIMER_NOTIFICATION_RESUME_BUTTON),
                         assistant::util::CreateAlarmTimerDeepLink(
                             AlarmTimerAction::kResumeTimer, timer.id)
                             .value(),
                         /*remove_notification_on_click=*/false});
    } else {
      // "PAUSE" button.
      buttons.push_back({l10n_util::GetStringUTF8(
                             IDS_ASSISTANT_TIMER_NOTIFICATION_PAUSE_BUTTON),
                         assistant::util::CreateAlarmTimerDeepLink(
                             AlarmTimerAction::kPauseTimer, timer.id)
                             .value(),
                         /*remove_notification_on_click=*/false});
    }
  }

  if (timer.state == AssistantTimerState::kFired) {
    // "STOP" button.
    buttons.push_back(
        {l10n_util::GetStringUTF8(IDS_ASSISTANT_TIMER_NOTIFICATION_STOP_BUTTON),
         assistant::util::CreateAlarmTimerDeepLink(
             AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
             .value(),
         /*remove_notification_on_click=*/true});

    // "ADD 1 MIN" button.
    buttons.push_back(
        {l10n_util::GetStringUTF8(
             IDS_ASSISTANT_TIMER_NOTIFICATION_ADD_1_MIN_BUTTON),
         assistant::util::CreateAlarmTimerDeepLink(
             AlarmTimerAction::kAddTimeToTimer, timer.id, base::Minutes(1))
             .value(),
         /*remove_notification_on_click=*/false});
  } else {
    // "CANCEL" button.
    buttons.push_back({l10n_util::GetStringUTF8(
                           IDS_ASSISTANT_TIMER_NOTIFICATION_CANCEL_BUTTON),
                       assistant::util::CreateAlarmTimerDeepLink(
                           AlarmTimerAction::kRemoveAlarmOrTimer, timer.id)
                           .value(),
                       /*remove_notification_on_click=*/true});
  }

  return buttons;
}

// Creates a timer notification priority for the given |timer|.
AssistantNotificationPriority CreateTimerNotificationPriority(
    const AssistantTimer& timer) {
  // In timers v2, a notification for a |kFired| timer is |kHigh| priority.
  // This will cause the notification to pop up to the user.
  if (timer.state == AssistantTimerState::kFired)
    return AssistantNotificationPriority::kHigh;

  // If the notification has lived for at least |kPopupThreshold|, drop the
  // priority to |kLow| so that the notification will not pop up to the user.
  constexpr base::TimeDelta kPopupThreshold = base::Seconds(6);
  const base::TimeDelta lifetime =
      base::Time::Now() - timer.creation_time.value_or(base::Time::Now());
  if (lifetime >= kPopupThreshold)
    return AssistantNotificationPriority::kLow;

  // Otherwise, the notification is |kDefault| priority. This means that it
  // may or may not pop up to the user, depending on the presence of other
  // notifications.
  return AssistantNotificationPriority::kDefault;
}

// Creates a notification for the given |timer|.
AssistantNotification CreateTimerNotification(
    const AssistantTimer& timer,
    const AssistantNotification* existing_notification = nullptr) {
  AssistantNotification notification;
  notification.title = CreateTimerNotificationTitle(timer);
  notification.message = CreateTimerNotificationMessage(timer);
  notification.buttons = CreateTimerNotificationButtons(timer);
  notification.client_id = CreateTimerNotificationId(timer);
  notification.grouping_key = kTimerNotificationGroupingKey;
  notification.priority = CreateTimerNotificationPriority(timer);
  notification.remove_on_click = false;
  notification.is_pinned = false;

  // If we are creating a notification to replace an |existing_notification| and
  // our new notification has higher priority, we want the system to "renotify"
  // the user of the notification change. This will cause the new notification
  // to popup to the user even if it was previously marked as read.
  if (existing_notification &&
      notification.priority > existing_notification->priority) {
    notification.renotify = true;
  }

  return notification;
}

// Returns whether an |update| from LibAssistant to the specified |original|
// timer is allowed. Updates are always allowed in v1, only conditionally in v2.
bool ShouldAllowUpdateFromLibAssistant(const AssistantTimer& original,
                                       const AssistantTimer& update) {
  // If |id| is not equal, then |update| does refer to the |original| timer.
  DCHECK_EQ(original.id, update.id);

  // In v2, updates are only allowed from LibAssistant if they are significant.
  // We may receive an update due to a state change in another timer, and we'd
  // want to discard the update to this timer to avoid introducing UI jank by
  // updating its notification outside of its regular tick interval. In v2, we
  // also update timer state from |kScheduled| to |kFired| ourselves to work
  // around latency in receiving the event from LibAssistant. When we do so, we
  // expect to later receive the state change from LibAssistant but discard it.
  return !original.IsEqualInLibAssistantTo(update);
}

}  // namespace

// AssistantAlarmTimerControllerImpl ------------------------------------------

AssistantAlarmTimerControllerImpl::AssistantAlarmTimerControllerImpl(
    AssistantControllerImpl* assistant_controller)
    : assistant_controller_(assistant_controller) {
  model_.AddObserver(this);
  assistant_controller_observation_.Observe(AssistantController::Get());
}

AssistantAlarmTimerControllerImpl::~AssistantAlarmTimerControllerImpl() {
  model_.RemoveObserver(this);
}

void AssistantAlarmTimerControllerImpl::SetAssistant(
    assistant::Assistant* assistant) {
  assistant_ = assistant;
}

const AssistantAlarmTimerModel* AssistantAlarmTimerControllerImpl::GetModel()
    const {
  return &model_;
}

void AssistantAlarmTimerControllerImpl::OnTimerStateChanged(
    const std::vector<AssistantTimer>& new_or_updated_timers) {
  // First we remove all old timers that no longer exist.
  for (const auto* old_timer : model_.GetAllTimers()) {
    if (!base::Contains(new_or_updated_timers, old_timer->id,
                        &AssistantTimer::id)) {
      model_.RemoveTimer(old_timer->id);
    }
  }

  // Then we add any new timers and update existing ones (if allowed).
  for (const auto& new_or_updated_timer : new_or_updated_timers) {
    const auto* original_timer = model_.GetTimerById(new_or_updated_timer.id);
    const bool is_new_timer = original_timer == nullptr;
    if (is_new_timer || ShouldAllowUpdateFromLibAssistant(
                            *original_timer, new_or_updated_timer)) {
      model_.AddOrUpdateTimer(std::move(new_or_updated_timer));
    }
  }
}

void AssistantAlarmTimerControllerImpl::OnAssistantControllerConstructed() {
  AssistantState::Get()->AddObserver(this);
}

void AssistantAlarmTimerControllerImpl::OnAssistantControllerDestroying() {
  AssistantState::Get()->RemoveObserver(this);
}

void AssistantAlarmTimerControllerImpl::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using assistant::util::DeepLinkParam;
  using assistant::util::DeepLinkType;

  if (type != DeepLinkType::kAlarmTimer)
    return;

  const std::optional<AlarmTimerAction>& action =
      assistant::util::GetDeepLinkParamAsAlarmTimerAction(params);
  if (!action.has_value())
    return;

  const std::optional<std::string>& alarm_timer_id =
      assistant::util::GetDeepLinkParam(params, DeepLinkParam::kId);
  if (!alarm_timer_id.has_value())
    return;

  // Duration is optional. Only used for adding time to timer.
  const std::optional<base::TimeDelta>& duration =
      assistant::util::GetDeepLinkParamAsTimeDelta(params,
                                                   DeepLinkParam::kDurationMs);

  PerformAlarmTimerAction(action.value(), alarm_timer_id.value(), duration);
}

void AssistantAlarmTimerControllerImpl::OnAssistantStatusChanged(
    assistant::AssistantStatus status) {
  // If LibAssistant is no longer running we need to clear our cache to
  // accurately reflect LibAssistant alarm/timer state.
  if (status == assistant::AssistantStatus::NOT_READY)
    model_.RemoveAllTimers();
}

void AssistantAlarmTimerControllerImpl::OnTimerAdded(
    const AssistantTimer& timer) {
  // Schedule the next tick of |timer|.
  ScheduleNextTick(timer);

  // Create a notification for the added alarm/timer.
  assistant_controller_->notification_controller()->AddOrUpdateNotification(
      CreateTimerNotification(timer));
}

void AssistantAlarmTimerControllerImpl::OnTimerUpdated(
    const AssistantTimer& timer) {
  // Schedule the next tick of |timer|.
  ScheduleNextTick(timer);

  auto* notification_controller =
      assistant_controller_->notification_controller();
  const auto* existing_notification =
      notification_controller->model()->GetNotificationById(
          CreateTimerNotificationId(timer));

  // When a |timer| is updated we need to update the corresponding notification
  // unless it has already been dismissed by the user.
  if (existing_notification) {
    notification_controller->AddOrUpdateNotification(
        CreateTimerNotification(timer, existing_notification));
  }
}

void AssistantAlarmTimerControllerImpl::OnTimerRemoved(
    const AssistantTimer& timer) {
  // Clean up the ticker for |timer|, if one exists.
  tickers_.erase(timer.id);

  // Remove any notification associated w/ |timer|.
  assistant_controller_->notification_controller()->RemoveNotificationById(
      CreateTimerNotificationId(timer), /*from_server=*/false);
}

void AssistantAlarmTimerControllerImpl::PerformAlarmTimerAction(
    const AlarmTimerAction& action,
    const std::string& alarm_timer_id,
    const std::optional<base::TimeDelta>& duration) {
  DCHECK(assistant_);

  switch (action) {
    case AlarmTimerAction::kAddTimeToTimer:
      if (!duration.has_value()) {
        LOG(ERROR) << "Ignoring add time to timer action duration.";
        return;
      }
      assistant_->AddTimeToTimer(alarm_timer_id, duration.value());
      break;
    case AlarmTimerAction::kPauseTimer:
      DCHECK(!duration.has_value());
      assistant_->PauseTimer(alarm_timer_id);
      break;
    case AlarmTimerAction::kRemoveAlarmOrTimer:
      DCHECK(!duration.has_value());
      assistant_->RemoveAlarmOrTimer(alarm_timer_id);
      break;
    case AlarmTimerAction::kResumeTimer:
      DCHECK(!duration.has_value());
      assistant_->ResumeTimer(alarm_timer_id);
      break;
  }
}

void AssistantAlarmTimerControllerImpl::ScheduleNextTick(
    const AssistantTimer& timer) {
  auto& ticker = tickers_[timer.id];
  if (ticker.IsRunning())
    return;

  // The next tick of |timer| should occur at its next full second of remaining
  // time. Here we are calculating the number of milliseconds to that next full
  // second.
  int millis_to_next_full_sec = timer.remaining_time.InMilliseconds() % 1000;

  // If |timer| has already fired, |millis_to_next_full_sec| will be negative.
  // In this case, we take the inverse of the value to get the correct number of
  // milliseconds to the next full second of remaining time.
  if (millis_to_next_full_sec < 0)
    millis_to_next_full_sec = 1000 + millis_to_next_full_sec;

  // If we are exactly at the boundary of a full second, we want to make sure
  // we wait until the next second to perform the next tick. Otherwise we'll end
  // up w/ a superfluous tick that is unnecessary.
  if (millis_to_next_full_sec == 0)
    millis_to_next_full_sec = 1000;

  // NOTE: We pass a copy of |timer.id| here as |timer| may no longer exist
  // when Tick() is called due to the possibility of the |model_| being updated
  // via a call to OnTimerStateChanged(), such as might happen if a timer is
  // created, paused, resumed, or removed by LibAssistant.
  ticker.Start(FROM_HERE, base::Milliseconds(millis_to_next_full_sec),
               base::BindOnce(&AssistantAlarmTimerControllerImpl::Tick,
                              base::Unretained(this), timer.id));
}

void AssistantAlarmTimerControllerImpl::Tick(const std::string& timer_id) {
  const auto* timer = model_.GetTimerById(timer_id);
  DCHECK(timer);

  // We don't tick paused timers. Once the |timer| resumes, ticking will resume.
  if (timer->state == AssistantTimerState::kPaused)
    return;

  // Update |timer| to reflect the new amount of |remaining_time|.
  AssistantTimer updated_timer(*timer);
  updated_timer.remaining_time = updated_timer.fire_time - base::Time::Now();

  // If there is no remaining time left on the timer, we ensure that our timer
  // is marked as |kFired|. Since LibAssistant may be a bit slow to notify us of
  // the change in state, we set the value ourselves to eliminate UI jank.
  // NOTE: We use the rounded value of |remaining_time| since that's what we are
  // displaying to the user and otherwise would be out of sync for ticks
  // occurring at full second boundary values.
  if (std::round(updated_timer.remaining_time.InSecondsF()) <= 0.f)
    updated_timer.state = AssistantTimerState::kFired;

  model_.AddOrUpdateTimer(std::move(updated_timer));
}

}  // namespace ash
