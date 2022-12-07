// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/arc_snapshot_reboot_notification_impl.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace arc {
namespace data_snapshotd {

namespace {

constexpr char kNotificationId[] = "arc_snaposhot_reboot";
constexpr int kRestartButtonId = 0;

}  // namespace

ArcSnapshotRebootNotificationImpl::ArcSnapshotRebootNotificationImpl() =
    default;

ArcSnapshotRebootNotificationImpl::~ArcSnapshotRebootNotificationImpl() {
  Hide();
}

void ArcSnapshotRebootNotificationImpl::SetUserConsentCallback(
    const base::RepeatingClosure& callback) {
  user_consent_callback_ = callback;
}

void ArcSnapshotRebootNotificationImpl::Show() {
  if (shown_)
    return;
  shown_ = true;

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId,
      ash::NotificationCatalogName::kArcSnapshotReboot);

  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ARC_SNAPSHOT_RESTART_NOTIFICATION_RESTART_BUTTON)));

  auto notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
      l10n_util::GetStringUTF16(IDS_ARC_SNAPSHOT_RESTART_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ARC_SNAPSHOT_RESTART_NOTIFICATION_MESSAGE),
      std::u16string() /* display_source */, GURL(), notifier_id,
      optional_fields,
      new message_center::HandleNotificationClickDelegate(
          base::BindRepeating(&ArcSnapshotRebootNotificationImpl::HandleClick,
                              weak_ptr_factory_.GetWeakPtr())),
      vector_icons::kBusinessIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  SystemNotificationHelper::GetInstance()->Display(notification);
}

void ArcSnapshotRebootNotificationImpl::Hide() {
  if (!shown_)
    return;
  shown_ = false;
  SystemNotificationHelper::GetInstance()->Close(kNotificationId);
}

// static
std::string
ArcSnapshotRebootNotificationImpl::get_notification_id_for_testing() {
  return kNotificationId;
}

// static
int ArcSnapshotRebootNotificationImpl::get_restart_button_id_for_testing() {
  return kRestartButtonId;
}

void ArcSnapshotRebootNotificationImpl::HandleClick(
    absl::optional<int> button_index) {
  if (!button_index)
    return;
  DCHECK(button_index.value() == kRestartButtonId);
  Hide();

  DCHECK(!user_consent_callback_.is_null());
  user_consent_callback_.Run();
}

}  // namespace data_snapshotd
}  // namespace arc
