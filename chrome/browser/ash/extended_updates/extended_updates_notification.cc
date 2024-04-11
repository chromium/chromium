// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_notification.h"

#include <optional>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system_notification_builder.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_dialog.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
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

ExtendedUpdatesNotification::ExtendedUpdatesNotification(Profile* profile) {
  CHECK(profile);
  profile_observation_.Observe(profile);
}

ExtendedUpdatesNotification::~ExtendedUpdatesNotification() = default;

base::WeakPtr<ExtendedUpdatesNotification> ExtendedUpdatesNotification::Create(
    Profile* profile) {
  return (new ExtendedUpdatesNotification(profile))->GetWeakPtr();
}

void ExtendedUpdatesNotification::Show() {
  Profile* profile = profile_observation_.GetSource();
  if (!profile) {
    return;
  }

  message_center::RichNotificationData data;
  // Keep same order as |IndexedButton| enum.
  AddButton(data, IndexedButton::kSetUp,
            IDS_EXTENDED_UPDATES_NOTIFICATION_SETUP_BUTTON);
  AddButton(data, IndexedButton::kLearnMore,
            IDS_EXTENDED_UPDATES_NOTIFICATION_LEARN_MORE_BUTTON);

  SystemNotificationBuilder builder;
  builder.SetId(std::string(kNotificationId))
      .SetCatalogName(NotificationCatalogName::kExtendedUpdatesAvailable)
      .SetTitle(l10n_util::GetStringFUTF16(
          IDS_EXTENDED_UPDATES_NOTIFICATION_TITLE, ui::GetChromeOSDeviceName()))
      .SetMessageId(IDS_EXTENDED_UPDATES_NOTIFICATION_MESSAGE)
      .SetOptionalFields(data)
      .SetDelegate(
          base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
              weak_factory_.GetWeakPtr()));
  NotificationDisplayService::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT,
      builder.Build(/*keep_timestamp=*/false), /*metadata=*/nullptr);
}

void ExtendedUpdatesNotification::Close(bool by_user) {
  delete this;
}

void ExtendedUpdatesNotification::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!button_index) {
    return;
  }

  switch (IndexedButton{*button_index}) {
    case IndexedButton::kSetUp:
      ShowExtendedUpdatesDialog();
      break;
    case IndexedButton::kLearnMore:
      OpenLearnMoreUrl();
      break;
  }
}

void ExtendedUpdatesNotification::OnProfileWillBeDestroyed(Profile* profile) {
  delete this;
}

base::WeakPtr<ExtendedUpdatesNotification>
ExtendedUpdatesNotification::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

}  // namespace ash
