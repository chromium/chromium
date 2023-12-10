// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_update_controller_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "components/session_manager/session_manager_types.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"

using message_center::Notification;
using session_manager::SessionState;

namespace ash {
namespace {

const char kLocaleChangeNotificationId[] = "chrome://settings/locale";
const char kNotifierLocale[] = "ash.locale";

class LocaleNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit LocaleNotificationDelegate(
      base::OnceCallback<void(LocaleNotificationResult)> callback);

  LocaleNotificationDelegate(const LocaleNotificationDelegate&) = delete;
  LocaleNotificationDelegate& operator=(const LocaleNotificationDelegate&) =
      delete;

 protected:
  ~LocaleNotificationDelegate() override;

  // message_center::NotificationDelegate overrides:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  base::OnceCallback<void(LocaleNotificationResult)> callback_;
};

LocaleNotificationDelegate::LocaleNotificationDelegate(
    base::OnceCallback<void(LocaleNotificationResult)> callback)
    : callback_(std::move(callback)) {}

LocaleNotificationDelegate::~LocaleNotificationDelegate() {
  if (callback_) {
    // We're being destroyed but the user didn't click on anything. Run the
    // callback so that we don't crash.
    std::move(callback_).Run(LocaleNotificationResult::kAccept);
  }
}

void LocaleNotificationDelegate::Close(bool by_user) {
  if (callback_) {
    std::move(callback_).Run(LocaleNotificationResult::kAccept);
  }
}

void LocaleNotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!callback_)
    return;

  std::move(callback_).Run(button_index ? LocaleNotificationResult::kRevert
                                        : LocaleNotificationResult::kAccept);

  message_center::MessageCenter::Get()->RemoveNotification(
      kLocaleChangeNotificationId, true /* by_user */);
}

}  // namespace

LocaleUpdateControllerImpl::LocaleUpdateControllerImpl() = default;

LocaleUpdateControllerImpl::~LocaleUpdateControllerImpl() = default;

void LocaleUpdateControllerImpl::OnLocaleChanged() {
  for (auto& observer : observers_)
    observer.OnLocaleChanged();
}

void LocaleUpdateControllerImpl::ConfirmLocaleChange(
    const std::string& current_locale,
    const std::string& from_locale,
    const std::string& to_locale,
    LocaleChangeConfirmationCallback callback) {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());
  std::u16string from_locale_name =
      l10n_util::GetDisplayNameForLocale(from_locale, current_locale, true);
  std::u16string to_locale_name =
      l10n_util::GetDisplayNameForLocale(to_locale, current_locale, true);

  message_center::RichNotificationData optional;
  optional.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_LOCALE_REVERT_MESSAGE, from_locale_name)));
  optional.never_timeout = true;

  for (auto& observer : observers_)
    observer.OnLocaleChanged();

  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kLocaleChangeNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCALE_CHANGE_TITLE),
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_LOCALE_CHANGE_MESSAGE,
                                 from_locale_name, to_locale_name),
      std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierLocale,
                                 NotificationCatalogName::kLocaleUpdate),
      optional, new LocaleNotificationDelegate(std::move(callback)),
      vector_icons::kSettingsIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void LocaleUpdateControllerImpl::AddObserver(LocaleChangeObserver* observer) {
  observers_.AddObserver(observer);
}

void LocaleUpdateControllerImpl::RemoveObserver(
    LocaleChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
