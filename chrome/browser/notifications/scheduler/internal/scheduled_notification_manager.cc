// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"

#include <algorithm>
#include <map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/internal/icon_store.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace notifications {
namespace {

// Comparator used to sort notification entries based on creation time.
bool CreateTimeCompare(const NotificationEntry* lhs,
                       const NotificationEntry* rhs) {
  DCHECK(lhs && rhs);
  return lhs->create_time < rhs->create_time;
}

// Vailidates notification entry. Returns false if the entry should be deleted.
bool ValidateNotificationEntry(const NotificationEntry& entry) {
  // Check the deliver time window.
  return (entry.schedule_params.deliver_time_start.has_value() &&
          entry.schedule_params.deliver_time_end.has_value() &&
          entry.schedule_params.deliver_time_end > base::Time::Now() &&
          entry.schedule_params.deliver_time_end >=
              entry.schedule_params.deliver_time_start);
}

class ScheduledNotificationManagerImpl : public ScheduledNotificationManager {
 public:
  using NotificationStore = std::unique_ptr<CollectionStore<NotificationEntry>>;

  ScheduledNotificationManagerImpl(
      NotificationStore notification_store,
      std::unique_ptr<IconStore> icon_store,
      const std::vector<SchedulerClientType>& clients,
      const SchedulerConfig& config)
      : notification_store_(std::move(notification_store)),
        icon_store_(std::move(icon_store)),
        clients_(clients.begin(), clients.end()),
        config_(config) {}
  ScheduledNotificationManagerImpl(const ScheduledNotificationManagerImpl&) =
      delete;
  ScheduledNotificationManagerImpl& operator=(
      const ScheduledNotificationManagerImpl&) = delete;

 private:
  // NotificationManager implementation.
  void Init(InitCallback callback) override {
    icon_store_->InitAndLoadKeys(base::BindOnce(
        &ScheduledNotificationManagerImpl::OnIconStoreInitialized,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ScheduleNotification(
      std::unique_ptr<NotificationParams> notification_params,
      ScheduleCallback callback) override {
    DCHECK(notification_params);
    std::string guid = notification_params->guid;
    DCHECK(!guid.empty());
    auto type = notification_params->type;
    stats::LogNotificationLifeCycleEvent(
        stats::NotificationLifeCycleEvent::kScheduleRequest, type);

    if (!clients_.count(type) ||
        (notifications_.count(type) && notifications_[type].count(guid))) {
      // TODO(xingliu): Report duplicate guid failure.
      std::move(callback).Run(false);
      return;
    }

    bool valid = ValidateNotificationParams(*notification_params);
    DCHECK(valid) << "Invalid notification parameters.";
    if (!valid) {
      stats::LogNotificationLifeCycleEvent(
          stats::NotificationLifeCycleEvent::kInvalidInput, type);
      std::move(callback).Run(false);
      return;
    }

    if (notification_params->enable_ihnr_buttons) {
      CreateInhrButtonsPair(&notification_params->notification_data.buttons);
    }

    auto entry =
        std::make_unique<NotificationEntry>(notification_params->type, guid);
    auto icon_bundles = std::move(notification_params->notification_data.icons);
    entry->notification_data =
        std::move(notification_params->notification_data);
    entry->schedule_params = std::move(notification_params->schedule_params);
    icon_store_->AddIcons(
        std::move(icon_bundles),
        base::BindOnce(&ScheduledNotificationManagerImpl::OnIconsAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(entry),
                       std::move(callback)));
  }

  void DisplayNotification(const std::string& guid,
                           DisplayCallback callback) override {
    NotificationEntry* entry = nullptr;
    for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
      if (it->second.count(guid)) {
        entry = it->second[guid].get();
        break;
      }
    }

    if (!entry) {
      std::move(callback).Run(nullptr);
      return;
    }

    std::vector<std::string> keys;
    for (const auto& pair : entry->icons_uuid) {
      keys.emplace_back(pair.second);
    }
    icon_store_->LoadIcons(
        std::move(keys),
        base::BindOnce(&ScheduledNotificationManagerImpl::OnIconsLoaded,
                       weak_ptr_factory_.GetWeakPtr(), entry->type, entry->guid,
                       std::move(callback)));
  }

  void GetAllNotifications(Notifications* notifications) const override {
    DCHECK(notifications);
    notifications->clear();

    for (auto it = notifications_.begin(); it != notifications_.end(); it++) {
      auto type = it->first;
      for (const auto& pair : it->second) {
        (*notifications)[type].emplace_back(pair.second.get());
      }
    }

    // Sort by creation time for each notification type.
    for (auto it = notifications->begin(); it != notifications->end(); ++it) {
      std::sort(it->second.begin(), it->second.end(), &CreateTimeCompare);
    }
  }

  void GetNotifications(
      SchedulerClientType type,
      std::vector<const NotificationEntry*>* notifications) const override {
    DCHECK(notifications);
    notifications->clear();
    const auto it = notifications_.find(type);
    if (it == notifications_.end())
      return;
    for (const auto& pair : it->second)
      notifications->emplace_back(pair.second.get());
  }

  void DeleteNotifications(SchedulerClientType type) override {
    if (!notifications_.count(type))
      return;

    auto it = notifications_[type].begin();
    while (it != notifications_[type].end()) {
      const auto& entry = *it->second;
      ++it;
      DeleteNotification(entry, false /*should_delete_in_memory*/);
    }
    notifications_.erase(type);
  }

  void OnIconStoreInitialized(InitCallback callback,
                              bool success,
                              IconStore::LoadedIconKeys loaded_keys) {
    if (!success) {
      std::move(callback).Run(false);
      return;
    }

    notification_store_->InitAndLoad(base::BindOnce(
        &ScheduledNotificationManagerImpl::OnNotificationStoreInitialized,
        weak_ptr_factory_.GetWeakPtr(), std::move(callback),
        std::move(loaded_keys)));
  }

  void OnNotificationStoreInitialized(
      InitCallback callback,
      std::unique_ptr<std::vector<std::string>> loaded_icon_keys,
      bool success,
      CollectionStore<NotificationEntry>::Entries entries) {
    if (!success) {
      std::move(callback).Run(false);
      return;
    }

    FilterNotificationEntries(std::move(entries));
    FilterIconEntries(std::move(loaded_icon_keys));
    std::move(callback).Run(true);
  }

  void FilterIconEntries(
      std::unique_ptr<std::vector<std::string>> uuids_from_icon_store) {
    std::unordered_set<std::string> icons_uuid_from_entries;
    for (const auto& client_pair : notifications_) {
      for (const auto& notification : client_pair.second) {
        for (const auto& icon : notification.second->icons_uuid) {
          icons_uuid_from_entries.emplace(icon.second);
        }
      }
    }
    std::vector<std::string> icons_to_delete;
    for (const auto& loaded_icon_key : *uuids_from_icon_store.get()) {
      if (!base::Contains(icons_uuid_from_entries, loaded_icon_key)) {
        icons_to_delete.emplace_back(loaded_icon_key);
      }
    }
    icon_store_->DeleteIcons(icons_to_delete, /*callback=*/base::DoNothing());
  }

  // Filters and loads notification into memory.
  void FilterNotificationEntries(
      CollectionStore<NotificationEntry>::Entries entries) {
    for (auto it = entries.begin(); it != entries.end(); it++) {
      auto* entry = it->get();
      // Prune expired notifications. Also delete them in db.
      bool expired = entry->create_time + config_->notification_expiration <=
                     base::Time::Now();
      bool valid = ValidateNotificationEntry(*entry);
      bool deprecated_client = !base::Contains(clients_, entry->type);
      if (expired || deprecated_client || !valid) {
        DeleteNotification(*entry, false /*should_delete_in_memory*/);
      } else {
        notifications_[entry->type].emplace(entry->guid, std::move(*it));
      }
    }
  }

  void OnIconsAdded(std::unique_ptr<NotificationEntry> entry,
                    ScheduleCallback schedule_callback,
                    IconStore::IconTypeUuidMap icons_uuid_map,
                    bool success) {
    if (!success) {
      std::move(schedule_callback).Run(false);
      return;
    }

    entry->icons_uuid = std::move(icons_uuid_map);
    const auto* entry_ptr = entry.get();
    notification_store_->Add(
        entry_ptr->guid, *entry_ptr,
        base::BindOnce(&ScheduledNotificationManagerImpl::OnNotificationAdded,
                       weak_ptr_factory_.GetWeakPtr(), std::move(entry),
                       std::move(schedule_callback)));
  }

  void OnNotificationAdded(std::unique_ptr<NotificationEntry> entry,
                           ScheduleCallback schedule_callback,
                           bool success) {
    // Delete the icons when failed to add to notification database.
    if (!success) {
      std::vector<std::string> icons_to_delete;
      for (const auto& uuid : entry->icons_uuid) {
        icons_to_delete.emplace_back(uuid.second);
      }
      icon_store_->DeleteIcons(std::move(icons_to_delete),
                               /*callback=*/base::DoNothing());
      std::move(schedule_callback).Run(false);
      return;
    }

    auto type = entry->type;
    auto guid = entry->guid;
    notifications_[type][guid] = std::move(entry);

    stats::LogNotificationLifeCycleEvent(
        stats::NotificationLifeCycleEvent::kScheduled, type);
    std::move(schedule_callback).Run(true);
  }

  void OnIconsLoaded(SchedulerClientType client_type,
                     const std::string& guid,
                     DisplayCallback display_callback,
                     bool success,
                     IconStore::LoadedIconsMap loaded_icons_map) {
    auto* entry_ptr = FindNotificationEntry(client_type, guid);
    if (!entry_ptr) {
      std::move(display_callback).Run(nullptr);
      return;
    }

    if (!success) {
      DeleteNotification(*entry_ptr, true /*should_delete_in_memory*/);
      std::move(display_callback).Run(nullptr);
      return;
    }

    // Glue the icon data to entry.
    std::unique_ptr<NotificationEntry> entry =
        std::move(notifications_[client_type][guid]);
    DCHECK(entry);
    for (const auto& pair : entry->icons_uuid) {
      auto icon_bundle = IconBundle(std::move(loaded_icons_map[pair.second]));
      entry->notification_data.icons.emplace(pair.first,
                                             std::move(icon_bundle));
    }

    // Before moving out the entry, delete it from container and disk.
    DeleteNotification(*entry.get(), true /*should_delete_in_memory*/);

    std::move(display_callback).Run(std::move(entry));
  }

  NotificationEntry* FindNotificationEntry(SchedulerClientType type,
                                           const std::string& guid) {
    if (!notifications_.count(type) || !notifications_[type].count(guid))
      return nullptr;
    return notifications_[type][guid].get();
  }

  // Delete NotitificationEntry from memory and disk.
  void DeleteNotification(const NotificationEntry& entry,
                          bool should_delete_in_memory) {
    // Deletes icon first.
    std::vector<std::string> icons_to_delete;
    for (const auto& icon_id : entry.icons_uuid) {
      icons_to_delete.emplace_back(icon_id.second);
    }
    icon_store_->DeleteIcons(std::move(icons_to_delete),
                             /*callback=*/base::DoNothing());

    auto guid = entry.guid;
    auto type = entry.type;

    // Deletes notification entry.
    notification_store_->Delete(guid, /*callback=*/base::DoNothing());

    if (should_delete_in_memory) {
      notifications_[type].erase(guid);
      if (notifications_[type].empty())
        notifications_.erase(type);
    }
  }

  // Create two default buttons {Helpful, Unhelpful} for notification.
  void CreateInhrButtonsPair(std::vector<NotificationData::Button>* buttons) {
    buttons->clear();
    NotificationData::Button helpful_button;
    helpful_button.type = ActionButtonType::kHelpful;
    helpful_button.id = notifications::kDefaultHelpfulButtonId;
    helpful_button.text =
        l10n_util::GetStringUTF16(IDS_NOTIFICATION_DEFAULT_HELPFUL_BUTTON_TEXT);
    buttons->emplace_back(std::move(helpful_button));

    NotificationData::Button unhelpful_button;
    unhelpful_button.type = ActionButtonType::kUnhelpful;
    unhelpful_button.id = notifications::kDefaultUnhelpfulButtonId;
    unhelpful_button.text = l10n_util::GetStringUTF16(
        IDS_NOTIFICATION_DEFAULT_UNHELPFUL_BUTTON_TEXT);
    buttons->emplace_back(std::move(unhelpful_button));
  }

  // Vailidates notification parameters. Returns false if the parameters are
  // invalid.
  bool ValidateNotificationParams(const NotificationParams& params) {
    // Validate time window. Currently we only support deliver notification
    // according to a time window, the deliver window should before the
    // expiration duration of notification data in config.
    if (!params.schedule_params.deliver_time_start.has_value() ||
        !params.schedule_params.deliver_time_end.has_value() ||
        params.schedule_params.deliver_time_start.value() >
            params.schedule_params.deliver_time_end.value() ||
        params.schedule_params.deliver_time_end.value() - base::Time::Now() >=
            config_->notification_expiration) {
      return false;
    }

    // Validate ihnr buttons option. Custom buttons will be overwritten.
    if (params.enable_ihnr_buttons &&
        !params.notification_data.buttons.empty()) {
      return false;
    }

    // Validate icon bundle data is correct: icon resource id should never be
    // persisted to disk,since it can change in different versions. Client
    // should overwrite with Android resource id in BeforeShowNotification
    // callback if it is required.
    for (const auto& icon_bundle_map : params.notification_data.icons) {
      const auto& icon_bundle = icon_bundle_map.second;
      if (icon_bundle.resource_id)
        return false;
    }

    return true;
  }

  NotificationStore notification_store_;
  std::unique_ptr<IconStore> icon_store_;
  const std::unordered_set<SchedulerClientType> clients_;
  std::map<SchedulerClientType,
           std::map<std::string, std::unique_ptr<NotificationEntry>>>
      notifications_;
  const raw_ref<const SchedulerConfig, DanglingUntriaged> config_;
  base::WeakPtrFactory<ScheduledNotificationManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace

// static
std::unique_ptr<ScheduledNotificationManager>
ScheduledNotificationManager::Create(
    std::unique_ptr<CollectionStore<NotificationEntry>> notification_store,
    std::unique_ptr<IconStore> icon_store,
    const std::vector<SchedulerClientType>& clients,
    const SchedulerConfig& config) {
  return std::make_unique<ScheduledNotificationManagerImpl>(
      std::move(notification_store), std::move(icon_store), clients, config);
}

ScheduledNotificationManager::ScheduledNotificationManager() = default;

ScheduledNotificationManager::~ScheduledNotificationManager() = default;

}  // namespace notifications
