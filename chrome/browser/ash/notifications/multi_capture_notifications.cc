// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_notifications.h"

#include <memory>
#include <optional>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/origin.h"

namespace ash {

namespace {

constexpr size_t kAppMaxNameLength = 18;

constexpr char kMultiCaptureId[] = "multi_capture";
constexpr char kNotifierMultiCapture[] = "ash.multi_capture";
constexpr char kMultiCaptureOnLoginId[] = "multi_capture_on_login";
constexpr char kNotifierMultiCaptureOnLogin[] = "ash.multi_capture_on_login";

constexpr base::TimeDelta kMinimumNotificationPresenceTime = base::Seconds(6);

void CreateAndShowNotification(
    const std::string& notifier_id,
    const std::string& notification_id,
    NotificationCatalogName notification_catalog_name,
    std::u16string notification_title,
    std::u16string notification_message) {
  message_center::NotifierId notifier(
      message_center::NotifierType::SYSTEM_COMPONENT, notifier_id,
      notification_catalog_name);

  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      /*title=*/
      notification_title,
      /*message=*/
      notification_message,
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(), notifier,
      /*optional_fields=*/message_center::RichNotificationData(),
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          message_center::HandleNotificationClickDelegate::ButtonClickCallback(
              base::DoNothing())),
      kShelfEnterpriseIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void MaybeShowLoginNotification(bool is_multi_capture_allowed) {
  if (!is_multi_capture_allowed) {
    return;
  }
  CreateAndShowNotification(
      kNotifierMultiCaptureOnLogin, kMultiCaptureOnLoginId,
      NotificationCatalogName::kMultiCaptureOnLogin,
      l10n_util::GetStringUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_ON_LOGIN_TITLE),
      l10n_util::GetStringUTF16(
          IDS_MULTI_CAPTURE_NOTIFICATION_ON_LOGIN_MESSAGE));
}

// This function makes sure that on login all data required to check whether a
// notification is needed is propagated from the policy to the
// ManagedAccessToGetAllScreensMediaInSessionAllowedForUrls pref.
void TransferGetAllScreensMediaPolicyValue(
    content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* pref_service = profile->GetPrefs();
  if (!pref_service) {
    return;
  }
  const base::Value::List& allowed_origins = pref_service->GetList(
      capture_policy::kManagedAccessToGetAllScreensMediaAllowedForUrls);
  pref_service->SetList(
      prefs::kManagedAccessToGetAllScreensMediaInSessionAllowedForUrls,
      allowed_origins.Clone());
}

void ShowLoginNotificationIfMultiCaptureAllowed() {
  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user) {
    return;
  }

  content::BrowserContext* browser_context =
      BrowserContextHelper::Get()->GetBrowserContextByUser(active_user);
  if (!browser_context) {
    return;
  }

  // TODO(b/329064666): Remove this function once the pivot to IWAs is complete.
  TransferGetAllScreensMediaPolicyValue(browser_context);

  capture_policy::CheckGetAllScreensMediaAllowedForAnyOrigin(
      browser_context, base::BindOnce(&MaybeShowLoginNotification));
}

}  // namespace

MultiCaptureNotifications::NotificationMetadata::NotificationMetadata(
    std::string id,
    base::TimeTicks time_created)
    : id(std::move(id)), time_created(time_created) {}
MultiCaptureNotifications::NotificationMetadata::NotificationMetadata(
    MultiCaptureNotifications::NotificationMetadata&& metadata) = default;
MultiCaptureNotifications::NotificationMetadata&
MultiCaptureNotifications::NotificationMetadata::operator=(
    MultiCaptureNotifications::NotificationMetadata&& metadata) = default;
MultiCaptureNotifications::NotificationMetadata::~NotificationMetadata() =
    default;

MultiCaptureNotifications::MultiCaptureNotifications() {
  DCHECK(Shell::HasInstance());
  multi_capture_service_client_observation_.Observe(
      Shell::Get()->multi_capture_service_client());
  login_state_observation_.Observe(LoginState::Get());
}

MultiCaptureNotifications::~MultiCaptureNotifications() = default;

void MultiCaptureNotifications::MultiCaptureStarted(const std::string& label,
                                                    const url::Origin& origin) {
  const std::string host = origin.host();
  MultiCaptureStartedInternal(label, base::StrCat({kMultiCaptureId, ":", host}),
                              host);
}

void MultiCaptureNotifications::MultiCaptureStartedFromApp(
    const std::string& label,
    const std::string& app_id,
    const std::string& app_short_name) {
  MultiCaptureStartedInternal(
      label, base::StrCat({kMultiCaptureId, ":", label}), app_short_name);
}

void MultiCaptureNotifications::MultiCaptureStopped(const std::string& label) {
  const auto notifications_metadata_iterator =
      notifications_metadata_.find(label);
  if (notifications_metadata_iterator == notifications_metadata_.end()) {
    LOG(ERROR) << "Label could not be found";
    return;
  }

  NotificationMetadata& metadata = notifications_metadata_iterator->second;
  const base::TimeDelta time_already_shown =
      base::TimeTicks::Now() - metadata.time_created;
  if (time_already_shown >= kMinimumNotificationPresenceTime) {
    SystemNotificationHelper::GetInstance()->Close(
        /*notification_id=*/metadata.id);
    notifications_metadata_.erase(notifications_metadata_iterator);
  } else if (!metadata.closing_timer) {
    metadata.closing_timer = std::make_unique<base::OneShotTimer>();
    metadata.closing_timer->Start(
        FROM_HERE, kMinimumNotificationPresenceTime - time_already_shown,
        base::BindOnce(&MultiCaptureNotifications::MultiCaptureStopped,
                       weak_factory_.GetSafeRef(), label));
  }
}

void MultiCaptureNotifications::MultiCaptureServiceClientDestroyed() {
  multi_capture_service_client_observation_.Reset();
  for (const auto& [label, notification_metadata] : notifications_metadata_) {
    SystemNotificationHelper::GetInstance()->Close(
        /*notification_id=*/notification_metadata.id);
  }
  notifications_metadata_.clear();
}

void MultiCaptureNotifications::LoggedInStateChanged() {
  if (LoginState::Get()->IsUserLoggedIn()) {
    user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (!active_user) {
      return;
    }

    active_user->AddProfileCreatedObserver(
        base::BindOnce(&ShowLoginNotificationIfMultiCaptureAllowed));
  }
}

void MultiCaptureNotifications::MultiCaptureStartedInternal(
    const std::string& label,
    const std::string& notification_id,
    const std::string& app_name) {
  notifications_metadata_.emplace(
      label, NotificationMetadata(notification_id, base::TimeTicks::Now()));

  const std::u16string converted_app_name =
      gfx::TruncateString(base::UTF8ToUTF16(app_name), kAppMaxNameLength,
                          gfx::BreakType::WORD_BREAK);

  // TODO(crbug.com/40236161): Make sure the notification does not disappear
  // automatically after some time.
  CreateAndShowNotification(
      kNotifierMultiCapture, notification_id,
      NotificationCatalogName::kMultiCapture,
      /*notification_title=*/
      l10n_util::GetStringFUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_TITLE,
                                 converted_app_name),
      /*notification_message=*/
      l10n_util::GetStringFUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_MESSAGE,
                                 converted_app_name));
}

}  // namespace ash
