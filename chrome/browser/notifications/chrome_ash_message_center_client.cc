// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chrome_ash_message_center_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "base/i18n/string_compare.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/arc_application_notifier_controller.h"
#include "chrome/browser/notifications/extension_notifier_controller.h"
#include "chrome/browser/notifications/web_page_notifier_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_manager/user_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using ash::mojom::NotifierUiDataPtr;
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

  bool operator()(const NotifierUiDataPtr& n1, const NotifierUiDataPtr& n2) {
    if (n1->notifier_id.type != n2->notifier_id.type)
      return n1->notifier_id.type < n2->notifier_id.type;

    if (collator_) {
      return base::i18n::CompareString16WithCollator(*collator_, n1->name,
                                                     n2->name) == UCOL_LESS;
    }
    return n1->name < n2->name;
  }

 private:
  icu::Collator* collator_;
};

}  // namespace

ChromeAshMessageCenterClient::ChromeAshMessageCenterClient(
    NotificationPlatformBridgeDelegate* delegate)
    : delegate_(delegate), binding_(this) {
  DCHECK(!g_chrome_ash_message_center_client);
  g_chrome_ash_message_center_client = this;

  // May be null in unit tests.
  auto* connection = content::ServiceManagerConnection::GetForProcess();
  if (connection) {
    connection->GetConnector()->BindInterface(ash::mojom::kServiceName,
                                              &controller_);

    // Register this object as the client interface implementation.
    ash::mojom::AshMessageCenterClientAssociatedPtrInfo ptr_info;
    binding_.Bind(mojo::MakeRequest(&ptr_info));
    controller_->SetClient(std::move(ptr_info));
  }

  sources_.insert(
      std::make_pair(NotifierId::APPLICATION,
                     std::make_unique<ExtensionNotifierController>(this)));

  sources_.insert(std::make_pair(
      NotifierId::WEB_PAGE, std::make_unique<WebPageNotifierController>(this)));

  sources_.insert(std::make_pair(
      NotifierId::ARC_APPLICATION,
      std::make_unique<arc::ArcApplicationNotifierController>(this)));
}

ChromeAshMessageCenterClient::~ChromeAshMessageCenterClient() {
  DCHECK_EQ(this, g_chrome_ash_message_center_client);
  g_chrome_ash_message_center_client = nullptr;
}

void ChromeAshMessageCenterClient::Display(
    const message_center::Notification& notification) {
  // Null in unit tests.
  if (!controller_)
    return;

  // Remove any previous mapping to |notification.id()| before inserting a new
  // one.
  base::EraseIf(
      displayed_notifications_,
      [notification](
          const std::pair<base::UnguessableToken, std::string>& pair) {
        return pair.second == notification.id();
      });

  base::UnguessableToken token = base::UnguessableToken::Create();
  displayed_notifications_[token] = notification.id();
  controller_->ShowClientNotification(notification, token);
}

void ChromeAshMessageCenterClient::Close(const std::string& notification_id) {
  controller_->CloseClientNotification(notification_id);
}

void ChromeAshMessageCenterClient::HandleNotificationClosed(
    const base::UnguessableToken& display_token,
    bool by_user) {
  auto entry = displayed_notifications_.find(display_token);
  if (entry != displayed_notifications_.end()) {
    delegate_->HandleNotificationClosed(entry->second, by_user);
    displayed_notifications_.erase(entry);
  }
}

void ChromeAshMessageCenterClient::HandleNotificationClicked(
    const std::string& id) {
  delegate_->HandleNotificationClicked(id);
}

void ChromeAshMessageCenterClient::HandleNotificationButtonClicked(
    const std::string& id,
    int button_index,
    const base::Optional<base::string16>& reply) {
  delegate_->HandleNotificationButtonClicked(id, button_index, reply);
}

void ChromeAshMessageCenterClient::HandleNotificationSettingsButtonClicked(
    const std::string& id) {
  delegate_->HandleNotificationSettingsButtonClicked(id);
}

void ChromeAshMessageCenterClient::DisableNotification(const std::string& id) {
  delegate_->DisableNotification(id);
}

void ChromeAshMessageCenterClient::SetNotifierEnabled(
    const NotifierId& notifier_id,
    bool enabled) {
  sources_[notifier_id.type]->SetNotifierEnabled(GetProfileForNotifiers(),
                                                 notifier_id, enabled);
}

void ChromeAshMessageCenterClient::GetNotifierList(
    GetNotifierListCallback callback) {
  std::vector<ash::mojom::NotifierUiDataPtr> notifiers;
  for (auto& source : sources_) {
    auto source_notifiers =
        source.second->GetNotifierList(GetProfileForNotifiers());
    for (auto& notifier : source_notifiers) {
      notifiers.push_back(std::move(notifier));
    }
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  NotifierComparator comparator(U_SUCCESS(error) ? collator.get() : nullptr);
  std::sort(notifiers.begin(), notifiers.end(), comparator);

  std::move(callback).Run(std::move(notifiers));
}

void ChromeAshMessageCenterClient::GetArcAppIdByPackageName(
    const std::string& package_name,
    GetArcAppIdByPackageNameCallback callback) {
  std::move(callback).Run(
      ArcAppListPrefs::Get(arc::ArcSessionManager::Get()->profile())
          ->GetAppIdByPackageName(package_name));
}

void ChromeAshMessageCenterClient::ShowLockScreenNotificationSettings() {
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        chrome::kLockScreenSubPage);
}

void ChromeAshMessageCenterClient::OnIconImageUpdated(
    const NotifierId& notifier_id,
    const gfx::ImageSkia& image) {
  // |controller_| may be null in unit tests.
  if (!image.isNull() && controller_)
    controller_->UpdateNotifierIcon(notifier_id, image);
}

void ChromeAshMessageCenterClient::OnNotifierEnabledChanged(
    const NotifierId& notifier_id,
    bool enabled) {
  // May be null in unit tests.
  if (controller_)
    controller_->NotifierEnabledChanged(notifier_id, enabled);
}

// static
void ChromeAshMessageCenterClient::FlushForTesting() {
  g_chrome_ash_message_center_client->binding_.FlushForTesting();
}
