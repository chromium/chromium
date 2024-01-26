// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_tester.h"

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Pointer to currently active tester, which is assumed to be a singleton.
NotificationDisplayServiceTester* g_tester = nullptr;

class NotificationDisplayServiceShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  NotificationDisplayServiceShutdownNotifierFactory(
      const NotificationDisplayServiceShutdownNotifierFactory&) = delete;
  NotificationDisplayServiceShutdownNotifierFactory& operator=(
      const NotificationDisplayServiceShutdownNotifierFactory&) = delete;
  static NotificationDisplayServiceShutdownNotifierFactory* GetInstance() {
    return base::Singleton<
        NotificationDisplayServiceShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<
      NotificationDisplayServiceShutdownNotifierFactory>;

  NotificationDisplayServiceShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "NotificationDisplayService") {
    DependsOn(NotificationDisplayServiceFactory::GetInstance());
  }
  ~NotificationDisplayServiceShutdownNotifierFactory() override {}
};

}  // namespace

NotificationDisplayServiceTester::NotificationDisplayServiceTester(
    Profile* profile)
    : profile_(profile) {
  // TODO(peter): Remove the StubNotificationDisplayService in favor of having
  // a fully functional MockNotificationPlatformBridge.
  if (profile_) {
    display_service_ = static_cast<StubNotificationDisplayService*>(
        NotificationDisplayServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_,
                base::BindRepeating(
                    &StubNotificationDisplayService::FactoryForTests)));

    profile_shutdown_subscription_ =
        NotificationDisplayServiceShutdownNotifierFactory::GetInstance()
            ->Get(profile)
            ->Subscribe(base::BindRepeating(
                &NotificationDisplayServiceTester::OnProfileShutdown,
                base::Unretained(this)));
  } else {
    auto system_display_service =
        std::make_unique<StubNotificationDisplayService>(nullptr);
    display_service_ = system_display_service.get();
    SystemNotificationHelper::GetInstance()->SetSystemServiceForTesting(
        std::move(system_display_service));
  }

  g_tester = this;
}

NotificationDisplayServiceTester::~NotificationDisplayServiceTester() {
  g_tester = nullptr;
  if (profile_) {
    NotificationDisplayServiceFactory::GetInstance()->SetTestingFactory(
        profile_, BrowserContextKeyedServiceFactory::TestingFactory());
  }
}

// static
NotificationDisplayServiceTester* NotificationDisplayServiceTester::Get() {
  return g_tester;
}

void NotificationDisplayServiceTester::SetNotificationAddedClosure(
    base::RepeatingClosure closure) {
  display_service_->SetNotificationAddedClosure(std::move(closure));
}

void NotificationDisplayServiceTester::SetNotificationClosedClosure(
    base::RepeatingClosure closure) {
  display_service_->SetNotificationClosedClosure(std::move(closure));
}

std::vector<message_center::Notification>
NotificationDisplayServiceTester::GetDisplayedNotificationsForType(
    NotificationHandler::Type type) {
  return display_service_->GetDisplayedNotificationsForType(type);
}

std::optional<message_center::Notification>
NotificationDisplayServiceTester::GetNotification(
    const std::string& notification_id) const {
  return display_service_->GetNotification(notification_id);
}

const NotificationCommon::Metadata*
NotificationDisplayServiceTester::GetMetadataForNotification(
    const message_center::Notification& notification) {
  return display_service_->GetMetadataForNotification(notification);
}

void NotificationDisplayServiceTester::SimulateClick(
    NotificationHandler::Type notification_type,
    const std::string& notification_id,
    std::optional<int> action_index,
    std::optional<std::u16string> reply) {
  display_service_->SimulateClick(notification_type, notification_id,
                                  std::move(action_index), std::move(reply));
}

void NotificationDisplayServiceTester::SimulateSettingsClick(
    NotificationHandler::Type notification_type,
    const std::string& notification_id) {
  display_service_->SimulateSettingsClick(notification_type, notification_id);
}

void NotificationDisplayServiceTester::RemoveNotification(
    NotificationHandler::Type type,
    const std::string& notification_id,
    bool by_user,
    bool silent) {
  display_service_->RemoveNotification(type, notification_id, by_user, silent);
}

void NotificationDisplayServiceTester::RemoveAllNotifications(
    NotificationHandler::Type type,
    bool by_user) {
  display_service_->RemoveAllNotifications(type, by_user);
}

void NotificationDisplayServiceTester::SetProcessNotificationOperationDelegate(
    const StubNotificationDisplayService::ProcessNotificationOperationCallback&
        delegate) {
  display_service_->SetProcessNotificationOperationDelegate(delegate);
}

void NotificationDisplayServiceTester::OnProfileShutdown() {
  profile_ = nullptr;
  profile_shutdown_subscription_ = {};
}

// static
void NotificationDisplayServiceTester::EnsureFactoryBuilt() {
  NotificationDisplayServiceShutdownNotifierFactory::GetInstance();
}
