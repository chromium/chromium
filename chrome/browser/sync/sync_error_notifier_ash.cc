// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_error_notifier_ash.h"

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/account_id/account_id.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace {

const char kProfileSyncNotificationId[] = "chrome://settings/sync/";

struct BubbleViewParameters {
  int message_id;
  base::RepeatingClosure click_action;
};

void ShowSyncSetup(Profile* profile) {
  LoginUIService* login_ui = LoginUIServiceFactory::GetForProfile(profile);
  if (login_ui->current_login_ui()) {
    // TODO(michaelpg): The LoginUI might be on an inactive desktop.
    // See crbug.com/354280.
    login_ui->current_login_ui()->FocusUI();
    return;
  }

  chrome::ShowSettingsSubPageForProfile(profile, chrome::kSyncSetupSubPage);
}

void TriggerSyncKeyRetrieval(Profile* profile) {
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  sync_ui_util::OpenTabForSyncKeyRetrieval(
      displayer.browser(), syncer::KeyRetrievalTriggerForUMA::kNotification);
}

BubbleViewParameters GetBubbleViewParameters(
    Profile* profile,
    syncer::SyncService* sync_service) {
  if (sync_ui_util::ShouldShowPassphraseError(sync_service)) {
    BubbleViewParameters params;
    params.message_id = IDS_SYNC_PASSPHRASE_ERROR_BUBBLE_VIEW_MESSAGE;
    // |profile| is guaranteed to outlive the callback because the ownership of
    // the notification gets transferred to NotificationDisplayService, which is
    // a keyed service that cannot outlive the profile.
    params.click_action =
        base::BindRepeating(&ShowSyncSetup, base::Unretained(profile));
    return params;
  }

  DCHECK(sync_ui_util::ShouldShowSyncKeysMissingError(sync_service));

  BubbleViewParameters params;
  params.message_id =
      sync_service->GetUserSettings()->IsEncryptEverythingEnabled()
          ? IDS_SYNC_NEEDS_KEYS_FOR_EVERYTHING_ERROR_BUBBLE_VIEW_MESSAGE
          : IDS_SYNC_NEEDS_KEYS_FOR_PASSWORDS_ERROR_BUBBLE_VIEW_MESSAGE;

  params.click_action =
      base::BindRepeating(&TriggerSyncKeyRetrieval, base::Unretained(profile));
  return params;
}

}  // namespace

SyncErrorNotifier::SyncErrorNotifier(syncer::SyncService* sync_service,
                                     Profile* profile)
    : sync_service_(sync_service), profile_(profile) {
  // Create a unique notification ID for this profile.
  notification_id_ =
      kProfileSyncNotificationId + profile_->GetProfileUserName();

  sync_service_->AddObserver(this);
  OnStateChanged(sync_service_);
}

SyncErrorNotifier::~SyncErrorNotifier() {
  DCHECK(!sync_service_) << "SyncErrorNotifier::Shutdown() was not called";
}

void SyncErrorNotifier::Shutdown() {
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

void SyncErrorNotifier::OnStateChanged(syncer::SyncService* service) {
  DCHECK_EQ(service, sync_service_);

  const bool should_display_notification =
      sync_ui_util::ShouldShowPassphraseError(sync_service_) ||
      sync_ui_util::ShouldShowSyncKeysMissingError(sync_service_);

  if (should_display_notification == notification_displayed_) {
    return;
  }

  auto* display_service = NotificationDisplayService::GetForProfile(profile_);
  if (!should_display_notification) {
    notification_displayed_ = false;
    display_service->Close(NotificationHandler::Type::TRANSIENT,
                           notification_id_);
    return;
  }

  // Error state just got triggered. There shouldn't be previous notification.
  // Let's display one.
  DCHECK(!notification_displayed_ && should_display_notification);

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kProfileSyncNotificationId);

  // Set |profile_id| for multi-user notification blocker.
  notifier_id.profile_id =
      multi_user_util::GetAccountIdFromProfile(profile_).GetUserEmail();

  BubbleViewParameters parameters =
      GetBubbleViewParameters(profile_, sync_service_);

  // Add a new notification.
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_,
          l10n_util::GetStringUTF16(IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE),
          l10n_util::GetStringUTF16(parameters.message_id), std::u16string(),
          GURL(notification_id_), notifier_id,
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              parameters.click_action),
          chromeos::kNotificationWarningIcon,
          message_center::SystemNotificationWarningLevel::WARNING);

  display_service->Display(NotificationHandler::Type::TRANSIENT, *notification,
                           /*metadata=*/nullptr);
  notification_displayed_ = true;
}
