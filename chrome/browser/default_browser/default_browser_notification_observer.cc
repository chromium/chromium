// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_notification_observer.h"

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/default_browser/default_browser_changed_notification_handler.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace default_browser {

DefaultBrowserNotificationObserver::DefaultBrowserNotificationObserver(
    RegisterCallback register_callback,
    InitialStateCheckCallback initial_state_check_callback,
    DefaultBrowserManager& manager)
    : manager_(manager) {
#if BUILDFLAG(IS_WIN)
  // On Windows, some install modes don't support being set as default.
  if (!install_static::SupportsSetAsDefaultBrowser()) {
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  default_browser_change_subscription_ =
      std::move(register_callback)
          .Run(base::BindRepeating(
              &DefaultBrowserNotificationObserver::OnDefaultBrowserStateChanged,
              // Safe because `default_browser_change_subscription_`
              // automatically unregisters the callback on destruction.
              base::Unretained(this)));

  std::move(initial_state_check_callback)
      .Run(base::BindOnce(
          &DefaultBrowserNotificationObserver::OnDefaultBrowserStateChanged,
          weak_ptr_factory_.GetWeakPtr()));
}

DefaultBrowserNotificationObserver::~DefaultBrowserNotificationObserver() =
    default;

void DefaultBrowserNotificationObserver::OnProfileWillBeDestroyed(
    Profile* profile) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  notification_shown_time_.reset();
  profile_observation_.Reset();
}

void DefaultBrowserNotificationObserver::OnDefaultBrowserStateChanged(
    DefaultBrowserState state) {
  // Ignore transient `UNKNOWN_DEFAULT` states so an earlier confirmed
  // `IS_DEFAULT` state is not overwritten.
  if (state == shell_integration::UNKNOWN_DEFAULT) {
    return;
  }

  const bool lost_default_status =
      (last_state_ == shell_integration::IS_DEFAULT &&
       state == shell_integration::NOT_DEFAULT);

  last_state_ = state;

  if (lost_default_status) {
    ShowNotification();
    return;
  }

  if (state == shell_integration::IS_DEFAULT) {
    MaybeCloseNotification();
  }
}

void DefaultBrowserNotificationObserver::ShowNotification() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  profile_observation_.Reset();
  notification_shown_time_.reset();

  Profile& profile = manager_->GetProfile();
  if (profile.ShutdownStarted()) {
    return;
  }
  // Can be null for non-user profiles without `NotificationDisplayService`
  // (such as the System Profile when the Profile Picker is open) or when a
  // null `TestingFactory` is installed in tests.
  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(&profile);
  if (!display_service) {
    return;
  }

  notification_shown_time_ = base::TimeTicks::Now();
  profile_observation_.Observe(&profile);

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_YES_BUTTON));
  optional_fields.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_NO_THANKS_BUTTON));
  optional_fields.accessible_name =
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_MESSAGE);

  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      DefaultBrowserManager::kNotificationId,
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_TITLE),
      l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_CHANGED_MESSAGE),
      ui::ImageModel::FromVectorIcon(vector_icons::kProductRefreshIcon,
                                     ui::kColorMenuIcon, 128),
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 DefaultBrowserManager::kNotificationId),
      optional_fields, /*delegate=*/nullptr);

  display_service->Display(NotificationHandler::Type::DEFAULT_BROWSER_CHANGED,
                           std::move(notification), /*metadata=*/nullptr);
}

void DefaultBrowserNotificationObserver::MaybeCloseNotification() {
  if (!notification_shown_time_.has_value()) {
    return;
  }

  const base::TimeDelta elapsed =
      base::TimeTicks::Now() - *notification_shown_time_;
  notification_shown_time_.reset();

  Profile* profile = profile_observation_.GetSource();
  if (!profile || profile->ShutdownStarted()) {
    profile_observation_.Reset();
    return;
  }
  // Can become null on a live `Profile` in tests when
  // `~NotificationDisplayServiceTester()` installs a null `TestingFactory`
  // during test teardown before `DefaultBrowserManager` or `Profile` shuts
  // down.
  NotificationDisplayService* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  if (!display_service) {
    profile_observation_.Reset();
    return;
  }

  display_service->GetDisplayed(base::BindOnce(
      &DefaultBrowserNotificationObserver::CloseNotificationIfDisplayed,
      weak_ptr_factory_.GetWeakPtr(), elapsed));
}

void DefaultBrowserNotificationObserver::CloseNotificationIfDisplayed(
    base::TimeDelta elapsed,
    std::set<std::string> displayed_ids,
    bool supports_synchronization) {
  Profile* profile = profile_observation_.GetSource();
  profile_observation_.Reset();
  if (!profile || profile->ShutdownStarted()) {
    return;
  }
  if (!supports_synchronization ||
      !displayed_ids.contains(DefaultBrowserManager::kNotificationId)) {
    return;
  }
  // `CloseNotification()` returns false if `OnClick()` or `OnClose()` already
  // consumed the active controller while `GetDisplayed()` was in flight (or if
  // `~NotificationDisplayServiceTester()` tore down the service in tests).
  if (DefaultBrowserChangedNotificationHandler::CloseNotification(profile)) {
    base::UmaHistogramLongTimes(
        "DefaultBrowser.ChangeDetectedNotification."
        "TimeUntilDefaultWithoutClick",
        elapsed);
  }
}

}  // namespace default_browser
