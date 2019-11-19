// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chrome_ash_message_center_client.h"

#include "ash/public/cpp/notifier_metadata.h"
#include "ash/public/cpp/notifier_settings_observer.h"
#include "base/i18n/string_compare.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/arc_application_notifier_controller.h"
#include "chrome/browser/notifications/extension_notifier_controller.h"
#include "chrome/browser/notifications/web_page_notifier_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::MessageCenter;
using message_center::NotifierId;

namespace {

// The singleton instance, which is tracked to allow access from tests.
ChromeAshMessageCenterClient* g_chrome_ash_message_center_client = nullptr;

// All notifier actions are performed on the notifiers for the currently active
// profile, so this just returns the active profile.
Profile* GetProfileForNotifiers() {
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetActiveUser());
}

class NotifierComparator {
 public:
  explicit NotifierComparator(icu::Collator* collator) : collator_(collator) {}

  bool operator()(const ash::NotifierMetadata& n1,
                  const ash::NotifierMetadata& n2) {
    if (n1.notifier_id.type != n2.notifier_id.type)
      return n1.notifier_id.type < n2.notifier_id.type;

    if (collator_) {
      return base::i18n::CompareString16WithCollator(*collator_, n1.name,
                                                     n2.name) == UCOL_LESS;
    }
    return n1.name < n2.name;
  }

 private:
  icu::Collator* collator_;
};

// This delegate forwards NotificationDelegate methods to their equivalent in
// NotificationPlatformBridgeDelegate.
class ForwardingNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  ForwardingNotificationDelegate(const std::string& notification_id,
                                 NotificationPlatformBridgeDelegate* delegate)
      : notification_id_(notification_id), delegate_(delegate) {}

  // message_center::NotificationDelegate:
  void Close(bool by_user) override {
    delegate_->HandleNotificationClosed(notification_id_, by_user);
  }

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override {
    if (button_index) {
      delegate_->HandleNotificationButtonClicked(notification_id_,
                                                 *button_index, reply);
    } else {
      delegate_->HandleNotificationClicked(notification_id_);
    }
  }

  void SettingsClick() override {
    delegate_->HandleNotificationSettingsButtonClicked(notification_id_);
  }

  void DisableNotification() override {
    delegate_->DisableNotification(notification_id_);
  }

 private:
  ~ForwardingNotificationDelegate() override = default;

  // The ID of the notification.
  const std::string notification_id_;

  NotificationPlatformBridgeDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingNotificationDelegate);
};

}  // namespace

ChromeAshMessageCenterClient::ChromeAshMessageCenterClient(
    NotificationPlatformBridgeDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(!g_chrome_ash_message_center_client);
  g_chrome_ash_message_center_client = this;

  sources_.insert(
      std::make_pair(message_center::NotifierType::APPLICATION,
                     std::make_unique<ExtensionNotifierController>(this)));

  sources_.insert(
      std::make_pair(message_center::NotifierType::WEB_PAGE,
                     std::make_unique<WebPageNotifierController>(this)));

  sources_.insert(std::make_pair(
      message_center::NotifierType::ARC_APPLICATION,
      std::make_unique<arc::ArcApplicationNotifierController>(this)));
}

ChromeAshMessageCenterClient::~ChromeAshMessageCenterClient() {
  DCHECK_EQ(this, g_chrome_ash_message_center_client);
  g_chrome_ash_message_center_client = nullptr;
}

void ChromeAshMessageCenterClient::Display(
    const message_center::Notification& notification) {
  auto message_center_notification =
      std::make_unique<message_center::Notification>(
          base::WrapRefCounted(
              new ForwardingNotificationDelegate(notification.id(), delegate_)),
          notification);
  MessageCenter::Get()->AddNotification(std::move(message_center_notification));
}

void ChromeAshMessageCenterClient::Close(const std::string& notification_id) {
  // During shutdown, Ash is destroyed before |this|, taking the MessageCenter
  // with it.
  if (MessageCenter::Get()) {
    MessageCenter::Get()->RemoveNotification(notification_id,
                                             false /* by_user */);
  }
}

void ChromeAshMessageCenterClient::GetNotifiers() {
  if (!notifier_observers_.might_have_observers())
    return;

  Profile* profile = GetProfileForNotifiers();
  if (!profile) {
    user_manager::UserManager::Get()
        ->GetActiveUser()
        ->AddProfileCreatedObserver(
            base::BindOnce(&ChromeAshMessageCenterClient::GetNotifiers,
                           weak_ptr_.GetWeakPtr()));
    LOG(ERROR) << "GetNotifiers called before profile fully loaded, see "
                  "https://crbug.com/968825";
    return;
  }

  std::vector<ash::NotifierMetadata> notifiers;
  for (auto& source : sources_) {
    auto source_notifiers = source.second->GetNotifierList(profile);
    for (auto& notifier : source_notifiers)
      notifiers.push_back(std::move(notifier));
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  NotifierComparator comparator(U_SUCCESS(error) ? collator.get() : nullptr);
  std::sort(notifiers.begin(), notifiers.end(), comparator);

  for (auto& observer : notifier_observers_)
    observer.OnNotifiersUpdated(notifiers);
}

void ChromeAshMessageCenterClient::SetNotifierEnabled(
    const NotifierId& notifier_id,
    bool enabled) {
  sources_[notifier_id.type]->SetNotifierEnabled(GetProfileForNotifiers(),
                                                 notifier_id, enabled);
}

void ChromeAshMessageCenterClient::AddNotifierSettingsObserver(
    ash::NotifierSettingsObserver* observer) {
  notifier_observers_.AddObserver(observer);
}

void ChromeAshMessageCenterClient::RemoveNotifierSettingsObserver(
    ash::NotifierSettingsObserver* observer) {
  notifier_observers_.RemoveObserver(observer);
}

void ChromeAshMessageCenterClient::OnIconImageUpdated(
    const NotifierId& notifier_id,
    const gfx::ImageSkia& image) {
  for (auto& observer : notifier_observers_)
    observer.OnNotifierIconUpdated(notifier_id, image);
}

void ChromeAshMessageCenterClient::OnNotifierEnabledChanged(
    const NotifierId& notifier_id,
    bool enabled) {
  if (!enabled)
    MessageCenter::Get()->RemoveNotificationsForNotifierId(notifier_id);
}
