// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_notification.h"

#include <optional>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "ash/system/extended_updates/extended_updates_metrics.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"

namespace ash {

namespace {

using IndexedButton = ExtendedUpdatesNotification::IndexedButton;

// Adds the |button| to the notification via |data|,
// where |title_id| is the string resource id for the button title.
void AddButton(message_center::RichNotificationData& data,
               IndexedButton button,
               int title_id) {
  DCHECK_EQ(data.buttons.size(), static_cast<size_t>(button));
  data.buttons.emplace_back(l10n_util::GetStringUTF16(title_id));
}

}  // namespace

ExtendedUpdatesNotification::ExtendedUpdatesNotification(Profile* profile)
    : profile_(profile->GetWeakPtr()) {}

ExtendedUpdatesNotification::~ExtendedUpdatesNotification() = default;

void ExtendedUpdatesNotification::Show(Profile* profile) {
  Show(base::MakeRefCounted<ExtendedUpdatesNotification>(profile));
}

void ExtendedUpdatesNotification::Show(
    scoped_refptr<ExtendedUpdatesNotification> delegate) {
  if (!delegate || !delegate->profile()) {
    return;
  }
  Profile* profile = delegate->profile();

  delegate->SubscribeToDeviceSettingsChanges();

  message_center::RichNotificationData data;
  // Keep same order as |IndexedButton| enum.
  AddButton(data, IndexedButton::kSetUp,
            IDS_EXTENDED_UPDATES_NOTIFICATION_SETUP_BUTTON);
  AddButton(data, IndexedButton::kLearnMore,
            IDS_EXTENDED_UPDATES_NOTIFICATION_LEARN_MORE_BUTTON);

  SystemNotificationBuilder builder;
  builder.SetId(std::string(kNotificationId))
      .SetCatalogName(NotificationCatalogName::kExtendedUpdatesAvailable)
      .SetTitleId(IDS_EXTENDED_UPDATES_NOTIFICATION_TITLE)
      .SetMessageId(IDS_EXTENDED_UPDATES_NOTIFICATION_MESSAGE)
      .SetOptionalFields(data)
      .SetDelegate(std::move(delegate));
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      kNotificationType, builder.Build(/*keep_timestamp=*/false),
      /*metadata=*/nullptr);
  RecordExtendedUpdatesEntryPointEvent(
      ExtendedUpdatesEntryPointEvent::kNoArcNotificationShown);
}

bool ExtendedUpdatesNotification::IsNotificationDismissed(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kExtendedUpdatesNotificationDismissed);
}

void ExtendedUpdatesNotification::Close(bool by_user) {
  if (by_user && profile_) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kExtendedUpdatesNotificationDismissed, true);
  }
}

void ExtendedUpdatesNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!button_index) {
    return;
  }

  switch (IndexedButton{*button_index}) {
    case IndexedButton::kSetUp:
      RecordExtendedUpdatesEntryPointEvent(
          ExtendedUpdatesEntryPointEvent::kNoArcNotificationClicked);
      ShowExtendedUpdatesDialog();
      break;
    case IndexedButton::kLearnMore:
      OpenLearnMoreUrl();
      break;
  }

  if (profile_) {
    NotificationDisplayServiceFactory::GetForProfile(profile_.get())
        ->Close(kNotificationType, std::string(kNotificationId));
  }
}

void ExtendedUpdatesNotification::ShowExtendedUpdatesDialog() {
  extended_updates::ExtendedUpdatesDialog::Show();
}

void ExtendedUpdatesNotification::OpenLearnMoreUrl() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(chrome::kDeviceExtendedUpdatesLearnMoreURL),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void ExtendedUpdatesNotification::SubscribeToDeviceSettingsChanges() {
  settings_change_subscription_ =
      ExtendedUpdatesController::SubscribeToDeviceSettingsChanges(
          base::BindRepeating(
              &ExtendedUpdatesNotification::OnDeviceSettingsChanged,
              weak_factory_.GetWeakPtr()));
}

void ExtendedUpdatesNotification::OnDeviceSettingsChanged() {
  if (profile_ && ExtendedUpdatesController::Get()->IsOptedIn()) {
    NotificationDisplayServiceFactory::GetForProfile(profile_.get())
        ->Close(kNotificationType, std::string(kNotificationId));
  }
}

}  // namespace ash
