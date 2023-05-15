// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_login_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

absl::optional<bool> g_is_multi_capture_allowed_for_testing;

constexpr char kMultiCaptureOnLoginId[] = "multi_capture_on_login";
constexpr char kNotifierMultiCaptureOnLogin[] = "ash.multi_capture_on_login";

bool IsMultiCaptureAllowed() {
  if (g_is_multi_capture_allowed_for_testing) {
    return *g_is_multi_capture_allowed_for_testing;
  }

  auto* active_user = user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user) {
    return false;
  }
  auto* browser_context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(active_user);
  if (!browser_context) {
    return false;
  }
  return capture_policy::IsGetAllScreensMediaAllowedForAnySite(browser_context);
}

void CreateAndShowNotification(bool is_multi_capture_allowed) {
  if (!is_multi_capture_allowed) {
    return;
  }
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kNotifierMultiCaptureOnLogin,
      ash::NotificationCatalogName::kMultiCaptureOnLogin);

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kMultiCaptureOnLoginId,
      /*title=*/
      l10n_util::GetStringUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_ON_LOGIN_TITLE),
      /*message=*/
      l10n_util::GetStringUTF16(
          IDS_MULTI_CAPTURE_NOTIFICATION_ON_LOGIN_MESSAGE),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(), notifier_id,
      /*optional_fields=*/message_center::RichNotificationData(),
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          message_center::HandleNotificationClickDelegate::ButtonClickCallback(
              base::DoNothing())),
      ash::kShelfEnterpriseIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

}  // namespace

namespace ash {

MultiCaptureLoginNotification::MultiCaptureLoginNotification() {
  login_state_observation_.Observe(ash::LoginState::Get());
}

MultiCaptureLoginNotification::~MultiCaptureLoginNotification() = default;

void MultiCaptureLoginNotification::LoggedInStateChanged() {
  if (ash::LoginState::Get()->IsUserLoggedIn()) {
    user_manager::User* active_user =
        user_manager::UserManager::Get()->GetActiveUser();
    if (!active_user) {
      return;
    }

    active_user->AddProfileCreatedObserver(
        base::BindOnce(&IsMultiCaptureAllowed)
            .Then(base::BindOnce(&CreateAndShowNotification)));
  }
}

void SetIsMultiCaptureAllowedCallbackForTesting(bool is_multi_capture_allowed) {
  g_is_multi_capture_allowed_for_testing = is_multi_capture_allowed;
}

}  // namespace ash
