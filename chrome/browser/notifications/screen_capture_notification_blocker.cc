// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_capture_notification_blocker.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

void RecordScreenCaptureCount(const std::string& suffix, int count) {
  base::UmaHistogramCounts100(
      base::StrCat({"Notifications.Blocker.ScreenCapture.", suffix}), count);
}

}  // namespace

const char kMuteNotificationId[] = "notifications_muted";

ScreenCaptureNotificationBlocker::ScreenCaptureNotificationBlocker(
    NotificationDisplayService* notification_display_service)
    : notification_display_service_(notification_display_service) {
  DCHECK(notification_display_service_);
  observation_.Observe(MediaCaptureDevicesDispatcher::GetInstance()
                           ->GetMediaStreamCaptureIndicator()
                           .get());
}

ScreenCaptureNotificationBlocker::~ScreenCaptureNotificationBlocker() = default;

bool ScreenCaptureNotificationBlocker::ShouldBlockNotification(
    const message_center::Notification& notification) {
  // Don't block if the user clicked on "Show" for the current session.
  if (state_ == NotifyState::kShowAll)
    return false;

  // Don't block if no WebContents currently captures the screen.
  if (capturing_web_contents_.empty())
    return false;

  // Otherwise block all notifications that belong to non-capturing origins.
  return base::ranges::none_of(
      capturing_web_contents_,
      [&notification](content::WebContents* web_contents) {
        return url::IsSameOriginWith(notification.origin_url(),
                                     web_contents->GetLastCommittedURL());
      });
}

void ScreenCaptureNotificationBlocker::OnBlockedNotification(
    const message_center::Notification& notification,
    bool replaced) {
  if (replaced)
    ++replaced_notification_count_;
  else
    ++muted_notification_count_;

  if (state_ == NotifyState::kSnooze)
    ++snoozed_notification_count_;

  if (state_ == NotifyState::kNotifyMuted)
    DisplayMuteNotification();
}

void ScreenCaptureNotificationBlocker::OnClosedNotification(
    const message_center::Notification& notification) {
  ++closed_notification_count_;
}

void ScreenCaptureNotificationBlocker::OnAction(
    MutedNotificationHandler::Action action) {
  DCHECK(state_ == NotifyState::kNotifyMuted);
  CloseMuteNotification();

  switch (action) {
    case MutedNotificationHandler::Action::kUserClose:
    case MutedNotificationHandler::Action::kBodyClick:
      // Nothing to do here.
      break;
    case MutedNotificationHandler::Action::kShowClick:
      state_ = NotifyState::kShowAll;
      NotifyBlockingStateChanged();
      ReportSessionMetrics(/*revealed=*/true);
      break;
    case MutedNotificationHandler::Action::kSnoozeClick:
      state_ = NotifyState::kSnooze;
      break;
  }
}

void ScreenCaptureNotificationBlocker::OnIsCapturingDisplayChanged(
    content::WebContents* web_contents,
    bool is_capturing_display) {
  if (is_capturing_display)
    capturing_web_contents_.insert(web_contents);
  else
    capturing_web_contents_.erase(web_contents);

  if (capturing_web_contents_.empty()) {
    ReportSessionMetrics(/*revealed=*/false);
    muted_notification_count_ = 0;
    replaced_notification_count_ = 0;
    closed_notification_count_ = 0;
    snoozed_notification_count_ = 0;
    reported_session_metrics_ = false;
    state_ = NotifyState::kNotifyMuted;
    last_screen_capture_session_start_time_ = base::TimeTicks();
    CloseMuteNotification();
  } else if (last_screen_capture_session_start_time_.is_null()) {
    last_screen_capture_session_start_time_ = base::TimeTicks::Now();
  }

  NotifyBlockingStateChanged();
}

void ScreenCaptureNotificationBlocker::ReportSessionMetrics(bool revealed) {
  auto elapsed_time =
      base::TimeTicks::Now() - last_screen_capture_session_start_time_;
  base::UmaHistogramLongTimes(
      base::StrCat({"Notifications.Blocker.ScreenCapture.",
                    revealed ? "RevealDuration" : "SessionDuration"}),
      elapsed_time);

  if (reported_session_metrics_)
    return;

  RecordScreenCaptureCount("MutedCount", muted_notification_count_);
  RecordScreenCaptureCount("ReplacedCount", replaced_notification_count_);
  RecordScreenCaptureCount("ClosedCount", closed_notification_count_);
  RecordScreenCaptureCount("SnoozedCount", snoozed_notification_count_);

  reported_session_metrics_ = true;
}

void ScreenCaptureNotificationBlocker::DisplayMuteNotification() {
  int total_notification_count =
      muted_notification_count_ + replaced_notification_count_;

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.renotify = true;

  if (base::FeatureList::IsEnabled(features::kMuteNotificationSnoozeAction)) {
    rich_notification_data.buttons.emplace_back(
        l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_ACTION_SNOOZE));
  }
  rich_notification_data.buttons.emplace_back(l10n_util::GetPluralStringFUTF16(
      IDS_NOTIFICATION_MUTED_ACTION_SHOW, total_notification_count));

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kMuteNotificationId,
      l10n_util::GetPluralStringFUTF16(IDS_NOTIFICATION_MUTED_TITLE,
                                       total_notification_count),
      l10n_util::GetStringUTF16(IDS_NOTIFICATION_MUTED_MESSAGE),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), message_center::NotifierId(),
      rich_notification_data,
      /*delegate=*/nullptr);

  notification_display_service_->Display(
      NotificationHandler::Type::NOTIFICATIONS_MUTED, notification,
      /*metadata=*/nullptr);

  last_mute_notification_time_ = base::TimeTicks::Now();
}

void ScreenCaptureNotificationBlocker::CloseMuteNotification() {
  notification_display_service_->Close(
      NotificationHandler::Type::NOTIFICATIONS_MUTED, kMuteNotificationId);
}
