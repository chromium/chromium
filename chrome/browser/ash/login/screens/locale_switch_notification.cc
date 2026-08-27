// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/locale_switch_notification.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/login/resources/grit/ash_login_strings.h"
#include "ash/public/cpp/message_center/oobe_notification_constants.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/user_manager/user.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {
namespace {

using ::message_center::NotificationDelegate;
using ::message_center::NotificationType;
using ::message_center::NotifierId;
using ::message_center::NotifierType;
using ::message_center::RichNotificationData;
using ::message_center::SystemNotificationWarningLevel;

// Simplest type of notification UI - no progress bars, images etc.
constexpr NotificationType kNotificationType =
    message_center::NOTIFICATION_TYPE_SIMPLE;

// Chromium logo icon that will displayed on the notification.
const gfx::VectorIcon& kIcon = vector_icons::kProductIcon;

constexpr SystemNotificationWarningLevel kWarningLevel =
    SystemNotificationWarningLevel::NORMAL;

class LocaleSwitchNotificationDelegate
    : public message_center::NotificationDelegate,
      public OobeUI::Observer {
 public:
  // `application_locale_storage` must be non-null and must outlive `this`.
  LocaleSwitchNotificationDelegate(
      ApplicationLocaleStorage* application_locale_storage,
      std::string new_locale,
      Profile* profile,
      std::string notification_id,
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

  const raw_ref<ApplicationLocaleStorage> application_locale_storage_;

  std::string new_locale_;
  raw_ptr<Profile> profile_;
  const std::string notification_id_;
  locale_util::SwitchLanguageCallback callback_;

  bool is_screen_changed_ = false;
};

LocaleSwitchNotificationDelegate::LocaleSwitchNotificationDelegate(
    ApplicationLocaleStorage* application_locale_storage,
    std::string new_locale,
    Profile* profile,
    std::string notification_id,
    locale_util::SwitchLanguageCallback callback)
    : application_locale_storage_(CHECK_DEREF(application_locale_storage)),
      new_locale_(std::move(new_locale)),
      profile_(profile),
      notification_id_(std::move(notification_id)),
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
        &application_locale_storage_.get(), new_locale_,
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

  message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                           /*by_user=*/false);
}

}  // namespace

// static
void LocaleSwitchNotification::Show(
    ApplicationLocaleStorage* application_locale_storage,
    Profile* profile,
    std::string new_locale,
    locale_util::SwitchLanguageCallback locale_switch_callback) {
  CHECK(application_locale_storage);
  CHECK(profile);

  const user_manager::User* user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  CHECK(user);

  const std::string notification_id = CreateUserScopedNotificationId(
      kOOBELocaleSwitchNotificationId, user->username_hash());

  // NotifierId for histogram reporting.
  NotifierId notifier_id(NotifierType::SYSTEM_COMPONENT,
                         kOOBELocaleSwitchNotificationId,
                         NotificationCatalogName::kLocaleUpdate);
  notifier_id.profile_id = user->GetAccountId().GetUserEmail();

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
          new_locale, /*display_locale=*/application_locale_storage->Get(),
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
          application_locale_storage, std::move(new_locale), profile,
          notification_id, std::move(locale_switch_callback));

  auto notification = CreateSystemNotificationPtr(
      kNotificationType, notification_id, title, body, *kEmptyDisplaySource,
      *kEmptyOriginUrl, notifier_id, rich_notification_data, delegate, kIcon,
      kWarningLevel);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace ash
