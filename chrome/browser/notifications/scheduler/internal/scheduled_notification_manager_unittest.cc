// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chrome/browser/notifications/scheduler/internal/collection_store.h"
#include "chrome/browser/notifications/scheduler/internal/icon_store.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::Invoke;
using Entries = std::vector<std::unique_ptr<notifications::NotificationEntry>>;

namespace notifications {
namespace {

const char kGuid[] = "test_guid_1234";
const char kNonExistentGuid[] = "guid_non_existent";
const char16_t kTitle[] = u"test_title";
const char kSmallIconUuid[] = "test_small_icon_uuid";
const char kLargeIconUuid[] = "test_large_icon_uuid";

NotificationEntry CreateNotificationEntry(SchedulerClientType type) {
  NotificationEntry entry(type,
                          base::Uuid::GenerateRandomV4().AsLowercaseString());
  entry.schedule_params.deliver_time_start = base::Time::Now() + base::Days(1);
  entry.schedule_params.deliver_time_end = base::Time::Now() + base::Days(2);
  return entry;
}

// Verifies notification entry is same as expected.
void VerifyNotificationEntry(const NotificationEntry* entry,
                             const NotificationEntry* expected) {
  if (expected == nullptr) {
    EXPECT_FALSE(entry);
    return;
  }

  EXPECT_TRUE(entry);
  EXPECT_EQ(*entry, *expected);
  const auto& entry_icons = entry->notification_data.icons;
  const auto& expected_icons = expected->notification_data.icons;
  for (const auto& icon : entry_icons) {
    auto icon_type = icon.first;
    EXPECT_TRUE(base::Contains(expected_icons, icon_type));
    EXPECT_EQ(entry_icons.at(icon_type).bitmap.width(),
              expected_icons.at(icon_type).bitmap.width());
    EXPECT_EQ(entry_icons.at(icon_type).bitmap.height(),
              expected_icons.at(icon_type).bitmap.height());
  }
}

IconStore::IconTypeBundleMap CreateIcons() {
  IconStore::IconTypeBundleMap result;
  SkBitmap large_icon;
  large_icon.allocN32Pixels(12, 13);
  SkBitmap small_icon;
  large_icon.allocN32Pixels(3, 4);
  result.emplace(IconType::kLargeIcon, IconBundle(std::move(large_icon)));
  result.emplace(IconType::kSmallIcon, IconBundle(std::move(small_icon)));
  return result;
}

class MockNotificationStore : public CollectionStore<NotificationEntry> {
 public:
  MockNotificationStore() {}
  MockNotificationStore(const MockNotificationStore&) = delete;
  MockNotificationStore& operator=(const MockNotificationStore&) = delete;

  MOCK_METHOD1(InitAndLoad,
               void(CollectionStore<NotificationEntry>::LoadCallback));
  MOCK_METHOD3(Add,
               void(const std::string&,
                    const NotificationEntry&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD3(Update,
               void(const std::string&,
                    const NotificationEntry&,
                    base::OnceCallback<void(bool)>));
  MOCK_METHOD2(Delete,
               void(const std::string&, base::OnceCallback<void(bool)>));
};

class MockIconStore : public IconStore {
 public:
  MockIconStore() {}
  MockIconStore(const MockIconStore&) = delete;
  MockIconStore& operator=(const MockIconStore&) = delete;

  MOCK_METHOD1(InitAndLoadKeys, void(IconStore::InitAndLoadKeysCallback));
  MOCK_METHOD2(LoadIcons,
               void(const std::vector<std::string>&,
                    IconStore::LoadIconsCallback));
  MOCK_METHOD2(AddIcons,
               void(IconStore::IconTypeBundleMap, IconStore::AddCallback));
  MOCK_METHOD2(DeleteIcons,
               void(const std::vector<std::string>&,
                    IconStore::UpdateCallback));
};

class ScheduledNotificationManagerTest : public testing::Test {
 public:
  ScheduledNotificationManagerTest()
      : notification_store_(nullptr), icon_store_(nullptr) {}
  ScheduledNotificationManagerTest(const ScheduledNotificationManagerTest&) =
      delete;
  ScheduledNotificationManagerTest& operator=(
      const ScheduledNotificationManagerTest&) = delete;
  ~ScheduledNotificationManagerTest() override = default;

  void SetUp() override {
    auto notification_store = std::make_unique<MockNotificationStore>();
    auto icon_store = std::make_unique<MockIconStore>();
    notification_store_ = notification_store.get();
    icon_store_ = icon_store.get();
    config_.notification_expiration = base::Days(1);
    manager_ = ScheduledNotificationManager::Create(
        std::move(notification_store), std::move(icon_store),
        {SchedulerClientType::kTest1, SchedulerClientType::kTest2}, config_);
  }

 protected:
  ScheduledNotificationManager* manager() { return manager_.get(); }
  MockNotificationStore* notification_store() { return notification_store_; }
  MockIconStore* icon_store() { return icon_store_; }
  const SchedulerConfig& config() const { return config_; }

  // Initializes the manager with predefined data in the store.
  void InitWithData(std::vector<NotificationEntry> data) {
    Entries entries;
    auto icon_keys = std::make_unique<std::vector<std::string>>();
    for (auto it = data.begin(); it != data.end(); ++it) {
      for (const auto& pair : it->icons_uuid) {
        icon_keys->emplace_back(pair.second);
      }
      auto entry_ptr = std::make_unique<NotificationEntry>(it->type, it->guid);
      *(entry_ptr.get()) = *it;
      entries.emplace_back(std::move(entry_ptr));
    }

    // Initialize the store and call the callback.
    EXPECT_CALL(*notification_store(), InitAndLoad(_))
        .WillOnce(
            Invoke([&entries](base::OnceCallback<void(bool, Entries)> cb) {
              std::move(cb).Run(true, std::move(entries));
            }));
    EXPECT_CALL(*icon_store(), InitAndLoadKeys(_))
        .WillOnce(Invoke(
            [&icon_keys](
                base::OnceCallback<void(bool, IconStore::LoadedIconKeys)> cb) {
              std::move(cb).Run(true, std::move(icon_keys));
            }));
    EXPECT_CALL(*icon_store(), DeleteIcons(_, _)).RetiresOnSaturation();

    base::RunLoop loop;
    manager()->Init(base::BindOnce(
        [](base::RepeatingClosure closure, bool success) {
          EXPECT_TRUE(success);
          std::move(closure).Run();
        },
        loop.QuitClosure()));
    loop.Run();
  }

  // Schedules a notification and wait until the callback is called.
  void ScheduleNotification(std::unique_ptr<NotificationParams> params,
                            bool expected_success) {
    base::RunLoop loop;
    auto schedule_callback = base::BindOnce(
        [](base::RepeatingClosure quit_closure, bool expected_success,
           bool success) {
          EXPECT_EQ(success, expected_success);
          quit_closure.Run();
        },
        loop.QuitClosure(), expected_success);
    manager()->ScheduleNotification(std::move(params),
                                    std::move(schedule_callback));
    loop.Run();
  }

  void DisplayNotification(const std::string& guid,
                           const NotificationEntry* expected_entry) {
    base::RunLoop loop;
    auto display_callback = base::BindOnce(
        [](base::RepeatingClosure quit_closure,
           const NotificationEntry* expected_entry,
           std::unique_ptr<NotificationEntry> entry) {
          VerifyNotificationEntry(entry.get(), expected_entry);
          quit_closure.Run();
        },
        loop.QuitClosure(), expected_entry);
    manager()->DisplayNotification(guid, std::move(display_callback));
    loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  raw_ptr<MockNotificationStore, DanglingUntriaged> notification_store_;
  raw_ptr<MockIconStore, DanglingUntriaged> icon_store_;
  std::vector<SchedulerClientType> clients_;
  std::unique_ptr<ScheduledNotificationManager> manager_;
  SchedulerConfig config_;
};

// Verify that error is received when notification database failed to
// initialize.
TEST_F(ScheduledNotificationManagerTest, NotificationDbInitFailed) {
  EXPECT_CALL(*notification_store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(bool, Entries)> callback) {
        std::move(callback).Run(false, Entries());
      }));

  EXPECT_CALL(*icon_store(), InitAndLoadKeys(_))
      .WillOnce(Invoke(
          [](base::OnceCallback<void(bool, IconStore::LoadedIconKeys)> cb) {
            std::move(cb).Run(true, nullptr);
          }));
  base::RunLoop loop;
  manager()->Init(base::BindOnce(
      [](base::RepeatingClosure closure, bool success) {
        // Expected to receive error.
        EXPECT_FALSE(success);
        std::move(closure).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

// Verify that error is received when icon database failed to initialize.
TEST_F(ScheduledNotificationManagerTest, IconDbInitFailed) {
  ON_CALL(*notification_store(), InitAndLoad(_))
      .WillByDefault(
          Invoke([](base::OnceCallback<void(bool, Entries)> callback) {
            std::move(callback).Run(true, Entries());
          }));

  EXPECT_CALL(*icon_store(), InitAndLoadKeys(_))
      .WillOnce(Invoke(
          [](base::OnceCallback<void(bool, IconStore::LoadedIconKeys)> cb) {
            std::move(cb).Run(false, nullptr);
          }));
  base::RunLoop loop;
  manager()->Init(base::BindOnce(
      [](base::RepeatingClosure closure, bool success) {
        // Expected to receive error.
        EXPECT_FALSE(success);
        std::move(closure).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

// Verify that residual expired icons are deleted when icon database is
// initialized.
TEST_F(ScheduledNotificationManagerTest, IconDbInitAndLoadKeys) {
  EXPECT_CALL(*notification_store(), InitAndLoad(_))
      .WillOnce(Invoke([](base::OnceCallback<void(bool, Entries)> callback) {
        std::move(callback).Run(true, Entries());
      }));

  auto icon_keys = std::make_unique<std::vector<std::string>>();
  auto* icon_keys_ptr = icon_keys.get();
  icon_keys->emplace_back(kSmallIconUuid);
  icon_keys->emplace_back(kLargeIconUuid);
  EXPECT_CALL(*icon_store(), InitAndLoadKeys(_))
      .WillOnce(Invoke(
          [&icon_keys](
              base::OnceCallback<void(bool, IconStore::LoadedIconKeys)> cb) {
            std::move(cb).Run(true, std::move(icon_keys));
          }));
  EXPECT_CALL(*icon_store(), DeleteIcons(*icon_keys_ptr, _));
  base::RunLoop loop;
  manager()->Init(base::BindOnce(
      [](base::RepeatingClosure closure, bool success) {
        EXPECT_TRUE(success);
        std::move(closure).Run();
      },
      loop.QuitClosure()));
  loop.Run();
}

// Test to schedule a notification.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotification) {
  InitWithData(std::vector<NotificationEntry>());
  NotificationData notification_data;
  notification_data.title = kTitle;
  ScheduleParams schedule_params;
  schedule_params.priority = ScheduleParams::Priority::kLow;
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, notification_data, schedule_params);
  params->schedule_params.deliver_time_start = base::Time::Now();
  params->schedule_params.deliver_time_end = base::Time::Now() + base::Days(1);

  params->enable_ihnr_buttons = true;
  std::string guid = params->guid;
  EXPECT_FALSE(guid.empty());

  // Verify call contract.
  EXPECT_CALL(*icon_store(), AddIcons(_, _))
      .WillOnce(Invoke([](IconStore::IconTypeBundleMap icons,
                          IconStore::AddCallback callback) {
        std::move(callback).Run(IconStore::IconTypeUuidMap{}, true);
      }));
  EXPECT_CALL(*notification_store(), Add(guid, _, _))
      .WillOnce(Invoke([guid](const std::string&, const NotificationEntry&,
                              base::OnceCallback<void(bool)> cb) {
        std::move(cb).Run(true);
      }));
  ScheduleNotification(std::move(params), true /*expected_success*/);

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  const NotificationEntry* entry = *(notifications.begin()->second.begin());
  EXPECT_EQ(entry->guid, guid);
  EXPECT_NE(entry->create_time, base::Time());

  // TODO(xingliu): change these to compare with operator==.
  EXPECT_EQ(entry->notification_data.title, kTitle);
  EXPECT_EQ(entry->schedule_params.priority, ScheduleParams::Priority::kLow);

  // Verify that |enable_ihnr_buttons| will add the helpful/unhelpful buttons.
  auto buttons = entry->notification_data.buttons;
  EXPECT_EQ(buttons.size(), 2u);
  EXPECT_EQ(buttons[0].id, notifications::kDefaultHelpfulButtonId);
  EXPECT_EQ(buttons[0].type, ActionButtonType::kHelpful);
  EXPECT_EQ(buttons[1].id, notifications::kDefaultUnhelpfulButtonId);
  EXPECT_EQ(buttons[1].type, ActionButtonType::kUnhelpful);
}

// Verifies that schedules a notification with unregistered client will fail.
TEST_F(ScheduledNotificationManagerTest, ScheduleInvalidNotification) {
  InitWithData(std::vector<NotificationEntry>());
  NotificationData notification_data;
  notification_data.title = kTitle;
  ScheduleParams schedule_params;
  // Client type kTest3 is not registered.
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest3, notification_data, schedule_params);

  EXPECT_CALL(*icon_store(), AddIcons(_, _)).Times(0);
  EXPECT_CALL(*notification_store(), Add(_, _, _)).Times(0);
  ScheduleNotification(std::move(params), false /*expected_success*/);
}

// Verifies that schedules a notification with duplicate guid will fail.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotificationDuplicateGuid) {
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry.guid = kGuid;
  InitWithData(std::vector<NotificationEntry>({entry}));

  NotificationData notification_data;
  notification_data.title = kTitle;
  ScheduleParams schedule_params;
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, notification_data, schedule_params);
  params->schedule_params.deliver_time_start = base::Time::Now();
  params->schedule_params.deliver_time_end = base::Time::Now() + base::Days(1);
  // Duplicate guid.
  params->guid = kGuid;

  EXPECT_CALL(*icon_store(), AddIcons(_, _)).Times(0);
  EXPECT_CALL(*notification_store(), Add(_, _, _)).Times(0);
  ScheduleNotification(std::move(params), false /*expected_success*/);
}

// Test to schedule a notification without guid, we will auto generated one.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotificationEmptyGuid) {
  InitWithData(std::vector<NotificationEntry>());
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, NotificationData(), ScheduleParams());
  params->schedule_params.deliver_time_start = base::Time::Now();
  params->schedule_params.deliver_time_end = base::Time::Now() + base::Days(1);

  // Verify call contract.
  EXPECT_CALL(*icon_store(), AddIcons(_, _))
      .WillOnce(Invoke([](IconStore::IconTypeBundleMap icons,
                          IconStore::AddCallback callback) {
        std::move(callback).Run(IconStore::IconTypeUuidMap{}, true);
      }));
  EXPECT_CALL(*notification_store(), Add(_, _, _))
      .WillOnce(Invoke(
          [&](const std::string&, const NotificationEntry&,
              base::OnceCallback<void(bool)> cb) { std::move(cb).Run(true); }));

  ScheduleNotification(std::move(params), true /*expected_success*/);

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  const NotificationEntry* entry = *(notifications.begin()->second.begin());
  EXPECT_NE(entry->guid, std::string());
  EXPECT_NE(entry->create_time, base::Time());
}

// Test to display a notification.
TEST_F(ScheduledNotificationManagerTest, DisplayNotification) {
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry.guid = kGuid;
  InitWithData(std::vector<NotificationEntry>({entry}));

  EXPECT_CALL(*icon_store(), LoadIcons(_, _))
      .WillOnce(Invoke([](std::vector<std::string> keys,
                          IconStore::LoadIconsCallback callback) {
        std::move(callback).Run(true, IconStore::LoadedIconsMap());
      }));
  EXPECT_CALL(*notification_store(), Delete(kGuid, _));
  EXPECT_CALL(*icon_store(), DeleteIcons(_, _));

  DisplayNotification(kGuid, &entry);

  // Before display, the notification is removed from the store.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_TRUE(notifications.empty());
}

// Test to display non-existing notification.
TEST_F(ScheduledNotificationManagerTest, DisplayNotificationWithoutEntry) {
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry.guid = kGuid;
  InitWithData(std::vector<NotificationEntry>({entry}));

  EXPECT_CALL(*icon_store(), LoadIcons(_, _)).Times(0);
  EXPECT_CALL(*notification_store(), Delete(_, _)).Times(0);
  EXPECT_CALL(*icon_store(), DeleteIcons(_, _)).Times(0);

  DisplayNotification(kNonExistentGuid, nullptr);

  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(*notifications.at(SchedulerClientType::kTest1).back(), entry);
}

// Verify GetAllNotifications API, the notification should be sorted based on
// creation timestamp.
TEST_F(ScheduledNotificationManagerTest, GetAllNotifications) {
  // Ordering: entry1.create_time < entry0.create_time < entry2.create_time.
  auto now = base::Time::Now();
  auto entry0 = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry0.create_time = now;
  auto entry1 = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry1.create_time = now - base::Minutes(1);
  auto entry2 = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry2.create_time = now + base::Minutes(1);

  InitWithData(std::vector<NotificationEntry>({entry0, entry1, entry2}));
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  EXPECT_EQ(notifications.begin()->second.size(), 3u);

  // Entries returned by GetAllNotifications() should be sorted by creation
  // timestamp.
  const NotificationEntry* output0 = notifications.begin()->second.front();
  const NotificationEntry* output2 = notifications.begin()->second.back();
  EXPECT_EQ(output0->create_time, entry1.create_time);
  EXPECT_EQ(output2->create_time, entry2.create_time);
}

// Verifies GetNotifications() can return the correct values.
TEST_F(ScheduledNotificationManagerTest, GetNotifications) {
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  InitWithData(std::vector<NotificationEntry>({entry}));
  std::vector<const NotificationEntry*> entries;
  manager()->GetNotifications(SchedulerClientType::kTest2, &entries);
  EXPECT_TRUE(entries.empty());

  manager()->GetNotifications(SchedulerClientType::kTest1, &entries);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(*entries.back(), entry);

  manager()->GetNotifications(SchedulerClientType::kTest2, &entries);
  EXPECT_TRUE(entries.empty());
}

// Verify DeleteNotifications API, all notifications with given
// SchedulerClientType should be deleted.
TEST_F(ScheduledNotificationManagerTest, DeleteNotifications) {
  // Type1: entry0
  // Type2: entry1, entry2
  auto entry0 = CreateNotificationEntry(SchedulerClientType::kTest1);
  auto entry1 = CreateNotificationEntry(SchedulerClientType::kTest2);
  auto entry2 = CreateNotificationEntry(SchedulerClientType::kTest2);
  InitWithData(std::vector<NotificationEntry>({entry0, entry1, entry2}));
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 2u);

  EXPECT_CALL(*notification_store(), Delete(_, _))
      .Times(2)
      .RetiresOnSaturation();
  EXPECT_CALL(*icon_store(), DeleteIcons(_, _)).Times(2).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest2);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);

  // Ensure deleting non-existing key will not crash, and store will not call
  // Delete.
  EXPECT_CALL(*notification_store(), Delete(_, _))
      .Times(0)
      .RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest2);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);

  EXPECT_CALL(*notification_store(), Delete(_, _)).RetiresOnSaturation();
  EXPECT_CALL(*icon_store(), DeleteIcons(_, _)).RetiresOnSaturation();
  manager()->DeleteNotifications(SchedulerClientType::kTest1);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 0u);

  // Unregistered client.
  EXPECT_CALL(*notification_store(), Delete(_, _)).Times(0);
  manager()->DeleteNotifications(SchedulerClientType::kTest3);
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 0u);
}

// Verifies that unused notifications will be deleted during initialization.
TEST_F(ScheduledNotificationManagerTest, PruneNotifications) {
  // Type1: entry0
  // Type2: entry1(invalid), entry2(expired), entry3
  // Type3: entry4(unregistered client)
  auto now = base::Time::Now();
  auto entry0 = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry0.create_time = now - base::Hours(12);
  auto entry1 = CreateNotificationEntry(SchedulerClientType::kTest2);
  entry1.create_time = now - base::Hours(14);
  entry1.schedule_params.deliver_time_start = base::Time::Now() - base::Days(2);
  entry1.schedule_params.deliver_time_end = base::Time::Now() - base::Days(1);
  auto entry2 = CreateNotificationEntry(SchedulerClientType::kTest2);
  entry2.create_time = now - base::Hours(24);
  auto entry3 = CreateNotificationEntry(SchedulerClientType::kTest2);
  entry3.create_time = now - base::Hours(23);
  auto entry4 = CreateNotificationEntry(SchedulerClientType::kTest3);

  EXPECT_CALL(*notification_store(), Delete(_, _)).Times(3);
  EXPECT_CALL(*icon_store(), DeleteIcons(_, _)).Times(3).RetiresOnSaturation();
  InitWithData(
      std::vector<NotificationEntry>({entry0, entry1, entry2, entry3, entry4}));
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 2u);
  EXPECT_EQ(notifications[SchedulerClientType::kTest1].size(), 1u);
  EXPECT_EQ(notifications[SchedulerClientType::kTest2].size(), 1u);
}

// Test to schedule a notification with two icons in notification data.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotificationWithIcons) {
  InitWithData(std::vector<NotificationEntry>());
  NotificationData notification_data;
  notification_data.icons = CreateIcons();
  ScheduleParams schedule_params;
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, notification_data, schedule_params);
  params->schedule_params.deliver_time_start = base::Time::Now();
  params->schedule_params.deliver_time_end = base::Time::Now() + base::Days(1);

  std::string guid = params->guid;
  EXPECT_FALSE(guid.empty());

  // Verify call contract.
  EXPECT_CALL(*icon_store(), AddIcons(_, _))
      .WillOnce(Invoke([](IconStore::IconTypeBundleMap icons,
                          IconStore::AddCallback callback) {
        IconStore::IconTypeUuidMap icons_uuid_map;
        for (const auto& pair : icons) {
          if (pair.first == IconType::kLargeIcon)
            icons_uuid_map.emplace(pair.first, kLargeIconUuid);
          else if (pair.first == IconType::kSmallIcon)
            icons_uuid_map.emplace(pair.first, kSmallIconUuid);
        }
        std::move(callback).Run(std::move(icons_uuid_map), true);
      }));

  EXPECT_CALL(*notification_store(), Add(guid, _, _))
      .WillOnce(Invoke([guid](const std::string&, const NotificationEntry&,
                              base::OnceCallback<void(bool)> cb) {
        std::move(cb).Run(true);
      }));
  ScheduleNotification(std::move(params), true /*expected_success*/);

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_EQ(notifications.size(), 1u);
  const NotificationEntry* entry = *(notifications.begin()->second.begin());
  EXPECT_EQ(entry->guid, guid);
  EXPECT_NE(entry->create_time, base::Time());
  EXPECT_EQ(entry->icons_uuid.size(), 2u);
  EXPECT_EQ(entry->icons_uuid.at(IconType::kLargeIcon), kLargeIconUuid);
  EXPECT_EQ(entry->icons_uuid.at(IconType::kSmallIcon), kSmallIconUuid);
}

// Test to schedule a notification failed on saving icons.
TEST_F(ScheduledNotificationManagerTest, ScheduleNotificationWithIconsFailed) {
  InitWithData(std::vector<NotificationEntry>());
  NotificationData notification_data;
  notification_data.icons = CreateIcons();
  ScheduleParams schedule_params;
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, notification_data, schedule_params);
  params->schedule_params.deliver_time_start = base::Time::Now();
  params->schedule_params.deliver_time_end = base::Time::Now() + base::Days(1);

  // Verify call contract.
  EXPECT_CALL(*icon_store(), AddIcons(_, _))
      .WillOnce(Invoke([](IconStore::IconTypeBundleMap icons,
                          IconStore::AddCallback callback) {
        IconStore::IconTypeUuidMap icons_uuid_map;
        for (const auto& pair : icons) {
          if (pair.first == IconType::kLargeIcon)
            icons_uuid_map.emplace(pair.first, kLargeIconUuid);
          else if (pair.first == IconType::kSmallIcon)
            icons_uuid_map.emplace(pair.first, kSmallIconUuid);
        }
        std::move(callback).Run(std::move(icons_uuid_map), false);
      }));

  ScheduleNotification(std::move(params), false /*expected_success*/);

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_TRUE(notifications.empty());
}

// Verifies the case that failed to add to the notification store.
TEST_F(ScheduledNotificationManagerTest, ScheduleAddNotificationFailed) {
  InitWithData(std::vector<NotificationEntry>());
  NotificationData notification_data;
  notification_data.icons = CreateIcons();
  ScheduleParams schedule_params;
  auto params = std::make_unique<NotificationParams>(
      SchedulerClientType::kTest1, notification_data, schedule_params);
  params->schedule_params.deliver_time_start = base::Time::Now();
  params->schedule_params.deliver_time_end = base::Time::Now() + base::Days(1);

  // Succeeded to add icons.
  EXPECT_CALL(*icon_store(), AddIcons(_, _))
      .WillOnce(Invoke([](IconStore::IconTypeBundleMap icons,
                          IconStore::AddCallback callback) {
        IconStore::IconTypeUuidMap icons_uuid_map;
        for (const auto& pair : icons) {
          if (pair.first == IconType::kLargeIcon)
            icons_uuid_map.emplace(pair.first, kLargeIconUuid);
          else if (pair.first == IconType::kSmallIcon)
            icons_uuid_map.emplace(pair.first, kSmallIconUuid);
        }
        std::move(callback).Run(std::move(icons_uuid_map), true);
      }));

  std::vector<std::string> icons_to_delete{kSmallIconUuid, kLargeIconUuid};
  EXPECT_CALL(*icon_store(), DeleteIcons(icons_to_delete, _));

  // Failed to add notifications.
  EXPECT_CALL(*notification_store(), Add(_, _, _))
      .WillOnce(Invoke(
          [](const std::string&, const NotificationEntry&,
             base::OnceCallback<void(bool)> cb) { std::move(cb).Run(false); }));

  ScheduleNotification(std::move(params), false /*expected_success*/);

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_TRUE(notifications.empty());
}

// Test to display a notification with icons.
TEST_F(ScheduledNotificationManagerTest, DisplayNotificationWithIcons) {
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry.guid = kGuid;
  entry.icons_uuid[IconType::kLargeIcon] = kLargeIconUuid;
  entry.icons_uuid[IconType::kSmallIcon] = kSmallIconUuid;
  InitWithData(std::vector<NotificationEntry>({entry}));

  auto icons = CreateIcons();
  EXPECT_CALL(*icon_store(), LoadIcons(_, _))
      .WillOnce(Invoke([&icons](std::vector<std::string> keys,
                                IconStore::LoadIconsCallback callback) {
        IconStore::LoadedIconsMap result;
        result.emplace(kSmallIconUuid, icons.at(IconType::kSmallIcon));
        result.emplace(kLargeIconUuid, icons.at(IconType::kLargeIcon));
        std::move(callback).Run(true, std::move(result));
      }));
  EXPECT_CALL(*notification_store(), Delete(kGuid, _));
  EXPECT_CALL(*icon_store(), DeleteIcons(_, _));

  auto expected_entry(entry);
  expected_entry.notification_data.icons = icons;
  DisplayNotification(kGuid, &expected_entry);
  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_TRUE(notifications.empty());
}

// Test to display a notification with icons loaded failed.
TEST_F(ScheduledNotificationManagerTest, DisplayNotificationWithIconsFailed) {
  auto entry = CreateNotificationEntry(SchedulerClientType::kTest1);
  entry.guid = kGuid;
  entry.icons_uuid[IconType::kLargeIcon] = kLargeIconUuid;
  entry.icons_uuid[IconType::kSmallIcon] = kSmallIconUuid;
  InitWithData(std::vector<NotificationEntry>({entry}));

  EXPECT_CALL(*icon_store(), LoadIcons(_, _))
      .WillOnce(Invoke([](std::vector<std::string> keys,
                          IconStore::LoadIconsCallback callback) {
        std::move(callback).Run(false, IconStore::LoadedIconsMap{});
      }));
  EXPECT_CALL(*notification_store(), Delete(kGuid, _));
  EXPECT_CALL(*icon_store(), DeleteIcons(_, _));
  DisplayNotification(kGuid, nullptr /*expected_entry*/);

  // Verify in-memory data.
  ScheduledNotificationManager::Notifications notifications;
  manager()->GetAllNotifications(&notifications);
  EXPECT_TRUE(notifications.empty());
}

}  // namespace
}  // namespace notifications
