// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/locale_switch_notification.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/message_center/oobe_notification_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {
namespace {

using ::message_center::Notification;
using ::message_center::NotificationDelegate;
using ::message_center::NotificationType;
using ::message_center::NotifierId;
using ::message_center::NotifierType;
using ::message_center::RichNotificationData;
using ::message_center::SystemNotificationWarningLevel;

// Simplest type of notification UI - no progress bars, images etc.
constexpr NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Generic type for notifications that are not from web pages etc.
constexpr NotificationHandler::Type kNotificationHandlerType =
    NotificationHandler::Type::TRANSIENT;

// Chromium logo icon that will displayed on the notification.
const gfx::VectorIcon& kIcon = vector_icons::kProductIcon;

constexpr SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::NORMAL;

class LocaleSwitchNotificationDelegate
    : public message_center::NotificationDelegate,
      public OobeUI::Observer {
 public:
  LocaleSwitchNotificationDelegate(
      std::string new_locale,
      Profile* profile,
      locale_util::SwitchLanguageCallback callback);

  LocaleSwitchNotificationDelegate(const LocaleSwitchNotificationDelegate&) =
      delete;
  LocaleSwitchNotificationDelegate& operator=(
      const LocaleSwitchNotificationDelegate&) = delete;

 protected:
  ~LocaleSwitchNotificationDelegate() override;

  // message_center::NotificationDelegate overrides:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  // OobeUI::Observer overrides:
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override;
  void OnDestroyingOobeUI() override;

  void CloseNotification();

  enum class NotificationButton {
    kSwitchLocale = 0,
  };

  std::string new_locale_;
  raw_ptr<Profile> profile_;
  locale_util::SwitchLanguageCallback callback_;

  bool is_screen_changed_ = false;
};

LocaleSwitchNotificationDelegate::LocaleSwitchNotificationDelegate(
    std::string new_locale,
    Profile* profile,
    locale_util::SwitchLanguageCallback callback)
    : new_locale_(std::move(new_locale)),
      profile_(profile),
      callback_(std::move(callback)) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (!host) {
    return;
  }
  OobeUI* ui = host->GetOobeUI();
  if (ui) {
    ui->AddObserver(this);
  }
}

LocaleSwitchNotificationDelegate::~LocaleSwitchNotificationDelegate() {
  // This observation removal handles the case when user clicks directly on the
  // close button (little cros in the upper-right corner of the notification).
  // Delegate is destroyed right after that click.
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (!host) {
    return;
  }
  OobeUI* ui = host->GetOobeUI();
  if (ui) {
    ui->RemoveObserver(this);
  }
}

void LocaleSwitchNotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  // If |button_index| is empty it means that user clicked on the body of a
  // notification. In this case notification will disappear from the screen, but
  // user still will be able to see it in the status tray. This will give user a
  // chance to change the locale if they accidentally missed the button.
  // If user proceeds to the next screen without any interactions with the
  // notification it will be removed from the status tray too.
  if (!button_index.has_value()) {
    return;
  }
  if (!callback_) {
    return;
  }

  // Switch locale if user selected the "Switch" option.
  if (*button_index == static_cast<int>(NotificationButton::kSwitchLocale)) {
    VLOG(1) << "Switching locale to " << new_locale_
            << " from the notification.";
    locale_util::SwitchLanguage(
        new_locale_,
        /*enable_locale_keyboard_layouts=*/false,  // The layouts will be synced
                                                   // instead. Also new user
                                                   // could enable required
                                                   // layouts from the settings.
        /*login_layouts_only=*/false, std::move(callback_), profile_);
  }

  // Remove notification regardless of which button user pressed.
  CloseNotification();
}

void LocaleSwitchNotificationDelegate::OnDestroyingOobeUI() {
  CloseNotification();
}

void LocaleSwitchNotificationDelegate::OnCurrentScreenChanged(
    OobeScreenId current_screen,
    OobeScreenId new_screen) {
  // |is_screen_changed_| will be set to |true| when OOBE flow will hit the
  // first screen that we will show after the locale switch screen.
  if (!is_screen_changed_) {
    is_screen_changed_ = true;
    return;
  }

  // In case we proceed with the OOBE flow and notification is still either
  // displayed on the screen or in the status tray we want to remove it and
  // cancel the observation.
  CloseNotification();
}

void LocaleSwitchNotificationDelegate::CloseNotification() {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (host) {
    OobeUI* ui = host->GetOobeUI();
    if (ui) {
      ui->RemoveObserver(this);
    }
  }

  NotificationDisplayService* nds =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  if (nds) {
    nds->Close(kNotificationHandlerType, kOOBELocaleSwitchNotificationId);
  }
}

}  // namespace

// static
void LocaleSwitchNotification::Show(
    Profile* profile,
    std::string new_locale,
    locale_util::SwitchLanguageCallback locale_switch_callback) {
  // NotifierId for histogram reporting.
  static const base::NoDestructor<NotifierId> kNotifierId(
      NotifierType::SYSTEM_COMPONENT, kOOBELocaleSwitchNotificationId,
      NotificationCatalogName::kLocaleUpdate);

  // Leaving this empty means the notification is attributed to the system -
  // ie "Chromium OS" or similar.
  static const base::NoDestructor<std::u16string> kEmptyDisplaySource;

  // No origin URL is needed since the notification comes from the system.
  static const base::NoDestructor<GURL> kEmptyOriginUrl;

  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_LOCALE_SWITCH_NOTIFICATION_TITLE);

  const std::u16string body = l10n_util::GetStringFUTF16(
      IDS_LOCALE_SWITCH_NOTIFICATION_TEXT,
      l10n_util::GetDisplayNameForLocale(
          new_locale, g_browser_process->GetApplicationLocale(),
          /*is_for_ui=*/true));

  const std::u16string accept_label = l10n_util::GetStringUTF16(
      IDS_LOCALE_SWITCH_NOTIFICATION_CONFIRM_BUTTON_LABEL);

  const std::u16string cancel_label = l10n_util::GetStringUTF16(
      IDS_LOCALE_SWITCH_NOTIFICATION_CANCEL_BUTTON_LABEL);

  RichNotificationData rich_notification_data;
  rich_notification_data.buttons.emplace_back(accept_label);
  rich_notification_data.buttons.emplace_back(cancel_label);

  const scoped_refptr<LocaleSwitchNotificationDelegate> delegate =
      base::MakeRefCounted<LocaleSwitchNotificationDelegate>(
          std::move(new_locale), profile, std::move(locale_switch_callback));

  Notification notification = CreateSystemNotification(
      kNotificationType, kOOBELocaleSwitchNotificationId, title, body,
      *kEmptyDisplaySource, *kEmptyOriginUrl, *kNotifierId,
      rich_notification_data, delegate, kIcon, kWarningLevel);

  NotificationDisplayService* nds =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  if (nds) {
    nds->Display(kNotificationHandlerType, notification, /*metadata=*/nullptr);
  }
}

}  // namespace ash
