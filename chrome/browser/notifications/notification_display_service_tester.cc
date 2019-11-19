// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_display_service_tester.h"

#include <set>

#include "base/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/buildflags.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Pointer to currently active tester, which is assumed to be a singleton.
NotificationDisplayServiceTester* g_tester = nullptr;

#if !BUILDFLAG(ENABLE_MESSAGE_CENTER)

// Mock implementation of the NotificationPlatformBridge interface that just
// implements the interface, and doesn't exercise behaviour of its own.
class MockNotificationPlatformBridge : public NotificationPlatformBridge {
 public:
  MockNotificationPlatformBridge() = default;
  ~MockNotificationPlatformBridge() override = default;

  // NotificationPlatformBridge implementation:
  void Display(
      NotificationHandler::Type notification_type,
      Profile* profile,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) override {}
  void Close(Profile* profile, const std::string& notification_id) override {}
  void GetDisplayed(Profile* profile,
                    GetDisplayedNotificationsCallback callback) const override {
    std::set<std::string> displayed_notifications;
    std::move(callback).Run(std::move(displayed_notifications),
                            false /* supports_synchronization */);
  }
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override {
    std::move(callback).Run(true /* ready */);
  }
  void DisplayServiceShutDown(Profile* profile) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNotificationPlatformBridge);
};

#endif  // !BUILDFLAG(ENABLE_MESSAGE_CENTER)

class NotificationDisplayServiceShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
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

  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayServiceShutdownNotifierFactory);
};

}  // namespace

NotificationDisplayServiceTester::NotificationDisplayServiceTester(
    Profile* profile)
    : profile_(profile) {
#if !BUILDFLAG(ENABLE_MESSAGE_CENTER)
  TestingBrowserProcess* browser_process = TestingBrowserProcess::GetGlobal();
  if (browser_process) {
    browser_process->SetNotificationPlatformBridge(
        std::make_unique<MockNotificationPlatformBridge>());
  }
#endif

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

base::Optional<message_center::Notification>
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
    base::Optional<int> action_index,
    base::Optional<base::string16> reply) {
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
  profile_shutdown_subscription_.reset();
}
